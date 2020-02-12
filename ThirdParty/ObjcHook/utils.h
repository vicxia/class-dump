//
//  utils.h
//  ObjCMsgSendHook
//
//  Created by vic on 2019/11/15.
//  Copyright © 2019 vic. All rights reserved.
//

#ifndef utils_h
#define utils_h

#include <stdio.h>
#include <objc/runtime.h>

/// 返回对象的类名
/// 获取类名
const char *hook_getObjectClassName(void *_target);
/// 获取当前系统时间
uint64_t hook_getMillisecond(void);
/// 判断target是否包含有prefix字符串
bool hook_has_prefix(const char *target, const char *prefix);

#endif /* utils_h */
