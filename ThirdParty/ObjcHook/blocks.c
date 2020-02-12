//
//  blocks.c
//  ObjCMsgSendHook
//
//  Created by vic on 2019/11/15.
//  Copyright © 2019 vic. All rights reserved.
//

#include "blocks.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <objc/runtime.h>

// Thanks to CTObjectiveCRuntimeAdditions (https://github.com/ebf/CTObjectiveCRuntimeAdditions).
// See http://clang.llvm.org/docs/Block-ABI-Apple.html.
char * blockDesc(id block)
{
    struct BlockLiteral_ *blockRef = (struct BlockLiteral_ *)block;
    int flags = blockRef->flags;
    
    const char *signature = NULL;
    
    if (flags & BLOCK_HAS_SIGNATURE) {
        unsigned char *signatureLocation = (unsigned char *)blockRef->descriptor;
        signatureLocation += sizeof(unsigned long int);
        signatureLocation += sizeof(unsigned long int);
        
        if (flags & BLOCK_HAS_COPY_DISPOSE) {
            signatureLocation += sizeof(void (*)(void *, void *));
            signatureLocation += sizeof(void (*)(void *));
        }
        
        signature = (*(const char **)signatureLocation);
    }
    const char *data = NULL;
    if (signature) {
        data = signature;
    } else {
        Class kind = object_getClass(block);
        data = class_getName(kind);
    }
    size_t length = strlen(data);
    char *desc = (char *)calloc(length + 7, sizeof(char));
    strncpy(desc, "block|", 6);
    strncat(desc, data, length);
    return desc;
}
