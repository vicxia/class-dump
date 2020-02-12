// -*- mode: ObjC -*-

//  This file is part of class-dump, a utility for examining the Objective-C segment of Mach-O files.
//  Copyright (C) 1997-2019 Steve Nygard.

@class CDTypeController, CDOCMethodRuntimeInfo;

@interface CDOCMethod : NSObject <NSCopying>

- (id)initWithName:(NSString *)name typeString:(NSString *)typeString;
- (id)initWithName:(NSString *)name typeString:(NSString *)typeString address:(NSUInteger)address;

@property (nonatomic, strong) CDOCMethodRuntimeInfo *runtimeInfo;

@property (readonly) NSString *name;
@property (readonly) NSString *typeString;
@property (assign) NSUInteger address;

- (NSArray *)parsedMethodTypes;

- (void)appendToString:(NSMutableString *)resultString typeController:(CDTypeController *)typeController;

- (NSComparisonResult)ascendingCompareByName:(CDOCMethod *)other;

@end
