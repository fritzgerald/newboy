#import "GameViewController.h"
#include <Metal/Metal.h>
#import "GBView.h"
#import "GameRenderer.h"

@interface GameViewController() <MetalViewDelegate>

@end

@implementation GameViewController {
    GameRenderer* _renderer;
}

- (void)loadView {
    GBView* gbView = [[GBView alloc] init];
    gbView.delegate = self;
    self.view = gbView;
}

-(void)viewDidLoad {
    [super viewDidLoad];

    id<MTLDevice> device = MTLCreateSystemDefaultDevice();

    GBView *view = (GBView *)self.view;

    // Set the device for the layer so the layer can create drawable textures that can be rendered to
    // on this device.
    view.metalLayer.device = device;

    // Set this class as the delegate to receive resize and render callbacks.
    view.delegate = self;

    view.metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm_sRGB;

    _renderer = [[GameRenderer alloc] initWithMetalDevice:device
                                      drawablePixelFormat:view.metalLayer.pixelFormat];
}

- (void)renderToMetalLayer:(nonnull CAMetalLayer *)metalLayer {
    [_renderer renderToMetalLayer: metalLayer];
}

- (void)drawableResize:(CGSize)size {
    [_renderer drawableResize:size];
}

@end