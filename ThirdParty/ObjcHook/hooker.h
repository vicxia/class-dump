//
//  hooker.h
//  ObjCMsgSendHook
//
//  Created by vic on 2019/11/15.
//  Copyright © 2019 vic. All rights reserved.
//

#ifndef hooker_h
#define hooker_h

#include <stdio.h>
#include <objc/runtime.h>
#include "ARM64Types.h"

///hook_core 实现了hook的方法统计,具体的hook在不同的CPU下面完成
///intel x64在llvm 2.0之后不再支持嵌入式asm代码
/// 前
void before_objc_msgSend(id object, SEL _cmd, uintptr_t lr, RegState *rs);
/// 开始hook
void start_objc_msgSend_hook(void);
/// 后
uintptr_t after_objc_msgSend(uintptr_t ret);

#endif /* hooker_h */
