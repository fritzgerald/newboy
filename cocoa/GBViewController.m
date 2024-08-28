#import "GBViewController.h"
#include <stdlib.h>
#include <CoreGraphics/CGGeometry.h>
#include <CoreFoundation/CFCGTypes.h>
#include <CoreGraphics/CGImage.h>
#include <stdint.h>
#include <AppKit/AppKit.h>
#import "GBView.h"
#include "core/definitions.h"
#include <Foundation/Foundation.h>
#include <stdio.h>
#include <stdbool.h>
#include <Foundation/NSObjCRuntime.h>
#include "core/MMU.h"
#include <objc/objc.h>
#include "core/CPU.h"
#include "core/MMU.h"
#include "core/Device.h"
#include "core/PPU.h"

@interface GBViewController() <MetalViewDelegate>

@property (weak, nonatomic) NSImageView* imageView;

@end

@implementation GBViewController {
    GB_device *device;
}

-(id)initWithRomFilePath:(NSString *)romFilePath {
    self = [self initWithNibName:nil bundle:nil];
    self.romFilePath = romFilePath;

    return self;
}

// - (void)loadView {
//     GBView* gbView = [[GBView alloc] init];
//     gbView.delegate = self;
//     self.view = gbView;
// }


- (void)viewDidLoad {
    [super viewDidLoad];

    device = GB_newDevice();
    GB_reset(device);

    NSImageView *imgView = [[NSImageView alloc] init];
    imgView.frame = CGRectMake(0, 0, 256, 256);
    imgView.layer.backgroundColor = [NSColor orangeColor].CGColor;
    [self.view addSubview:imgView];
    self.imageView = imgView;

    int result = GB_deviceloadRom(device, _romFilePath.cString);
    if(result == GB_CARTRIDGE_SUCCESS) {
        NSLog(@"loading file %@ succeed", self.romFilePath);
    } else {
        NSLog(@"failed to load file %@", self.romFilePath);
        return;
    }

    [NSTimer scheduledTimerWithTimeInterval:1.0/60.0
        target:self
        selector:@selector(renderFrame)
        userInfo:nil
        repeats:YES];
    //[self renderFrame];
}

-(void)renderFrame {
    int strIdx = 0;
    char console[100];
    while ((device->mmu->interruptRequest & GB_INTERRUPT_FLAG_VBLANK) == 0) {
        unsigned char cycles = GB_deviceCpuStep(device);
        GB_devicePPUstep(device, cycles);
        
        if(GB_deviceReadByte(device, 0xFF02) == 0x81) {
            unsigned char sb = GB_deviceReadByte(device, 0xFF01);
            GB_deviceWriteByte(device, 0xFF02, 0x01);
            console[strIdx] = sb;
            strIdx++;
            if(sb == '\n' || strIdx == 99) {
                NSLog(@"%@", [NSString stringWithCString:console length:strIdx]);
                strIdx = 0;
            }
        }
    }
    // TODO: Render Frame
    // Frame done
    device->mmu->interruptRequest &= ~(GB_INTERRUPT_FLAG_VBLANK);
    uint8_t* data =  GB_ppu_gen_background_bitmap(device);
    NSBitmapImageRep* img = [[NSBitmapImageRep alloc] 
        initWithBitmapDataPlanes: &data 
        pixelsWide:256 
        pixelsHigh:256 
        bitsPerSample:8 
        samplesPerPixel: 4 
        hasAlpha:YES isPlanar:NO
        colorSpaceName:NSDeviceRGBColorSpace 
        bitmapFormat:NSBitmapFormatThirtyTwoBitLittleEndian 
        bytesPerRow:256 * 4
        bitsPerPixel:32];
    
    CGImageRef cgImg = [img CGImage];
    NSImage *nsImg = [[NSImage alloc] initWithCGImage:cgImg size: NSMakeSize(256, 256)];
    [self.imageView setImage:nsImg];
    free(data);
}

- (void)drawableResize:(CGSize)size {
}

- (void)renderToMetalLayer:(nonnull CAMetalLayer *)metalLayer {


}

-(void)dealloc {
    GB_freeDevice(device);
}

@end