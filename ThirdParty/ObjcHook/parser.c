//
//  parser.c
//  ObjCMsgSendHook
//
//  Created by vic on 2019/12/23.
//  Copyright © 2019 vic. All rights reserved.
//

#include "parser.h"
#include <stdlib.h>
#include <string.h>
#include <arm/limits.h>
#include <objc/NSObjCRuntime.h>
#include <CoreGraphics/CGGeometry.h>
#include <CoreGraphics/CGAffineTransform.h>

#include "blocks.h"

static Class NSString_Class = NULL;
static Class NSMutalbeString_Class = NULL;
static Class NSData_Class = NULL;
static Class NSMutableData_Class = NULL;
static Class NSArray_Class = NULL;
static Class NSMutableArray_Class = NULL;
static Class NSDictionary_Class = NULL;
static Class NSMutableDicionary_Class = NULL;
static Class NSNumber_Class = NULL;
static Class NSValue_Class = NULL;
static Class NSBlock_Class = NULL;

static inline void logNSStringForStruct(FILE *file, char *str)
{
    flog(file, "%s", str);
}

static inline void logNSString(FILE *file, char *str)
{
    flog(file, "@\"%s\"", str);
}

static inline BOOL isKindOfClass(Class selfClass, Class clazz)
{
    for (Class candidate = selfClass; candidate; candidate = class_getSuperclass(candidate)) {
        if (candidate == clazz) {
            return YES;
        }
    }
    return NO;
}

char *objectDesc(id obj) {
    char * desc;
    if (obj == nil) {
        return strdup("");
    }
    Class kind = object_getClass(obj);
    if (NSString_Class == NULL) {
        NSString_Class = objc_getClass("NSString");
        NSMutalbeString_Class = objc_getClass("NSMutableString");
        NSData_Class = objc_getClass("NSData");
        NSMutableData_Class = objc_getClass("NSMutableData");
        NSArray_Class = objc_getClass("NSArray");
        NSMutableArray_Class = objc_getClass("NSMutableArray");
        NSDictionary_Class = objc_getClass("NSDictionary");
        NSMutableDicionary_Class = objc_getClass("NSMutableDictionary");
        NSNumber_Class = objc_getClass("NSNumber");
        NSValue_Class = objc_getClass("NSValue");
        NSBlock_Class = objc_getClass("NSBlock");
    }
    if (isKindOfClass(kind, NSBlock_Class)) {
        desc = blockDesc(obj);
    } else {
        const char *data;
        if (isKindOfClass(kind, NSString_Class)) {
            data = "NSString";
        } else if (isKindOfClass(kind, NSArray_Class)) {
            data = "NSArray";
        } else if (isKindOfClass(kind, NSDictionary_Class)) {
            data = "NSDictionary";
        } else if (isKindOfClass(kind, NSNumber_Class)) {
            data = "NSNumber";
        } else if (isKindOfClass(kind, NSValue_Class)) {
            data = "NSValue";
        } else if (isKindOfClass(kind, NSData_Class)) {
            data = "NSData";
        } else {
            data = class_getName(kind);;
        }
        desc = strdup(data);
    }
    return desc;
}

#ifndef __arm64__
static float float_from_va_list(va_list &args)
{
    union {
        uint32_t i;
        float f;
    } value = {va_arg(args, uint32_t)};
    return value.f;
}
#endif

typedef struct UIEdgeInsets {
    float top, left, bottom, right;  // specify amount to inset (positive) for each of the edges. values can be negative to 'outset'
} UIEdgeInsets;

typedef struct UIOffset {
    float horizontal, vertical; // specify amount to offset a position, positive for right or down, negative for left or up
} UIOffset;

typedef struct _NSRange {
    NSUInteger location;
    NSUInteger length;
} NSRange;

/*
typedef struct CGAffineTransform {
    float a, b, c, d;
    float tx, ty;
} CGAffineTransform;

typedef struct CGPoint {
    float x;
    float y;
}CGPoint;

typedef struct CGSize {
    float width;
    float height;
}CGSize;

typedef struct CGVector {
    float dx;
    float dy;
}CGVector;

typedef struct CGRect {
    CGPoint origin;
    CGSize size;
}CGRect;



#define _C_ID       '@'
#define _C_CLASS    '#'
#define _C_SEL      ':'

#define _C_CHR      'c'
#define _C_UCHR     'C'
#define _C_SHT      's'
#define _C_USHT     'S'
#define _C_INT      'i'
#define _C_UINT     'I'
#define _C_LNG      'l'
#define _C_ULNG     'L'
#define _C_LNG_LNG  'q'
#define _C_ULNG_LNG 'Q'
#define _C_FLT      'f'
#define _C_DBL      'd'

#define _C_BFLD     'b'
#define _C_BOOL     'B'
#define _C_VOID     'v'
#define _C_UNDEF    '?'
#define _C_PTR      '^'
#define _C_CHARPTR  '*'
#define _C_ATOM     '%'
#define _C_ARY_B    '['
#define _C_ARY_E    ']'
#define _C_UNION_B  '('
#define _C_UNION_E  ')'
#define _C_STRUCT_B '{'
#define _C_STRUCT_E '}'
#define _C_VECTOR   '!'
#define _C_CONST    'r'
*/
char * parseStruct(const char *type) {
    int startIdx = 0;
    if (*type == '{') {
        startIdx++;
    }
    while (*(type + startIdx) == '_') {
        startIdx++;
    }
    
    int endIdx = 0;
    while (*(type + endIdx) != '=') {
        endIdx++;
    }
    int length = endIdx - startIdx;
    char *name = calloc(1, length + 1);
    strncpy(name, (type + startIdx), length);
    return name;
}

