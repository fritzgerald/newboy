#import "MetalView.h"
#include <QuartzCore/CAFrameRateRange.h>
#include <AppKit/AppKit.h>
#include <Foundation/Foundation.h>
#include <QuartzCore/QuartzCore.h>

@implementation MetalView {
    CVDisplayLinkRef _displayLink;
    dispatch_source_t _displaySource;
}

- (instancetype)initWithFrame:(CGRect)frame
{
    self = [super initWithFrame:frame];
    if(self) {
        [self commonInit];
    }
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)aDecoder
{
    self = [super initWithCoder:aDecoder];
    if(self) {
        [self commonInit];
    }
    return self;
}

- (void)commonInit {
    self.wantsLayer = YES;
    self.layerContentsRedrawPolicy = NSViewLayerContentsRedrawDuringViewResize;

    _metalLayer = (CAMetalLayer *) self.layer;
    self.layer.delegate = self;
}

- (CALayer *)makeBackingLayer
{
    return [CAMetalLayer layer];
}

- (void)viewDidMoveToWindow {
    [super viewDidMoveToWindow];

    [self setupRenderLoop];
    [self resizeDrawable:self.window.screen.backingScaleFactor];
}

- (void)stopRenderLoop {
    if(_displaySource) {
        dispatch_source_cancel(_displaySource);
    }
}

- (void)resizeDrawable:(CGFloat)scaleFactor
{
    CGSize newSize = self.bounds.size;
    newSize.width *= scaleFactor;
    newSize.height *= scaleFactor;

    if(newSize.width <= 0 || newSize.width <= 0) {
        return;
    }

    if(newSize.width == _metalLayer.drawableSize.width &&
       newSize.height == _metalLayer.drawableSize.height) {
        return;
    }

    _metalLayer.drawableSize = newSize;

    [_delegate drawableResize:newSize];
}

- (void)setupRenderLoop {
    _displaySource = dispatch_source_create(
        DISPATCH_SOURCE_TYPE_TIMER, 
        0, 
        0,
        dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0)
    ); 

    double intervalSeconds = 1/60.0;
    dispatch_time_t startTime = dispatch_time(DISPATCH_TIME_NOW, 0);
    uint64_t intervalNanoSeconds = (int64_t)(intervalSeconds * NSEC_PER_SEC);
    dispatch_source_set_timer(_displaySource, startTime, intervalNanoSeconds, 0);

    __weak MetalView* weakSelf = self;
    dispatch_source_set_event_handler(_displaySource, ^{
        [weakSelf render];
    });

    // Start the timer
    dispatch_resume(_displaySource);
    // When you want to stop the timer, you need to suspend the source
    // dispatch_suspend(dispatchSource);
}

- (void)render {
    [self.delegate renderToMetalLayer:_metalLayer];
}

- (void)renderFrame:(id)sender {
    // 'DispatchRenderLoop' is always called on a secondary thread.  Merge the dispatch source
    // setup for the main queue so that rendering occurs on the main thread
    [self render];
}

- (void)windowWillClose:(NSNotification*)notification
{
    // Stop the display link when the window is closing since there
    // is no point in drawing something that can't be seen
    if(notification.object == self.window) {
        CVDisplayLinkStop(_displayLink);
        dispatch_source_cancel(_displaySource);
    }
}

- (void)viewDidChangeBackingProperties {
    [super viewDidChangeBackingProperties];
    [self resizeDrawable:self.window.screen.backingScaleFactor];
}

- (void)setFrameSize:(NSSize)size {
    [super setFrameSize:size];
    [self resizeDrawable:self.window.screen.backingScaleFactor];
}

- (void)setBoundsSize:(NSSize)size {
    [super setBoundsSize:size];
    [self resizeDrawable:self.window.screen.backingScaleFactor];
}

@end