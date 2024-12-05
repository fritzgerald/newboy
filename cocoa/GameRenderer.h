#import <Metal/Metal.h>
#include <objc/NSObject.h>
#import <QuartzCore/CAMetalLayer.h>
#import "core/Newboy.h"

@interface GameRenderer: NSObject

@property NSInteger frameRate;
@property GBJoypadState joypad;
@property GB_device* gameboydevice;

- (nonnull instancetype)initWithMetalDevice:(nonnull id<MTLDevice>)device
                        drawablePixelFormat:(MTLPixelFormat)drawabklePixelFormat
                        romPath:(NSString*)romPath;

- (void)renderToMetalLayer:(nonnull CAMetalLayer*)metalLayer;

- (void)drawableResize:(CGSize)drawableSize;

- (void)disposeRessources;

- (void)saveRam;

- (void)setDMGColorPalette:(uint32_t*)palette;

@end