//
//  hooker.c
//  ObjCMsgSendHook
//
//  Created by vic on 2019/11/15.
//  Copyright © 2019 vic. All rights reserved.
//

#include "hooker.h"
#include "utils.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <objc/runtime.h>
#include <dispatch/dispatch.h>
#include <dispatch/queue.h>
#include "fishhook.h"
#include <sys/_types/_uintptr_t.h>
#include "parser.h"
#include "CoreFoundation/CoreFoundation.h"
#include "database.h"
#import <libkern/OSAtomic.h>
//#import <os/lock.h>

//宏定义布尔类型
#define BOOL int
#define TRUE 1
#define FALSE 0

#define MAIN_THREAD_ONLY 0

#define MAX_PATH_LENGTH 1024

#define DEFAULT_CALLSTACK_DEPTH 128
#define CALLSTACK_DEPTH_INCREMENT 64

#define DEFAULT_MAX_RELATIVE_RECURSIVE_DESCENT_DEPTH 64

#if MAIN_THREAD_ONLY
#define LOCK
#define UNLOCK
#else
OSSpinLock spinLock = OS_SPINLOCK_INIT;
#define LOCK    OSSpinLockLock(&spinLock);
#define UNLOCK  OSSpinLockUnlock(&spinLock);
#endif



#define WATCH_ALL_SELECTORS_SELECTOR NULL

//#if __arm64__
//#define arg_list pa_list
//#else
//#define arg_list va_list
//#endif

CFMutableSetRef skipMethodSet;

#if MAIN_THREAD_ONLY
static void performBlockOnProperThread(void (^block)(void)) {
    if (pthread_main_np()) {
        block();
    } else {
        dispatch_async(dispatch_get_main_queue(), block);
    }
}
#else
static void performBlockOnProperThread(void (^block)(void)) {
    LOCK;
    block();
    UNLOCK;
}
#endif

// The original objc_msgSend.
static id (*orig_objc_msgSend)(id, SEL, ...) = NULL;

char *directory;
char *dbPath;

// Max callstack depth to log after the last hit.
static int maxRelativeRecursiveDepth = DEFAULT_MAX_RELATIVE_RECURSIVE_DESCENT_DEPTH;

// Shared functions.

static void checkDirectory() {
    char path[MAX_PATH_LENGTH];
    
    sprintf(path, "%s/ObjCHook", directory);
    mkdir(path, 0755);
    sprintf(path, "%s/ObjCHook/database", directory);
    mkdir(path, 0755);
    sprintf(path, "%s/ObjCHook/database/method.db", directory);
    dbPath = strdup(path);
}

static pthread_key_t _thread_key;

///获取线程共享数据
common_data *get_thread_call_stack() {
    /// pthread_getspecific 获取指定key的同一线程中不同方法的共享数据
    common_data *_cd = (common_data *)pthread_getspecific(_thread_key);
    if (_cd == NULL) {
        _cd = (common_data *)calloc(1, sizeof(common_data));
        _cd->size = 63;
        //调用栈
        _cd->cs = (call_stack *)calloc(_cd->size, sizeof(call_stack));
        //当前调用栈索引
        _cd->index = 0;
        _cd->cost = 0;
        _cd->stack_info = NULL;
        //存放共享数据
        pthread_setspecific(_thread_key, (void *)_cd);
    }
    _cd->is_main_thread = pthread_main_np();
    return _cd;
}
/// 如果调用栈大小不够,重新分配空间
void realloc_call_stack_memery_ifneed() {
    common_data *cd = get_thread_call_stack();
    /// 重新分配空间
    if (cd->index >= cd->size) {
        cd->size += 30;
        cd->cs = (call_stack *)realloc(cd->cs, sizeof(call_stack) * cd->size);
        printf("重新分配调用栈空间， size: %d", cd->size);
    }
}

