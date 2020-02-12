//
//  utils.c
//  ObjCMsgSendHook
//
//  Created by vic on 2019/11/15.
//  Copyright © 2019 vic. All rights reserved.
//

#include "utils.h"
#include <sys/time.h>
#include <string.h>

const char *hook_getObjectClassName(void *_target) {
    return class_getName(object_getClass(_target));
}

/// 获取当前系统时间
uint64_t hook_getMillisecond() {
    struct timeval tv;
    gettimeofday(&tv,NULL);
    return tv.tv_usec / 1000 + tv.tv_sec * 1000;
}

bool hook_has_prefix(const char *target, const char *prefix) {
    uint64_t count = strlen(prefix);
    bool ret = true;
    if(strlen(target) < count)
        return false;
    for (int i = 0; i < count; i ++) {
        if (target[i] != prefix[i]) {
            ret = false;
            break;
        }
    }
    return ret;
}
