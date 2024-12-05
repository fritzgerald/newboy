#pragma once

#include "cocoa/AppDelegate.h"
#import <Cocoa/Cocoa.h>

@interface GBWeakWindowReference: NSObject

@property (weak, nonatomic) NSWindow* weakWindow;

-(id)initWith:(NSWindow*) window;

@end

@interface AppDelegate: NSObject<NSApplicationDelegate>

@property (nonatomic, strong) NSWindow *mainWindow;
@property (nonatomic, strong) NSMutableArray<GBWeakWindowReference*>* trackedWindows;

- (void)startEmulator:(NSString*)romFile;

@end