static inline bool isSystemClass(char *className, Class clazz){
#if 0
    return (strncmp(className, "AW", 2) != 0);
#else
    if (strncmp(className, "OS", 2) == 0
        ||strncmp(className, "UI", 2) == 0
        ||strncmp(className, "AV", 2) == 0
        ||strncmp(className, "NS", 2) == 0
        ||strncmp(className, "CF", 2) == 0
        ||strncmp(className, "FM", 2) == 0
        ||strncmp(className, "YY", 2) == 0
        ||strncmp(className, "CA", 2) == 0
        ||strncmp(className, "MT", 2) == 0
        ||strncmp(className, "_", 1) == 0
        ) {
        return true;
    }
    const char *imgName = class_getImageName(clazz);
    if(imgName != NULL && hook_has_prefix(imgName, "/private/var/")) {
        return true;
    }

    return false;
#endif
}

static inline BOOL isKindOfClass(Class selfClass, Class clazz) {
    for (Class candidate = selfClass; candidate; candidate = class_getSuperclass(candidate)) {
        if (candidate == clazz) {
            return YES;
        }
    }
    return NO;
}

static inline void parseArgs(call_stack *cs, id obj, SEL _cmd, arg_list *args, Class clazz) {
    Method method = cs->is_class_method ? class_getClassMethod(clazz, _cmd) : class_getInstanceMethod(clazz, _cmd);
    if (method) {
        const char *typeEncoding = method_getTypeEncoding(method);
        if (!typeEncoding) {
            cs->is_tracking = false;
            return;
        }
        
        const unsigned int numberOfArguments = method_getNumberOfArguments(method);
        char *retType = method_copyReturnType(method);
        cs->ret_type = strdup(retType);
        free(retType);
        cs->param_count = numberOfArguments;
        cs->params = (char **)calloc(numberOfArguments, sizeof(char *));
        cs->params[0] = NULL;
        cs->params[1] = NULL;
        for (unsigned int index = 2; index < numberOfArguments; ++index) {
            char *type = method_copyArgumentType(method, index);
            parseArgument(cs, args, type, index);
            if (type) {
                free(type);
            }
        }
    } else {
        cs->is_tracking = false;
    }
}

