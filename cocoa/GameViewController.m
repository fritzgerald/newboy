#import "GameViewController.h"
#include <Foundation/NSString.h>
#include <stdbool.h>
#include <AppKit/AppKit.h>
#include <Metal/Metal.h>
#import "GBView.h"
#import "GameRenderer.h"
#import <Carbon/Carbon.h>
#import "core/MMU.h"

@interface GameViewController() <MetalViewDelegate>

@end

@implementation GameViewController {
    GameRenderer* _renderer;
    GBJoypadState joypad;
    NSString* _romPath;
}

-(id)initWithRomFilePath:(NSString *) path {
    self = [super init];
    if (self) {
        _romPath = path;
    }
    return self;
}

- (void)loadView {
    GBView* gbView = [[GBView alloc] init];
    gbView.delegate = self;
    self.view = gbView;
}

-(void)viewDidLoad {
    [super viewDidLoad];

    joypad = (GBJoypadState) { false, false, false, false, false, false, false, false };

    id<MTLDevice> device = MTLCreateSystemDefaultDevice();

    GBView *view = (GBView *)self.view;

    // Set the device for the layer so the layer can create drawable textures that can be rendered to
    // on this device.
    view.metalLayer.device = device;

    // Set this class as the delegate to receive resize and render callbacks.
    view.delegate = self;

    view.metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm_sRGB;

    _renderer = [[GameRenderer alloc] initWithMetalDevice:device
                                      drawablePixelFormat:view.metalLayer.pixelFormat
                                      romPath:_romPath];

    [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskKeyDown handler:^ NSEvent * (NSEvent * event){

        switch (event.keyCode) {
            case kVK_UpArrow:
                self->joypad.upPressed = true;
                break;
            case kVK_DownArrow:
                self->joypad.downPressed = true;
                break;
            case kVK_LeftArrow:
                self->joypad.leftPressed = true;
                break;
            case kVK_RightArrow:
                self->joypad.rightPressed = true;
                break;
            case kVK_ANSI_Z:
                self->joypad.aPressed = true;
                break;
            case kVK_ANSI_X:
                self->joypad.bPressed = true;
                break;
            case kVK_Return:
                self->joypad.startPressed = true;
                break;
            case kVK_Escape:
                self->joypad.selectPressed = true;
                break;

        }
        return nil;
    }];

    [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskKeyUp handler:^ NSEvent * (NSEvent * event){

        switch (event.keyCode) {
            case kVK_UpArrow:
                self->joypad.upPressed = false;
                break;
            case kVK_DownArrow:
                self->joypad.downPressed = false;
                break;
            case kVK_LeftArrow:
                self->joypad.leftPressed = false;
                break;
            case kVK_RightArrow:
                self->joypad.rightPressed = false;
                break;
            case kVK_ANSI_Z:
                self->joypad.aPressed = false;
                break;
            case kVK_ANSI_X:
                self->joypad.bPressed = false;
                break;
            case kVK_Return:
                self->joypad.startPressed = false;
                break;
            case kVK_Escape:
                self->joypad.selectPressed = false;
                break;
        }
        return nil;
    }];
}

-(void)viewWillDisappear {
    [super viewWillDisappear];

    [_renderer disposeRessources];
}

- (void)renderToMetalLayer:(nonnull CAMetalLayer *)metalLayer {
    _renderer.joypad = joypad;
    [_renderer renderToMetalLayer: metalLayer];
}

- (void)drawableResize:(CGSize)size {
    [_renderer drawableResize:size];
}

@end