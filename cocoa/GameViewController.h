#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>

@interface GameViewController: NSViewController

@property (nonatomic, assign) NSInteger dmgPaletteId;

-(id)initWithRomFilePath:(NSString *) path;

- (void)saveRam;

@end