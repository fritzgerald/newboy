#import <AppKit/AppKit.h>
#include <Foundation/Foundation.h>

@interface GBViewController: NSViewController

@property (nonatomic, strong, nonnull) NSString* romFilePath;

-(id)initWithRomFilePath:(NSString *) romFilePath;

@end