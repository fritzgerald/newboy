#import "GBViewController.h"
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

@implementation GBViewController {
    GB_device *device;
}

-(id)initWithRomFilePath:(NSString *)romFilePath {
    self = [self initWithNibName:nil bundle:nil];
    self.romFilePath = romFilePath;

    return self;
}

- (void)viewDidLoad {
    [super viewDidLoad];

    device = GB_newDevice();
    GB_reset(device);

    int result = GB_deviceloadRom(device, _romFilePath.cString);
    if(result == GB_CARTRIDGE_SUCCESS) {
        NSLog(@"loading file %@ succeed", self.romFilePath);
    } else {
        NSLog(@"failed to load file %@", self.romFilePath);
        return;
    }
    
    int strIdx = 0;
    char console[100];
    while (true) {
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
            //NSLog(@"%@", [NSString stringWithCString:&sb length:1]);
            //printf("%c", sb);
            
        }
    }
    NSLog(@"pause");
}

@end