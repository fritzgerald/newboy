#include "Device.h"
#include "definitions.h"
#include "CPU.h"
#include "PPU.h"
#include "MMU.h"
#include "APU.h"
#include <stdlib.h>
#include <string.h>

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
    memset(apu, 0, sizeof(GBApu));

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

void GB_updateDivCounter(GB_device* device, Byte ticks) {
    GB_cpu* cpu = device->cpu;
    GB_mmu* mmu = device->mmu;
    // update DIV register
    cpu->divCounter += ticks;
    if(cpu->divCounter >= DIV_CLOCK_INC) { // TODO: Handle double speed
        mmu->div++;
    }
    cpu->divCounter %= DIV_CLOCK_INC;
}

void GB_emulationAdvance(GB_device* device, Byte cycles) {
    GB_cpu* cpu = device->cpu;
    GB_mmu* mmu = device->mmu;

    Byte ticks = cycles / 4;

    GB_updateDivCounter(device, ticks);
    GB_update_tima_counter(device, ticks);

    GB_devicePPUstep(device, cycles);
    GBProcessMemEvents(device, cycles);
    GBApuStep(device, cycles);
}