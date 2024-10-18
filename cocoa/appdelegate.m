#import "appdelegate.h"
#include "cocoa/GBViewController.h"
#include <MacTypes.h>
#include <AppKit/AppKit.h>
#include <Foundation/Foundation.h>
#include "GBView.h"
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import "GameViewController.h"

@implementation AppDelegate

- (id) init {
    NSLog(@"Entered AppDelegate init"); 
    return [super init];
}

- (void) setupMainMenu {
    NSMenu *mainMenu = [NSMenu new];
    [[NSApplication sharedApplication] setMenu:mainMenu];

    NSMenuItem* appMenuItem = [NSMenuItem new];
    NSMenu *appMenu = [NSMenu new];
    [appMenu addItemWithTitle: @"Quit" action:@selector(menuAction:) keyEquivalent:@"q"];
    [appMenuItem setSubmenu:appMenu];
    [mainMenu addItem:appMenuItem];

    NSMenuItem* fileMenuItem = [NSMenuItem new];
    NSMenu *fileMenu = [[NSMenu alloc] initWithTitle: @"File"];
    [fileMenu addItemWithTitle:@"open.." action:@selector(openGameDialog:) keyEquivalent:@""];
    [fileMenuItem setSubmenu:fileMenu];
    [mainMenu addItem: fileMenuItem];
}

- (void) menuAction: (id)sender {
    NSLog(@"%@", sender);
}

- (void) openGameDialog: (id)sender {
    NSOpenPanel* openPanel = [NSOpenPanel openPanel];
    openPanel.title = @"Open game boy ROM";
    openPanel.canChooseDirectories = NO;
    openPanel.allowsMultipleSelection = NO;
    openPanel.allowedContentTypes = @[ 
        [UTType typeWithFilenameExtension: @"gb"]
    ];

    //this launches the dialogue
    __weak AppDelegate* weakSelf = self;
    [openPanel beginWithCompletionHandler:^(NSInteger result) {
        //if the result is NSOKButton
        //the user selected a file
        if (result == NSModalResponseOK) {
            //get the selected file URLs
            NSURL *selection = openPanel.URLs[0];
            NSString* path = [[selection path] stringByResolvingSymlinksInPath];
            
            [weakSelf startEmulator:path];
        }
    }];
}

- (void) startEmulator:(NSString*)romFile {
    NSRect frame = NSMakeRect(0, 0, 600, 600);
    GameViewController* vc = [[GameViewController alloc] initWithRomFilePath:romFile];
    NSWindow* window = [NSWindow windowWithContentViewController: vc];
    window.title = @"NewBoy";

    [window setContentSize:NSMakeSize(600, 600)];
    [window makeKeyAndOrderFront:nil];
}

-(void) presentGameViewController {
    NSRect frame = NSMakeRect(0, 0, 600, 600);
    GameViewController* vc = [[GameViewController alloc] init];
    NSWindow* window = [NSWindow windowWithContentViewController: vc];
    window.title = @"NewBoy";

    [window setContentSize:NSMakeSize(600, 600)];
    [window makeKeyAndOrderFront:nil];
}

- (void) applicationDidFinishLaunching:(NSNotification *)notification {
    [NSApp activate];
    [self setupMainMenu];
}

@end