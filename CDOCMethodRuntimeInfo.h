//
//  CDOCMethodRuntimeInfo.h
//  class-dump
//
//  Created by vic on 2020/1/23.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface CDOCMethodRuntimeInfo : NSObject <NSCopying>

@property (nonatomic, strong) NSString *key;
@property (nonatomic, strong) NSString *className;
@property (nonatomic, strong) NSString *methodName;
@property (nonatomic, assign) BOOL isClassMethod;
@property (nonatomic, strong) NSArray<NSString *> *paramTypes;
@property (nonatomic, strong) NSString *retType;

@end

NS_ASSUME_NONNULL_END
