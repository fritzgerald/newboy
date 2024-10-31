#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import "AppDelegate+Menu.h"


static NSString *GBRecentFileUserDefaultKey = @"GB_recent_files";

@interface AppDelegate (PrivateMenu)

- (NSArray<NSString*>*)getRecentFiles;
- (void) clearRecentFileList: (id)sender;

@end

@implementation AppDelegate (Menu)

- (NSArray<NSString*>*)getRecentFiles {
    NSArray<NSString*>* filePathList = [[NSUserDefaults standardUserDefaults] arrayForKey: GBRecentFileUserDefaultKey];
    if (filePathList == nil) {
        filePathList = @[];
    }
    return filePathList;
}

- (void)addRecentFile: (NSString*) path {
    NSMutableArray<NSString*>* recents = [NSMutableArray arrayWithArray: [self getRecentFiles]];
    [recents removeObject:path];
    [recents insertObject:path atIndex:0];
    [[NSUserDefaults standardUserDefaults] setValue:recents forKey: GBRecentFileUserDefaultKey];
    [self buildNewMenu];
}

- (void)clearRecentFileList: (id)sender {
    [[NSUserDefaults standardUserDefaults] setValue:@[] forKey: GBRecentFileUserDefaultKey];
    [self buildNewMenu];
}

- (NSMenuItem*)buildAppMenuItem {
    NSMenuItem* appMenuItem = [NSMenuItem new];
    NSMenu *appMenu = [NSMenu new];
    [appMenu addItemWithTitle: @"Quit NewBoy" action:@selector(quitApplication:) keyEquivalent:@"q"];
    [appMenuItem setSubmenu:appMenu];
    return  appMenuItem;
}

- (NSMenuItem*)buildFileMenuItem {
    NSMenuItem* fileMenuItem = [NSMenuItem new];

    NSMenu *fileMenu = [[NSMenu alloc] initWithTitle: @"File"];
    [fileMenuItem setSubmenu:fileMenu];

    [fileMenu addItemWithTitle:@"Open.." action:@selector(openGameDialog:) keyEquivalent:@"O"];
    
    NSMenuItem* recentMenu = [NSMenuItem new];
    recentMenu.title = @"Open recent";
    [fileMenu addItem:recentMenu];

    NSMenu *recentSubMenu = [[NSMenu alloc] initWithTitle:@"Open recent"];
    [recentMenu setSubmenu:recentSubMenu];

    for(NSString* path in [self getRecentFiles]) {
        NSMenuItem* newItem = [[NSMenuItem alloc] initWithTitle:[path lastPathComponent] action:@selector(onRecentFile:) keyEquivalent:@""];
        [newItem setRepresentedObject: path];

        [recentSubMenu addItem:newItem];
    }
    [recentSubMenu addItem:[NSMenuItem separatorItem]];
    NSMenuItem* clearRecent = [[NSMenuItem alloc] initWithTitle:@" Clear Menu" action:@selector(clearRecentFileList:) keyEquivalent:@""];
    [recentSubMenu addItem: clearRecent];

    [fileMenu addItem: [NSMenuItem separatorItem]];
    [fileMenu addItemWithTitle:@"Close Window" action:@selector(closeWindow:) keyEquivalent:@"W"];

    return fileMenuItem;
}

-(void)buildNewMenu {
    NSMenu* mainMenu = [NSMenu new];
    [[NSApplication sharedApplication] setMenu: mainMenu];

    [mainMenu addItem: [self buildAppMenuItem]];
    [mainMenu addItem: [self buildFileMenuItem]];
}

-(void) onRecentFile: (id)sender {
    NSMenuItem* item = sender;
    if ([item.representedObject isKindOfClass:[NSString class]]) {
        NSString* path = item.representedObject;
        [self startEmulator:path];
    }
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

- (void)quitApplication: (id)sender {

}

- (void)closeWindow: (id)sender {

}

@end