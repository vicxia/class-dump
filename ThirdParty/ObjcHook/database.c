//
//  database.c
//  ObjCMsgSendHook
//
//  Created by vic on 2019/12/20.
//  Copyright © 2019 vic. All rights reserved.
//

#include "database.h"
#include "sqlite3.h"

#define kMaxErrorRetryCount (8)
#define kMinRetryTimeInterval (2.0)

sqlite3 *_db;
CFMutableDictionaryRef _dbStmtCache;
CFMutableSetRef _dbInsertedKeys;
CFTimeInterval _dbLastOpenErrorTime;
int _dbOpenErrorCount;
bool _errorLogsEnabled;

bool dbCreate(char *path);
bool dbClose(void);
bool dbInitialize(void);
bool dbExecute(const char *sql);
bool dbCheck(void);
void dbCheckpoint(void);
sqlite3_stmt * dbPrepareStmt(const char *sql);

bool dbOpen(char *path)
{
    if (!dbCreate(path) || !dbInitialize()) {
        dbClose();
        clog("database init error: fail to open sqlite db.");
        return false;
    }
    return true;
}

bool dbCreate(char *path) {
    if (_db) { return true; }
    
    int result = sqlite3_open(path, &_db);
    if (result == SQLITE_OK) {
        CFDictionaryKeyCallBacks keyCallbacks = kCFCopyStringDictionaryKeyCallBacks;
        CFDictionaryValueCallBacks valueCallbacks = {0};
        _dbStmtCache = CFDictionaryCreateMutable(NULL, 0, &keyCallbacks, &valueCallbacks);
        
        _dbInsertedKeys = CFSetCreateMutable(NULL, 0, &kCFTypeSetCallBacks);
        
        _dbLastOpenErrorTime = 0;
        _dbOpenErrorCount = 0;
        return true;
    } else {
        const char *errorMsg = sqlite3_errmsg(_db);
        printf("dbOpen failed; %s", errorMsg);
        _db = NULL;
        if (_dbStmtCache) CFRelease(_dbStmtCache);
        _dbLastOpenErrorTime = CFAbsoluteTimeGetCurrent();
        _dbOpenErrorCount++;
        return false;
    }
}

bool dbFlush(void)
{
    if (!_db) { return true; }
    int result = sqlite3_db_cacheflush(_db);
    return result == SQLITE_OK;
}

bool dbClose(void) {
    if (!_db) { return true; }
    
    int  result = 0;
    bool retry = false;
    bool stmtFinalized = false;
    
    if (_dbStmtCache) CFRelease(_dbStmtCache);
    _dbStmtCache = NULL;
    
    do {
        retry = false;
        result = sqlite3_close(_db);
        if (result == SQLITE_BUSY || result == SQLITE_LOCKED) {
            if (!stmtFinalized) {
                stmtFinalized = true;
                sqlite3_stmt *stmt;
                while ((stmt = sqlite3_next_stmt(_db, nil)) != 0) {
                    sqlite3_finalize(stmt);
                    retry = true;
                }
            }
        } else if (result != SQLITE_OK) {
            if (_errorLogsEnabled) {
                clog("%s line:%d sqlite close failed (%d).", __FUNCTION__, __LINE__, result);
            }
        }
    } while (retry);
    _db = NULL;
    return true;
}

bool dbCheck(void) {
    if (!_db) {
        if (_dbOpenErrorCount < kMaxErrorRetryCount &&
            CFAbsoluteTimeGetCurrent() - _dbLastOpenErrorTime > kMinRetryTimeInterval) {
            return dbInitialize();
        } else {
            return false;
        }
    }
    return true;
}

bool dbInitialize(void) {
    // 略微优化数据库写入性能https://www.jianshu.com/p/1fe61d31f75e
    const char *sql = "pragma journal_mode = wal; pragma synchronous = normal; create table if not exists method (key text, class_method int, ret_type text, class text, sel text, param_count int, param2 text, param3 text, param4 text, param5 text, param6 text, param7 text, param8 text, param9 text, param10 text, param11 text, param12 text, param13 text, param14 text, primary key(key));";
    return dbExecute(sql);
}
void dbCheckpoint(void) {
    if (!dbCheck()) return;
    // Cause a checkpoint to occur, merge `sqlite-wal` file to `sqlite` file.
    sqlite3_wal_checkpoint(_db, NULL);
}

bool dbExecute(const char *sql) {
    if (strlen(sql) == 0) return false;
    if (!dbCheck()) return false;

    char *error = NULL;
    int result = sqlite3_exec(_db, sql, NULL, NULL, &error);
    if (error) {
        if (_errorLogsEnabled) clog("%s line:%d sqlite exec error (%d): %s", __FUNCTION__, __LINE__, result, error);
        sqlite3_free(error);
    }
    
    return result == SQLITE_OK;
}

sqlite3_stmt * dbPrepareStmt(const char *sql) {
    if (!dbCheck() || strlen(sql) == 0 || !_dbStmtCache) return NULL;
    CFStringRef cfKey = CFStringCreateWithCString(NULL, sql, kCFStringEncodingUTF8);
    sqlite3_stmt *stmt = (sqlite3_stmt *)CFDictionaryGetValue(_dbStmtCache, cfKey);
    if (!stmt) {
        int result = sqlite3_prepare_v2(_db, sql, -1, &stmt, NULL);
        if (result != SQLITE_OK) {
            if (_errorLogsEnabled) {
                clog("%s line:%d sqlite stmt prepare error (%d): %s", __FUNCTION__, __LINE__, result, sqlite3_errmsg(_db));
            }
            return NULL;
        }
        CFDictionaryAddValue(_dbStmtCache, cfKey, stmt);
    } else {
        sqlite3_reset(stmt);
    }
    return stmt;
}

