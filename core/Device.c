#include "Device.h"
#include "definitions.h"
#include "CPU.h"
#include "PPU.h"
#include "MMU.h"
#include "APU.h"
#include <stdlib.h>

GB_device* GB_newDevice() {
    GB_device* device = malloc(sizeof(GB_device));
    if (device == NULL) {
        return NULL;
    }

    GB_cpu* cpu = malloc(sizeof(GB_cpu));
    if (cpu == NULL) {
        free(device);
        return NULL;
    }
    GB_mmu* mmu = malloc(sizeof(GB_mmu));
    if (mmu == NULL) {
        free(device);
        free(cpu);
        return NULL;
    }
    GB_ppu* ppu = malloc(sizeof(GB_ppu));
    if (ppu == NULL) {
        free(device);
        free(cpu);
        free(mmu);
        return NULL;
    }

    GBApu* apu = malloc(sizeof(GBApu));
    if (apu == NULL) {
        free(device);
        free(cpu);
        free(mmu);
        free(ppu);
        return NULL;
    }
    device->cpu = cpu;
    device->mmu = mmu;
    device->ppu = ppu;
    device->apu = apu;

    GB_reset(device);

    return device;
}

void GB_freeDevice(GB_device* device) {
    free(device->cpu);
    free(device->mmu);
    free(device->ppu);
    free(device);
}

void GB_reset(GB_device* device) {
    GB_deviceResetMMU(device);
    GB_deviceResetPPU(device);
    GB_deviceCpuReset(device);
}