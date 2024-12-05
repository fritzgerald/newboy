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
    [self.trackedWindows addObject:[[GBWeakWindowReference alloc] initWith:window]];
}

- (void) applicationDidFinishLaunching:(NSNotification *)notification {
    [NSApp activate];
    self.trackedWindows = [NSMutableArray new];
    [self buildNewMenu];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(buildNewMenu) name:NSWindowDidBecomeKeyNotification object:nil];
}

@end

@implementation GBWeakWindowReference

-(id)initWith:(NSWindow*) window {
    self = [super init];
    if(self) {
        self.weakWindow = window;
    }
    return self;
}

@end