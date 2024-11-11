#pragma once

#import "AppDelegate.h"
#import <Cocoa/Cocoa.h>

@interface AppDelegate (Menu)

- (void)buildNewMenu;
- (void)addRecentFile:(NSString*) path;
- (NSMenuItem*)buildAppMenuItem;
- (NSMenuItem*)buildFileMenuItem;

@end