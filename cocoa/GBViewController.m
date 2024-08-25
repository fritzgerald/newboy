#import "GBViewController.h"
#include <Foundation/Foundation.h>
#include <stdio.h>
#include <stdbool.h>
#include <Foundation/NSObjCRuntime.h>
#include "core/MMU.h"
#include <objc/objc.h>
#include "core/CPU.h"
#include "core/MMU.h"

@implementation GBViewController {
    GB_cpu cpu;
}

-(id)initWithRomFilePath:(NSString *)romFilePath {
    self = [self initWithNibName:nil bundle:nil];
    self.romFilePath = romFilePath;

    return self;
}

- (void)viewDidLoad {
    [super viewDidLoad];

    GB_cpu_reset(&cpu);
    int result = GB_mmu_load(&cpu.memory, _romFilePath.cString);
    if(result == GB_CARTRIDGE_SUCCESS) {
        NSLog(@"loading file %@ succeed", self.romFilePath);
    } else {
        NSLog(@"failed to load file %@", self.romFilePath);
        return;
    }
    
    int strIdx = 0;
    char console[10000];
    while (strIdx < 10000) {
        unsigned char cycles = GB_cpu_step(&cpu);
        GB_ppu_step(&cpu.memory.ppu, cycles);
        
        if(GB_mmu_read_byte(&cpu.memory, 0xFF02) == 0x81) {
            unsigned char sb = GB_mmu_read_byte(&cpu.memory, 0xFF01);
            GB_mmu_write_byte(&cpu.memory, 0xFF02, 0x01);
            console[strIdx] = sb;
            NSLog(@"%@", [NSString stringWithCString:&sb length:1]);
            //printf("%c", sb);
            strIdx++;
        }
    }
    NSLog(@"pause");
}

@end