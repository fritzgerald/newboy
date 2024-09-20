#pragma once

#include "definitions.h"

struct GB_device_s {
    GB_cpu* cpu;
    GB_mmu* mmu;
    GB_ppu* ppu;
    GBApu*  apu;
};

GB_device* GB_newDevice();
void GB_freeDevice(GB_device* device);
void GB_reset(GB_device* device);
void GB_emulationStep(GB_device* device);
void GB_emulationAdvance(GB_device* device, Byte cycles);