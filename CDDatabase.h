//
//  CDDatabase.h
//  class-dump
//
//  Created by vic on 2020/1/23.
//

#import <Foundation/Foundation.h>

@class CDOCMethodRuntimeInfo;

NS_ASSUME_NONNULL_BEGIN

@interface CDDatabase : NSObject

- (instancetype)initWithDBPath:(NSString *)dbPath;

- (NSDictionary<NSString *, CDOCMethodRuntimeInfo *> *)runtimeInfoForClass:(NSString *)clazz;

+ (CDOCMethodRuntimeInfo *)runtimeInfoWithClass:(NSString *)clazz
                                         method:(NSString *)method
                                  isClassMethod:(BOOL)isClassMethod
                                       fromDict:(NSDictionary *)dict;
@end

NS_ASSUME_NONNULL_END