char *parseArray(const char *type) {
    return strdup("");
}

char *parseUnion(const char *type) {
    int startIdx = 0;
    if (*type == '(') {
        startIdx++;
    }
    while (*(type + startIdx) == '_') {
        startIdx++;
    }
    
    int endIdx = 0;
    while (*(type + endIdx) != '=') {
        endIdx++;
    }
    int length = endIdx - startIdx;
    char *name = calloc(1, length + 1);
    strncpy(name, (type + startIdx), length);
    return name;
}

char *parseVector(const char *type) {
    return strdup("");
}

char * parsePointer(const char *type)
{
    switch (*type) {
        case _C_CHARPTR:  // A character string (char *).
            return strdup("char **");
        case _C_BOOL:  // A C++ bool or a C99 _Bool.
            return strdup("bool *");
        case _C_CHR:  // A char.
            return strdup("char *");
        case _C_UCHR:  // An unsigned char.
            return strdup("unsigned char *");
        case _C_SHT:  // A short.
            return strdup("short *");
        case _C_USHT:  // An unsigned short.
            return strdup("unsigned short *");
        case _C_INT:  // An int.
            return strdup("int *");
        case _C_UINT:  // An unsigned int.
            return strdup("unsigned int *");
        case _C_LNG:  // A long.
            return strdup("long *");
        case _C_ULNG:  // An unsigned long.
            return strdup("unsigned long *");
        case _C_LNG_LNG:  // A long long.
            return strdup("long long *");
        case _C_ULNG_LNG:  // An unsigned long long.
            return strdup("unsigned long long *");
        case _C_FLT:  // A float.
            return strdup("float *");
        case _C_DBL:  // A double.
            return strdup("double *");
        case _C_STRUCT_B: {// A struct. We check for some common structs.
            char *srcType = parseStruct(type);
            char *data = (char *)calloc(1, strlen(srcType) + 3);
            strncpy(data, srcType, strlen(srcType));
            strncat(data, " *", 2);
            free(srcType);
            return data;
        }
        case _C_UNION_B  : {//'('
            char *srcType = parseUnion(type);
            char *data = (char *)calloc(1, strlen(srcType) + 3);
            strncpy(data, srcType, strlen(srcType));
            strncat(data, " *", 2);
            free(srcType);
            return data;
        }
        case 'N': // inout.
        case 'n': // in.
        case 'O': // bycopy.
        case 'o': // out.
        case 'R': // byref.
        case _C_CONST: // const.
        case _C_ATOM://
        case 'V': // oneway.
        default: return strdup("");
    }
}


