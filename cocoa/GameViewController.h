#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>
#import "core/Newboy.h"

@interface GameViewController: NSViewController

@property (strong, nonatomic) GameViewController* linkGameViewController;
@property (nonatomic, assign) NSInteger dmgPaletteId;
@property (nonatomic, strong) NSString* romPath;

-(id)initWithRomFilePath:(NSString *) path;

- (void)saveRam;

- (GB_device*) gameboydevice;

@end