#include "cocoa/GameViewController.h"
#import <AppKit/AppKit.h>
#include <AppKit/NSWindow.h>
#import <Foundation/Foundation.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import "AppDelegate+Menu.h"


static NSString *GBRecentFileUserDefaultKey = @"GB_recent_files";

@interface AppDelegate (PrivateMenu)

@property(readonly) GameViewController* _Nullable currentGameViewController;

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

- (NSMenuItem*)buildEmulationMenuItem {
    NSMenuItem* emuMenuItem = [NSMenuItem new];

    NSMenu *emulationMenu = [[NSMenu alloc] initWithTitle: @"Emulation"];
    [emuMenuItem setSubmenu: emulationMenu];

    [emulationMenu addItemWithTitle:@"Save ram" action:@selector(saveRamData:) keyEquivalent:@"s"];

    NSMenuItem* dmgPalette = [NSMenuItem new];
    dmgPalette.title = @"DMG palette";
    [emulationMenu addItem:dmgPalette];

    NSMenu *dmgPaletteSubMenu = [[NSMenu alloc] initWithTitle:@"DMG palette"];
    [dmgPalette setSubmenu:dmgPaletteSubMenu];

    NSMenuItem* greyScaleItem = [[NSMenuItem alloc] initWithTitle:@"Grey scales" action:@selector(setDMGColorPalette:) keyEquivalent:@""];
    [greyScaleItem setRepresentedObject: @(0)];
    [dmgPaletteSubMenu addItem:greyScaleItem];

    NSMenuItem* dmgItem = [[NSMenuItem alloc] initWithTitle:@"DMG green" action:@selector(setDMGColorPalette:) keyEquivalent:@""];
    [dmgItem setRepresentedObject: @(1)];
    [dmgPaletteSubMenu addItem:dmgItem];

    if (self.currentGameViewController.dmgPaletteId == 0) {
        [dmgPaletteSubMenu setSelectedItems: @[greyScaleItem]];
    } else if (self.currentGameViewController.dmgPaletteId == 1) {
        [dmgPaletteSubMenu setSelectedItems: @[dmgItem]];
    }

    return emuMenuItem;
}

-(void)buildNewMenu {
    NSMenu* mainMenu = [NSMenu new];
    [[NSApplication sharedApplication] setMenu: mainMenu];

    [mainMenu addItem: [self buildAppMenuItem]];
    [mainMenu addItem: [self buildFileMenuItem]];
    if (self.currentGameViewController != nil) {
        [mainMenu addItem: [self buildEmulationMenuItem]];
    }
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
        [UTType typeWithFilenameExtension: @"gb"],
        [UTType typeWithFilenameExtension: @"gbc"]
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

- (GameViewController* _Nullable)currentGameViewController {
    if ([[NSApplication sharedApplication] keyWindow] != nil &&
        [[NSApplication sharedApplication].keyWindow.contentViewController isKindOfClass:[GameViewController class]]
    ) {
        return (GameViewController*)[NSApplication sharedApplication].keyWindow.contentViewController;
    }
    return nil;
}

- (void)saveRamData: (id)selector {
    [self.currentGameViewController saveRam];
}

- (void)setDMGColorPalette: (id)sender {
    if (![sender isKindOfClass:[NSMenuItem class]]) {
        return;
    }
    NSMenuItem* menu = sender;
    if (![menu.representedObject isKindOfClass:[NSNumber class]]) {
        return;
    }
    NSNumber* value = menu.representedObject;
    [self.currentGameViewController setDmgPaletteId:value.integerValue];
    [menu.parentItem.submenu setSelectedItems: @[menu]];
}

@end