bool dbInsertCallstack(call_stack *cs) {
    if (!cs->is_tracking) { return false; }
        
    char *key = (char *)calloc(1, strlen(cs->class_name) + strlen(cs->method_name) + 5);
    strncpy(key, cs->class_name, strlen(cs->class_name));
    strcat(key, "|");
    strcat(key, cs->method_name);
    strcat(key, "|");
    strcat(key, cs->is_class_method ? "1" : "0");
    
//    CFStringRef cfKey = CFStringCreateWithCString(NULL, key, kCFStringEncodingUTF8);
//
//    if (CFSetContainsValue(_dbInsertedKeys, cfKey)) {
//        CFRelease(cfKey);
//        free(key);
//        return false;
//    }
    
    const char *sql = "insert or replace into method (key, class_method, ret_type, class, sel, param_count, param2, param3, param4, param5, param6, param7, param8, param9, param10, param11, param12, param13, param14) values (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13, ?14, ?15, ?16, ?17, ?18, ?19);";
    sqlite3_stmt *stmt = dbPrepareStmt(sql);
    if (!stmt) {
//        CFRelease(cfKey);
        free(key);
        return false;
    }

    sqlite3_bind_text(stmt, 1, key, -1, NULL);
    sqlite3_bind_int (stmt, 2, cs->is_class_method ? 1 : 0);
    sqlite3_bind_text(stmt, 3, cs->ret_type, -1, NULL);
    sqlite3_bind_text(stmt, 4, cs->class_name, -1, NULL);
    sqlite3_bind_text(stmt, 5, cs->method_name, -1, NULL);
    sqlite3_bind_int (stmt, 6, cs->param_count);
    for (int i = 2;  i <= 14; i++) {
        if (i < cs->param_count) {
            sqlite3_bind_text(stmt, 5 + i, cs->params[i], -1, NULL);
        } else {
            sqlite3_bind_text(stmt, 5 + i, NULL, -1, NULL);
        }
    }

    int result = sqlite3_step(stmt);
    if (result != SQLITE_DONE) {
        if (_errorLogsEnabled)  {
            clog("%s line:%d sqlite insert error (%d): %s", __FUNCTION__, __LINE__, result, sqlite3_errmsg(_db));
        }
//        CFRelease(cfKey);
        free(key);
        return false;
    }
    
//    CFSetAddValue(_dbInsertedKeys, cfKey);
//    CFRelease(cfKey);
    free(key);

    return true;
}

bool dbClearAllData(void)
{
    const char *sql = "DELETE FROM method;";
    sqlite3_stmt *stmt = dbPrepareStmt(sql);
    if (!stmt) {
        return false;
    }
    
    int result = sqlite3_step(stmt);
    if (result != SQLITE_DONE) {
        if (_errorLogsEnabled)  {
            clog("%s line:%d sqlite insert error (%d): %s", __FUNCTION__, __LINE__, result, sqlite3_errmsg(_db));
        }
        return false;
    }
    
    dbFlush();
    
    return true;
}

call_stack * dbGetCallStackFromStmt(sqlite3_stmt *stmt) {
//  create table if not exists method (key text, class_method int, ret_type text, class text, sel text, param2 text, param3 text, param4 text, param5 text, param6 text, param7 text, param8 text, param9 text, param10 text, param11 text, param12 text, param13 text, param14 text, primary key(key));
    int idx = 0;
    char *key = strdup((char *)sqlite3_column_text(stmt, idx++));
    int class_method = sqlite3_column_int(stmt, idx++);
    char *ret_type = strdup((char *)sqlite3_column_text(stmt, idx++));
    char *class = strdup((char *)sqlite3_column_text(stmt, idx++));
    char *sel = strdup((char *)sqlite3_column_text(stmt, idx++));
    int param_count = sqlite3_column_int(stmt, idx++);
    
    call_stack *cs = (call_stack *)calloc(1, sizeof(call_stack));
    cs->class_name = class;
    cs->method_name = sel;
    cs->is_class_method = class_method > 0;
    cs->ret_type = ret_type;
    cs->param_count = param_count;
    cs->params = (char **)calloc(cs->param_count, sizeof(char *));
    cs->key = key;
    for (int i = 2; (i < cs->param_count) && (i < 13); i++) {
        cs->params[i] = strdup((char *)sqlite3_column_text(stmt, idx + i - 2));
    }
    return cs;
}

CFDictionaryRef dbGetCallStacksWithClass(const char *class) {
    if(!dbCheck()) {
        return nil;
    }
    
    char *sql = calloc(1, 256);
    const char *slqFormater = "select * from method where class='%s';";
    sprintf(sql, slqFormater, class);
    
    sqlite3_stmt *stmt = NULL;
    int result = sqlite3_prepare_v2(_db, sql, -1, &stmt, NULL);
    if (result != SQLITE_OK) {
        if (_errorLogsEnabled) {
            clog("%s line:%d sqlite stmt prepare error (%d): %s", __FUNCTION__, __LINE__, result, sqlite3_errmsg(_db));
        }
        free(sql);
        return nil;
    }
    
    CFMutableDictionaryRef csDict = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    do {
        result = sqlite3_step(stmt);
        if (result == SQLITE_ROW) {
            call_stack *cs = dbGetCallStackFromStmt(stmt);
            if (cs != NULL) {
                CFDictionarySetValue(csDict, cs->key, cs);
            }
        } else if (result == SQLITE_DONE) {
            break;
        } else {
            if (_errorLogsEnabled) {
                clog("%s line:%d sqlite query error (%d): %s", __FUNCTION__, __LINE__, result, sqlite3_errmsg(_db));
            }
            break;
        }
    } while (1);
    sqlite3_finalize(stmt);
    free(sql);
    return csDict;
}


