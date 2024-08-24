#import <AppKit/AppKit.h>
#import <Metal/Metal.h>
#import <QuartzCore/QuartzCore.h>

@protocol MetalViewDelegate <NSObject>

-(void)drawableResize:(CGSize)size;
-(void)renderToMetalLayer:(nonnull CAMetalLayer *)metalLayer;

@end

@interface MetalView: NSView <CALayerDelegate>

@property(nonatomic, nonnull, readonly) CAMetalLayer *metalLayer;
@property(nonatomic, weak) id<MetalViewDelegate> _Nullable delegate;

-(void)commonInit;
-(void)resizeDrawable:(CGFloat)scaleFactor;
-(void)stopRenderLoop;
-(void)render;

@end