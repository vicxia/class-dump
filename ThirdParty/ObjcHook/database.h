//
//  database.h
//  ObjCMsgSendHook
//
//  Created by vic on 2019/12/20.
//  Copyright © 2019 vic. All rights reserved.
//

#ifndef database_h
#define database_h

#include <stdio.h>
#include "CoreFoundation/CoreFoundation.h"
#include "ARM64Types.h"

bool dbOpen(char *path);
bool dbFlush(void);
bool dbClose(void);
bool dbClearAllData(void);

bool dbInsertCallstack(call_stack *cs);

CFDictionaryRef dbGetCallStacksWithClass(const char *clazz);



#endif /* database_h */
