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
    memset(device, 0, sizeof(GB_device));

    GB_cpu* cpu = malloc(sizeof(GB_cpu));
    if (cpu == NULL) {
        free(device);
        return NULL;
    }
    memset(cpu, 0, sizeof(GB_cpu));

    GB_mmu* mmu = malloc(sizeof(GB_mmu));
    if (mmu == NULL) {
        free(device);
        free(cpu);
        return NULL;
    }
    memset(mmu, 0, sizeof(GB_mmu));

    GB_ppu* ppu = malloc(sizeof(GB_ppu));
    if (ppu == NULL) {
        free(device);
        free(cpu);
        free(mmu);
        return NULL;
    }
    memset(ppu, 0, sizeof(GB_ppu));

    GBApu* apu = malloc(sizeof(GBApu));
    if (apu == NULL) {
        free(device);
        free(cpu);
        free(mmu);
        free(ppu);
        return NULL;
    }
    memset(apu, 0, sizeof(GBApu));

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

void GB_updateDivCounter(GB_device* device, Byte cycles) {
    GB_cpu* cpu = device->cpu;
    GB_mmu* mmu = device->mmu;

    // Update TIMA if enabled
    u_int16_t timaMask[] = {0x100, 0x04, 0x10, 0x40};
    u_int16_t bitTracked = timaMask[mmu->timaClockCycles];
    u_int16_t ticks = cycles / 4;
    for (int i = 0; i < ticks; i++) {

        // update DIV register
        u_int32_t newDiv = cpu->divCounter + 1;
        u_int32_t changedBits = cpu->divCounter ^ newDiv;
        cpu->divCounter = newDiv;
        mmu->div = (cpu->divCounter >> 6);

        if (mmu->isTimaEnabled == true && mmu->timaStatus == GBTimaRunning && (changedBits & bitTracked)) {
            mmu->tima++;
            if (mmu->tima == 0) {
                mmu->timaStatus = GBTimaReloading;
            }
        }
    }
}

void GB_emulationStep(GB_device* device) {
    Byte cycles = GB_deviceCpuStep(device);
}

void GB_emulationAdvance(GB_device* device, Byte cycles) {
    GB_cpu* cpu = device->cpu;
    GB_mmu* mmu = device->mmu;

    Byte ticks = cycles / 4;

    GB_update_tima_status(device);
    GB_updateDivCounter(device, cycles);
    GBProcessMemEvents(device, cycles);
    GB_devicePPUstep(device, cycles);
    GBApuStep(device, cycles);
}