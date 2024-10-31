#pragma once

#import <Cocoa/Cocoa.h>

@interface AppDelegate: NSObject<NSApplicationDelegate>

@property (nonatomic, strong) NSWindow *mainWindow;

- (void)startEmulator:(NSString*)romFile;

@end