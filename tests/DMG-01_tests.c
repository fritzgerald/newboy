
#include "core/Device.h"
#import "core/CPU.h"
#include "core/PPU.h"
#include "core/MMU.h"
#include <stdbool.h>
#include <stdio.h>

int test_cpu_boot_sequence() {
    GB_cpu cpu;
    GB_cpu *p_cpu = &cpu;
    GB_cpu_reset(p_cpu);



    GB_mmu_load(&p_cpu->memory, "/Users/fitji/Documents/workspace/cpu_instrs/source/test03b.gb");
    while (cpu.is_halted == false && cpu.registers.pc != 0x0068) {
        GB_cpu_step(p_cpu);
    }
    for (int i = 0; i < 0x20; i++) {
        GB_ppu_gen_tile_bitmap(&p_cpu->memory.ppu, i);
    }
    printf("ended");
    return 0;
}

int main(int argc, const char * argv[]) {
    test_cpu_boot_sequence();
    return 0;
}
