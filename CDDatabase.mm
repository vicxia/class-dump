//
//  CDDatabase.m
//  class-dump
//
//  Created by vic on 2020/1/23.
//

#import "CDDatabase.h"
#import "CDOCMethodRuntimeInfo.h"
#import "sqlite3.h"

@interface CDDatabase() {
    sqlite3 *_db;
    BOOL _errorLogsEnabled;
}

@end

@implementation CDDatabase

- (instancetype)initWithDBPath:(NSString *)dbPath
{
    if (self = [super init]) {
        [self openDBWithPath:dbPath];
    }
    return self;
}

- (void)dealloc
{
    [self closeDB];
}

- (void)openDBWithPath:(NSString *)dbPath
{
    if (_db != NULL) {
        return;
    }
    int result = sqlite3_open(dbPath.UTF8String, &_db);
    if (result == SQLITE_OK) {
        NSLog(@"openDB succeed");
//        [self dbInitialize];
    } else {
        NSLog(@"openDB failed");
    }
}

- (void)closeDB
{
    if (!_db) { return;}
    
    int  result = 0;
    bool retry = false;
    bool stmtFinalized = false;
    
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
                NSLog(@"%s line:%d sqlite close failed (%d).", __FUNCTION__, __LINE__, result);
            }
        }
    } while (retry);
    _db = NULL;
}

- (CDOCMethodRuntimeInfo *) runtimeInfoFromStmt:(sqlite3_stmt *)stmt
{
//  create table if not exists method (key text, class_method int, ret_type text, class text, sel text, param2 text, param3 text, param4 text, param5 text, param6 text, param7 text, param8 text, param9 text, param10 text, param11 text, param12 text, param13 text, param14 text, primary key(key));
    int idx = 0;
    CDOCMethodRuntimeInfo *runtimeInfo = [CDOCMethodRuntimeInfo new];
    runtimeInfo.key = [NSString stringWithUTF8String:(char *)sqlite3_column_text(stmt, idx++)];
    runtimeInfo.isClassMethod = sqlite3_column_int(stmt, idx++) > 0;
    runtimeInfo.retType = [NSString stringWithUTF8String:(char *)sqlite3_column_text(stmt, idx++)];
    runtimeInfo.className = [NSString stringWithUTF8String:(char *)sqlite3_column_text(stmt, idx++)];
    runtimeInfo.methodName = [NSString stringWithUTF8String:(char *)sqlite3_column_text(stmt, idx++)];
    int paramCount = sqlite3_column_int(stmt, idx++);
    
    NSMutableArray *params = [NSMutableArray arrayWithCapacity:paramCount - 2];
    for (int i = 2; (i < paramCount) && (i < 13); i++) {
        char *type = (char *)sqlite3_column_text(stmt, idx + i - 2);
        if (type != NULL) {
            [params addObject:[NSString stringWithUTF8String:type]];
        } else {
            [params addObject:[NSNull null]];
        }
    }
    runtimeInfo.paramTypes = params;
    return runtimeInfo;
}

- (BOOL) dbInitialize {
    // 略微优化数据库写入性能https://www.jianshu.com/p/1fe61d31f75e
    NSString *sql = @"pragma journal_mode = wal; pragma synchronous = normal; create table if not exists method (key text, class_method int, ret_type text, class text, sel text, param_count int, param2 text, param3 text, param4 text, param5 text, param6 text, param7 text, param8 text, param9 text, param10 text, param11 text, param12 text, param13 text, param14 text, primary key(key));";
    return [self dbExecute:sql];
}

- (BOOL)dbExecute:(NSString *)sql {
    if (sql.length == 0) return false;
    char *error = NULL;
    int result = sqlite3_exec(_db, sql.UTF8String, NULL, NULL, &error);
    if (error) {
        if (_errorLogsEnabled) {
            NSLog(@"%s line:%d sqlite exec error (%d): %s", __FUNCTION__, __LINE__, result, error);
        }
        sqlite3_free(error);
    }
    
    return result == SQLITE_OK;
}

- (NSDictionary<NSString *, CDOCMethodRuntimeInfo *> *)runtimeInfoForClass:(NSString *)clazz
{
    if (!_db) {
        return [NSDictionary dictionary];
    }

    NSString *sql = [NSString stringWithFormat:@"select * from method where class='%s';",clazz.UTF8String];

    sqlite3_stmt *stmt = NULL;
    int result = sqlite3_prepare_v2(_db, sql.UTF8String, -1, &stmt, NULL);
    if (result != SQLITE_OK) {
        if (_errorLogsEnabled) {
            NSLog(@"%s line:%d sqlite stmt prepare error (%d): %s", __FUNCTION__, __LINE__, result, sqlite3_errmsg(_db));
        }
        return nil;
    }
    
    NSMutableDictionary *dict = [NSMutableDictionary dictionary];
    do {
        result = sqlite3_step(stmt);
        if (result == SQLITE_ROW) {
            CDOCMethodRuntimeInfo *runtimeInfo = [self runtimeInfoFromStmt:stmt];
            dict[runtimeInfo.key] = runtimeInfo;
        } else if (result == SQLITE_DONE) {
            break;
        } else {
            if (_errorLogsEnabled) {
                NSLog(@"%s line:%d sqlite query error (%d): %s", __FUNCTION__, __LINE__, result, sqlite3_errmsg(_db));
            }
            break;
        }
    } while (1);
    sqlite3_finalize(stmt);
//    free(sql);
    return dict;
}

+ (CDOCMethodRuntimeInfo *)runtimeInfoWithClass:(NSString *)clazz
                                         method:(NSString *)method
                                  isClassMethod:(BOOL)isClassMethod
                                       fromDict:(NSDictionary *)dict
{
    NSString *key = [NSString stringWithFormat:@"%@|%@|%d",clazz, method, isClassMethod ? 1 : 0];
    return dict[key];
}

@end
