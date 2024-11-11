#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>
#import "GameViewController.h"
#import "AppDelegate.h"
#import "AppDelegate+Menu.h"

@implementation AppDelegate

- (void)startEmulator:(NSString*)romFile {
    [self addRecentFile: romFile];

    NSRect frame = NSMakeRect(0, 0, 600, 600);
    GameViewController* vc = [[GameViewController alloc] initWithRomFilePath:romFile];
    NSWindow* window = [NSWindow windowWithContentViewController: vc];
    window.title = @"NewBoy";

    [window setContentSize:NSMakeSize(600, 600)];
    [window makeKeyAndOrderFront:nil];
    [self buildNewMenu];
}

- (void) applicationDidFinishLaunching:(NSNotification *)notification {
    [NSApp activate];
    [self buildNewMenu];
}

@end