bool parseArgument(call_stack *cs, arg_list *args, char *type, unsigned int index)
{
    bool succ = NO;
retry:
    switch (*type) {
        case _C_CLASS    : // A class object (Class).
        {
            cs->params[index] = strdup("Class");
            id value = pa_arg(args, id);
            succ = true;
            break;
        }
        case _C_ID       : // An object (whether statically typed or typed id).
        {
            id value = pa_arg(args, id);
            cs->params[index] = objectDesc(value);
//            clog("%s|", cs->params[index]);
            succ = true;
            break;
        }
        case _C_SEL      : // A method selector (SEL).
        {
            SEL value = pa_arg(args, SEL);
            if (value == NULL) {
                cs->params[index] = strdup("NULL");
            } else {
                const char *data = sel_getName(value);
                cs->params[index] = strdup(data);
            }
//            clog("@selector(%s)|", cs->params[index]);
            succ = true;
            break;
        }
        case _C_CHR      : //'c'
        {
            cs->params[index] = strdup("char");
            signed char value = pa_arg(args, int_up_cast(char));
            succ = true;
            break;
        }
        case _C_UCHR     : //'C'
        {
            cs->params[index] = strdup("unsigned char");
            signed char value = pa_arg(args, int_up_cast(unsigned char));
            succ = true;
            break;
        }
        case _C_SHT      : //'s'
        {
            cs->params[index] = strdup("short");
            short value = pa_arg(args, int_up_cast(short));
            succ = true;
            break;
        }
        case _C_USHT     : //'S'
        {
            cs->params[index] = strdup("unsigned short");
            unsigned short value = pa_arg(args, uint_up_cast(unsigned short));
            succ = true;
            break;
        }
        case _C_INT      : //'i'
        {
            cs->params[index] = strdup("int");
            int value = pa_arg(args, int);
            succ = true;
            break;
        }
        case _C_UINT     : //'I'
        {
            cs->params[index] = strdup("unsigned int");
            unsigned int value = pa_arg(args, unsigned int);
            succ = true;
            break;
        }
        case _C_LNG      : //'l'
        {
            cs->params[index] = strdup("long");
            int value = pa_arg(args, int);
            succ = true;
            break;
        }
        case _C_ULNG     : //'L'
        {
            cs->params[index] = strdup("unsigned long");
            unsigned int value = pa_arg(args, unsigned int);
            succ = true;
            break;
        }
        case _C_LNG_LNG  : //'q'
        {
            cs->params[index] = strdup("long long");
            long long value = pa_arg(args, long long);
            succ = true;
            break;
        }
        case _C_ULNG_LNG : //'Q'
        {
            cs->params[index] = strdup("unsigned long long");
            unsigned long long value = pa_arg(args, unsigned long long);
            succ = true;
            break;
        }
        case _C_FLT      : //'f'
        {
            cs->params[index] = strdup("float");
            float value = pa_float(args);
            succ = true;
            break;
        }
        case _C_DBL      : //'d'
        {
            cs->params[index] = strdup("double");
            double value = pa_double(args);
            succ = true;
            break;
        }
        case _C_BFLD     : //'b'
            cs->params[index] = strdup("bit_field");
            break;
        case _C_BOOL     : //'B'
        {
            cs->params[index] = strdup("bool");
            bool value = pa_arg(args, int_up_cast(bool));
            succ = true;
            break;
        }
        case _C_VOID     : //'v'
            cs->params[index] = strdup("void");
            succ = true;
            break;
        case _C_PTR      : //'^'
        {
            char *realType = type + 1;
            cs->params[index] = parsePointer(realType);
            void *value = pa_arg(args, void *);
            succ = true;
            break;
        }
        case _C_CHARPTR  : //'*'
        {
            cs->params[index] = strdup("char *");
            const char *value = pa_arg(args, const char *);
            succ = true;
            break;
        }
        case _C_STRUCT_B : //'{'
            cs->params[index] = parseStruct(type);
            if (strncmp(type, "{CGAffineTransform=", 19) == 0) {
                CGAffineTransform *ptr = (CGAffineTransform *)pa_arg(args, void *);
            } else if (strncmp(type, "{CGPoint=", 9) == 0) {
                pa_two_doubles(args, CGPoint, point)
            } else if (strncmp(type, "{CGRect=", 8) == 0) {
                pa_four_doubles(args, UIEdgeInsets, insets)
                CGRect rect = CGRectMake(insets.top, insets.left, insets.bottom, insets.right);
            } else if (strncmp(type, "{CGSize=", 8) == 0) {
                pa_two_doubles(args, CGSize, size)
            }  else if (strncmp(type, "{UIEdgeInsets=", 14) == 0) {
                pa_four_doubles(args, UIEdgeInsets, insets)
            } else if (strncmp(type, "{UIOffset=", 10) == 0) {
                pa_two_doubles(args, UIOffset, offset)
            } else if (strncmp(type, "{_NSRange=", 10) == 0) {
                pa_two_ints(args, NSRange, range, unsigned long);
            }
        case _C_STRUCT_E : //'}'
            break;
        case _C_ARY_B    : //'['
            cs->params[index] = parseArray(type);
        case _C_ARY_E    : //']'
            break;
        case _C_UNION_B  : //'('
            cs->params[index] = parseUnion(type);
        case _C_UNION_E  : //')'
            break;
        case _C_VECTOR   : //'!'
            cs->params[index] = parseVector(type);
            break;
        case _C_UNDEF    : //'?'
        case _C_CONST: // const.
        case _C_ATOM://
        case 'N': // inout.
        case 'n': // in.
        case 'O': // bycopy.
        case 'o': // out.
        case 'R': // byref.
        case 'V': // oneway.
            ++type;
            goto retry;
            
        default:
            break;
    }
    return succ;
}

bool parseRetType(call_stack *cs, uintptr_t ret, char *type)
{
retry:
    switch (*type) {
        case _C_CLASS: // A class object (Class).
            if (cs->ret_type != NULL) {
                free(cs->ret_type);
            }
            cs->ret_type = strdup("Class");
            break;
        case _C_ID: { // An object (whether statically typed or typed id).
            id value = (id)(ret);
            if (cs->ret_type != NULL) {
                free(cs->ret_type);
            }
            cs->ret_type = objectDesc(value);
//            clog("%s|", cs->ret_type);
            break;
        }
        case _C_SEL: { // A method selector (SEL).
            cs->ret_type = strdup("SEL");
            break;
        }
        case 'N': // inout.
        case 'n': // in.
        case 'O': // bycopy.
        case 'o': // out.
        case 'R': // byref.
        case 'V': // oneway.
        case _C_CONST: // const.
        case _C_ATOM://
            ++type;
            goto retry;
            
        default:
            break;
    }
    return true;
}
