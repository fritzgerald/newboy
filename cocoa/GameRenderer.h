#import <Metal/Metal.h>
#include <objc/NSObject.h>
#import <QuartzCore/CAMetalLayer.h>
#import "core/MMU.h"

@interface GameRenderer: NSObject

@property GBJoypadState joypad;

- (nonnull instancetype)initWithMetalDevice:(nonnull id<MTLDevice>)device
                        drawablePixelFormat:(MTLPixelFormat)drawabklePixelFormat;

- (void)renderToMetalLayer:(nonnull CAMetalLayer*)metalLayer;

- (void)drawableResize:(CGSize)drawableSize;

@end