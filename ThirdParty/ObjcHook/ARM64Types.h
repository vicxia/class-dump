//
//  ARM64Types.h
//  ObjCMsgSendHook
//
//  Created by vic on 2019/11/15.
//  Copyright © 2019 vic. All rights reserved.
//

#ifndef ARM64Types_h
#define ARM64Types_h

#include <objc/runtime.h>

// ARM64 defines.
#define alignof(t) __alignof__(t)

// TODO(DavidGoldman): Treat the regs as a pointer directly to avoid casting? i.e.:
// (*(t *)(&args->regs->general.arr[args->ngrn++]))
// ARM64 defines.
#define alignof(t) __alignof__(t)

// TODO(DavidGoldman): Treat the regs as a pointer directly to avoid casting? i.e.:
// (*(t *)(&args->regs->general.arr[args->ngrn++]))
#define pa_arg(args, t) \
  ( (args->ngrn < 8) ? ((t)(args->regs->general.arr[args->ngrn++])) : \
        pa_stack_arg(args, t) \
    )

#define pa_float(args) \
  ( (args->nsrn < 8) ? args->regs->floating.arr[args->nsrn++].f.f1 : \
        pa_stack_arg(args, float) \
    )

#define pa_double(args) \
  ( (args->nsrn < 8) ? args->regs->floating.arr[args->nsrn++].d.d1 : \
        pa_stack_arg(args, double) \
    )

// We need to align the sp - we do so via sp = ((sp + alignment - 1) & -alignment).
// Then we increment the sp by the size of the argument and return the argument.
#define pa_stack_arg(args, t) \
  (*(t *)( (args->stack = (unsigned char *)( ((uintptr_t)args->stack + (alignof(t) - 1)) & -alignof(t)) + sizeof(t)) - sizeof(t) ))

#define pa_two_ints(args, varType, varName, intType) \
  varType varName; \
  if (args->ngrn < 7) { \
    intType a = (intType)args->regs->general.arr[args->ngrn++]; \
    intType b = (intType)args->regs->general.arr[args->ngrn++]; \
    varName = (varType) { a, b }; \
  } else { \
    args->ngrn = 8; \
    intType a = pa_stack_arg(args, intType); \
    intType b = pa_stack_arg(args, intType); \
    varName = (varType) { a, b }; \
  } \

#define pa_two_doubles(args, t, varName) \
  t varName; \
  if (args->nsrn < 7) { \
    double a = args->regs->floating.arr[args->nsrn++].d.d1; \
    double b = args->regs->floating.arr[args->nsrn++].d.d1; \
    varName = (t) { a, b }; \
  } else { \
    args->nsrn = 8; \
    double a = pa_stack_arg(args, double); \
    double b = pa_stack_arg(args, double); \
    varName = (t) { a, b }; \
  } \

#define pa_four_doubles(args, t, varName) \
  t varName; \
  if (args->nsrn < 5) { \
    double a = args->regs->floating.arr[args->nsrn++].d.d1; \
    double b = args->regs->floating.arr[args->nsrn++].d.d1; \
    double c = args->regs->floating.arr[args->nsrn++].d.d1; \
    double d = args->regs->floating.arr[args->nsrn++].d.d1; \
    varName = (t) { a, b, c, d }; \
  } else { \
    args->nsrn = 8; \
    double a = pa_stack_arg(args, double); \
    double b = pa_stack_arg(args, double); \
    double c = pa_stack_arg(args, double); \
    double d = pa_stack_arg(args, double); \
    varName = (t) { a, b, c, d }; \
  } \

typedef union FPReg_ {
    __int128_t q;
    struct {
        double d1; // Holds the double (LSB).
        double d2;
    } d;
    struct {
        float f1; // Holds the float (LSB).
        float f2;
        float f3;
        float f4;
    } f;
} FPReg;

#ifndef _UINT64_T
#define _UINT64_T
typedef unsigned long long uint64_t;
#endif /* _UINT64_T */

typedef struct RegState_ {
    union {
        uint64_t arr[10];
        struct {
            uint64_t x0;
            uint64_t x1;
            uint64_t x2;
            uint64_t x3;
            uint64_t x4;
            uint64_t x5;
            uint64_t x6;
            uint64_t x7;
            uint64_t x8;
            uint64_t lr;
        } regs;
    } general;
    
    union {
        FPReg arr[8];
        struct {
            FPReg q0;
            FPReg q1;
            FPReg q2;
            FPReg q3;
            FPReg q4;
            FPReg q5;
            FPReg q6;
            FPReg q7;
        } regs;
    } floating;
}RegState;

typedef struct pa_list_ {
    RegState *regs; // Registers saved when function is called.
    unsigned char *stack; // Address of current argument.
    int ngrn; // The Next General-purpose Register Number.
    int nsrn; // The Next SIMD and Floating-point Register Number.
} pa_list;


typedef struct {
    uintptr_t lr;
    SEL _cmd;
    //是否要追踪
    bool is_tracking;
    //唯一标示
    char *key;
    // 类名
    char *class_name;
    // 函数名
    char *method_name;
    // 是否是类方法
    bool is_class_method;
    //返回值类型
    char *ret_type;
    //参数个数
    uint8_t param_count;
    //参数类型的二维数组
    char **params;
}call_stack;

typedef struct {
    /// 调用栈
    call_stack *cs;
    /// 调用栈第一个: 每一个objc_msgSend开始
    call_stack *first;
    uint index;
    /// 当前数据总大小
    uint size;
    /// 总开销
    uint64_t cost;
    /// 是否是主线程
    bool is_main_thread;
    // 调用栈信息
    char *stack_info;
    uint64_t stack_info_size;
}common_data;

//extern char *directory;
//extern char *dbPath;


#define kEnableFileLog 1
#define kEnableConsoleLog 1

#if kEnableFileLog
#define flog(file, format,...) \
fprintf(file, format, ##__VA_ARGS__);\
printf(format "|", ##__VA_ARGS__)
#else
#define flog(file, format,...)
#endif

#if kEnableConsoleLog
#define clog(format,...) printf("FILE: " __FILE__ ", LINE: %d: " format "\n", __LINE__, ##__VA_ARGS__)
#else
#define clog(format,...)
#endif


#endif /* ARM64Types_h */
