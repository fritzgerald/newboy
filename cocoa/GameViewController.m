#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <Carbon/Carbon.h>

#import "GameViewController.h"

#import "GBView.h"
#import "GameRenderer.h"
#include "core/MMU.h"

@interface GameViewController() <MetalViewDelegate>

@property(weak, nonatomic) NSTextField* debugTextField;

@end

@implementation GameViewController {
    GameRenderer* _renderer;
    GBJoypadState _joypad;
    NSString* _romPath;
    dispatch_source_t _renderDispatch;
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

    _joypad = (GBJoypadState) { false, false, false, false, false, false, false, false };

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

    [self addFPSLabel];
}

- (void)addFPSLabel {
    NSTextField* textField = [[NSTextField alloc] initWithFrame:CGRectZero];
    [textField setBezeled:NO];
    [textField setDrawsBackground:NO];
    [textField setEditable:NO];
    [textField setSelectable:NO];
    textField.translatesAutoresizingMaskIntoConstraints = NO;
    [self.view addSubview: textField];
    [NSLayoutConstraint activateConstraints:@[
        [textField.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
        [textField.topAnchor constraintEqualToAnchor:self.view.topAnchor]
    ]];
    self.debugTextField = textField;
    textField.textColor = [NSColor redColor];
    textField.stringValue = @"120 fps";
}

- (void)viewWillAppear {
    [super viewWillAppear];
}

- (void)viewWillDisappear {
    [super viewWillDisappear];
}

- (void)keyUp:(NSEvent *)event {
    [super keyUp: event];

    switch (event.keyCode) {
        case kVK_UpArrow:
            _joypad.upPressed = false;
            break;
        case kVK_DownArrow:
            _joypad.downPressed = false;
            break;
        case kVK_LeftArrow:
            _joypad.leftPressed = false;
            break;
        case kVK_RightArrow:
            _joypad.rightPressed = false;
            break;
        case kVK_ANSI_Z:
            _joypad.aPressed = false;
            break;
        case kVK_ANSI_X:
            _joypad.bPressed = false;
            break;
        case kVK_Return:
            _joypad.startPressed = false;
            break;
        case kVK_Escape:
            _joypad.selectPressed = false;
            break;
    }
}

-(BOOL)acceptsFirstResponder {
    return YES;
}

- (void)keyDown:(NSEvent *)event {
    switch (event.keyCode) {
        case kVK_UpArrow:
            _joypad.upPressed = true;
            break;
        case kVK_DownArrow:
            _joypad.downPressed = true;
            break;
        case kVK_LeftArrow:
            _joypad.leftPressed = true;
            break;
        case kVK_RightArrow:
            _joypad.rightPressed = true;
            break;
        case kVK_ANSI_Z:
            _joypad.aPressed = true;
            break;
        case kVK_ANSI_X:
            _joypad.bPressed = true;
            break;
        case kVK_Return:
            _joypad.startPressed = true;
            break;
        case kVK_Escape:
            _joypad.selectPressed = true;
            break;
        default:
            [super keyDown: event];
            break;
    }
}

- (void)renderToMetalLayer:(nonnull CAMetalLayer *)metalLayer {
    _renderer.joypad = _joypad;
    [_renderer renderToMetalLayer: metalLayer];
    _debugTextField.stringValue = [NSString stringWithFormat:@"%ld FPS", _renderer.frameRate];
}

- (void)drawableResize:(CGSize)size {
    [_renderer drawableResize:size];
}

- (void)dealloc {
    [_renderer disposeRessources];
}

@end