/// hook 之前
void before_objc_msgSend(id object, SEL _cmd, uintptr_t lr, RegState *rs) {
    realloc_call_stack_memery_ifneed();
    common_data *cd = get_thread_call_stack();
    // 保存当前调用栈下一个函数方法执行地址
    call_stack *cs =  &(cd->cs[cd->index]);
    memset(cs, 0, sizeof(call_stack));
    // 正常执行程序的下一条指令,保存起来在hook完之后需要恢复到hook前的状态,程序lr寄存器能正常执行
    cs->lr = lr;
    // 保存执行的SEL
    cs->_cmd = _cmd;
    // 保存当前对象的类名
    cs->class_name = strdup((char *)hook_getObjectClassName((void *)(object)));
    
    // 保存当前调用的方法名
    cs->method_name = strdup((char *)cs->_cmd);

    // 判断是否是元类的实例方法,即类的类方法
    Class class = object_getClass(object);
    cs->is_class_method = class_isMetaClass(class);
    // index为0表示调用栈顶,即objc_msgSend初始调用(objc_msgSend内部还会调用其它objc_msgSend)
    if(cd->index == 0) {
        cd->first = cs;
        cd->stack_info_size = 1024;
        if(cd->stack_info) free(cd->stack_info);
        cd->stack_info = (char *)calloc(1, 1024);
    }
    
    cs->ret_type = NULL;
    cs->params = NULL;
    cs->param_count = 0;
    
    // 入栈
    // 调用栈前进
    cd->index ++;
    
    //printf("img: %s \n", imgName);
    
#if MAIN_THREAD_ONLY
    if (cd->is_main_thread && !isSystemClass(cs->class_name, class)) {
#else
    if (!isSystemClass(cs->class_name, class)) {
#endif
//        CFStringRef methodName = CFStringCreateWithBytesNoCopy(NULL, (UInt8 *)cs->method_name, strlen(cs->method_name), kCFStringEncodingUTF8, false, NULL);
        CFStringRef methodName = CFStringCreateWithBytes(NULL, (UInt8 *)cs->method_name, strlen(cs->method_name), kCFStringEncodingUTF8, false);
        if (!CFSetContainsValue(skipMethodSet, methodName)) {
            cs->is_tracking = YES;
            pa_list args = (pa_list){ rs, ((unsigned char *)rs) + 208, 2, 0 }; // 208 is the offset of rs from the top of the stack.
            
            parseArgs(cs, object, _cmd, &args, class);
        }
        CFRelease(methodName);
    }
}


#if __aarch64__
//static id (*orig_objc_msgSend)(void);
// 将参数value(地址)传递给x12寄存器
// blr开始执行
#if 0

#define call(b, value) \
__asm volatile ("mov x12, %0\n" :: "r"(value)); \
__asm volatile (#b " x12\n");

#else

#define call(b, value) \
__asm volatile ("stp x8, x9, [sp, #-16]!\n"); \
__asm volatile ("mov x12, %0\n" :: "r"(value)); \
__asm volatile ("ldp x8, x9, [sp], #16\n"); \
__asm volatile (#b " x12\n");

#endif

/// 依次将寄存器数据入栈
#define save() \
__asm volatile ( \
"stp q6, q7, [sp, #-32]!\n" \
"stp q4, q5, [sp, #-32]!\n" \
"stp q2, q3, [sp, #-32]!\n" \
"stp q0, q1, [sp, #-32]!\n" \
"stp x8, x9, [sp, #-16]!\n" \
"stp x6, x7, [sp, #-16]!\n" \
"stp x4, x5, [sp, #-16]!\n" \
"stp x2, x3, [sp, #-16]!\n" \
"stp x0, x1, [sp, #-16]!\n" );

/// 依次将寄存器数据出栈
#define load() \
__asm volatile ( \
"ldp x0, x1, [sp], #16\n" \
"ldp x2, x3, [sp], #16\n" \
"ldp x4, x5, [sp], #16\n" \
"ldp x6, x7, [sp], #16\n" \
"ldp x8, x9, [sp], #16\n" \
"ldp q0, q1, [sp], #32\n" \
"ldp q2, q3, [sp], #32\n" \
"ldp q4, q5, [sp], #32\n" \
"ldp q6, q7, [sp], #32\n" );

/// 程序执行完成,返回将继续执行lr中的函数
#define ret() __asm volatile ("ret\n");

static void hook_objc_msgSend() {
    /// before之前保存objc_msgSend的参数
    save()
    /// 将objc_msgSend执行的下一个函数地址传递给before_objc_msgSend的第二个参数x0 self, x1 _cmd, x2: lr address
    // Swap args around for call.
    //    void before_objc_msgSend(id object, SEL _cmd, uintptr_t lr, RegState *rs)
    __asm volatile ("mov x2, lr\n"
                    "mov x3, sp\n");
    /// 执行before_objc_msgSend
    call(blr, &before_objc_msgSend)
    /// 恢复objc_msgSend参数，并执行
    load()
    call(blr, orig_objc_msgSend)
    /// after之前保存objc_msgSend执行完成的参数
    save()
    /// 调用 after_objc_msgSend
    call(blr, &after_objc_msgSend)
    /// 将after_objc_msgSend返回的参数放入lr,恢复调用before_objc_msgSend前的lr地址
    __asm volatile ("mov lr, x0\n");
    /// 恢复objc_msgSend执行完成的参数
    load()
    /// 方法结束,继续执行lr
    ret()
}

#endif

void freeCallstack(call_stack *cs) {
    if (cs->method_name != NULL) {
        free(cs->method_name);
        cs->method_name = NULL;
    }
    if (cs->class_name != NULL) {
        free(cs->class_name);
        cs->class_name = NULL;
    }
    if (cs->is_tracking) {
        if (cs->ret_type != NULL) {
            free(cs->ret_type);
            cs->ret_type = NULL;
        }
        if (cs->param_count > 2) {
            for (int i = 2; i < cs->param_count; i++) {
                char *param = cs->params[i];
                if (param != NULL) {
                    free(param);
                }
            }
            cs->param_count = 0;
            free(cs->params);
            cs->params = NULL;
        }
    }
}

/// hook 之后
uintptr_t after_objc_msgSend(uintptr_t ret) {
    common_data *_cd = (common_data *)pthread_getspecific(_thread_key);
    /// 后退
    _cd->index --;
    /// 获取即将完成的调用
    call_stack *cs = &(_cd->cs[_cd->index]);
    
    if (cs->is_tracking) {
        if (cs->ret_type[0] == _C_ID) {
            parseRetType(cs, ret, cs->ret_type);
        }
    }
    if (cs->is_tracking) {
        LOCK;
        dbInsertCallstack(cs);
        UNLOCK;
    }
    //释放stack
    freeCallstack(cs);
    // 将下一条函数指令返回,并存放到寄存器x0
    return cs->lr;
}
/// 释放与线程共享数据的相关资源
void release_thread_stack() {
    common_data *_cd = (common_data *)pthread_getspecific(_thread_key);
    if(!_cd) return;
    if (_cd->cs) free(_cd->cs);
    if (_cd->stack_info) free(_cd->stack_info);
    free(_cd);
}

static void printKeys (const void* key, const void* value, void* context) {
    printf("key: %s \n", (char *)key);
    call_stack *cs = (call_stack *)value;
    printf("value: %s, %s, %s", cs->class_name, cs->method_name, cs->ret_type);
    for (int i = 2; i < cs->param_count; i++) {
        printf(", %s", cs->params[i]);
    }
    printf("\n");
    free(key);
    freeCallstack(cs);
    free(cs);
}

void start_objc_msgSend_hook() {
    printf("hook_start\n");
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        pthread_key_create(&_thread_key, &release_thread_stack);
        
        
#ifdef MAIN_THREAD_ONLY
        printf("[ObjCHook] Loading - Directory is \"%s\"", directory);
#else
        printf("[ObjCHook] Multithreaded; Loading - Directory is \"%s\"", directory);
#endif
        
        skipMethodSet = CFSetCreateMutable(NULL, 0, &kCFCopyStringSetCallBacks);
        CFSetAddValue(skipMethodSet, CFSTR("new"));
        CFSetAddValue(skipMethodSet, CFSTR("alloc"));
        CFSetAddValue(skipMethodSet, CFSTR("init"));
        CFSetAddValue(skipMethodSet, CFSTR("class"));
        CFSetAddValue(skipMethodSet, CFSTR("dealloc"));
        CFSetAddValue(skipMethodSet, CFSTR("retain"));
        CFSetAddValue(skipMethodSet, CFSTR("release"));
        CFSetAddValue(skipMethodSet, CFSTR("frame"));
        CFSetAddValue(skipMethodSet, CFSTR("bounds"));
        CFSetAddValue(skipMethodSet, CFSTR("center"));
        CFSetAddValue(skipMethodSet, CFSTR("performSelector:withObject:"));

        checkDirectory();
        dbOpen(dbPath);

        /// 替换系统的函数: 因为apple使用了系统函数动态绑定技术,所以可以hook系统函数,本地函数编译时地址已固定
        /// rebinding: 第一个参数,需要hook的系统函数名称
        ///            第二个参数,替换系统的函数
        ///            第三个参数,指向被hook函数,调用orig_objc_msgSend相当于调用原始函数
        rebind_symbols((struct rebinding[1]){{"objc_msgSend", (void *)hook_objc_msgSend, (void **)&orig_objc_msgSend}}, 1);
    });
}
