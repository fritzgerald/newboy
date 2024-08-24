#pragma once

#include "definitions.h"
#include <stdbool.h>
#include "PPU.h"

#define GB_CARTRIDGE_SUCCESS    0
#define GB_CARTRIDGE_FILE_ERROR -1

#define GB_CARTRIDGE_NAME     0x0134
#define GB_CARTRIDGE_TYPE     0x0147
#define GB_CARTRIDGE_ROM_SIZE 0x0148
#define GB_CARTRIDGE_RAM_SIZE 0x0149

typedef enum {
    GBTimaClockCycles4,
    GBTimaClockCycles16,
    GBTimaClockCycles64,
    GBTimaClockCycles256
} GBTimaClockCycles;

typedef enum {
    GBCPUSpeedSingle,
    GBCPUSpeedDouble
} GBCPUSpeed;

typedef struct {
    bool in_bios;

    Byte bios[0x100];
    Byte* rom; // TODO: handle multiple rom sizes
    GB_ppu ppu;
    Byte eRam[0x2000];
    Byte wRam[0x2000];
    Byte zRam[0x80];

    Byte sb;
    Byte sc;
    Byte div;
    bool isTimaEnabled;
    GBTimaClockCycles timaClockCycles;
    Byte tima;
    Byte tma;
    Byte tac;
    Byte interruptRequest;
    Byte KEY1;
} GB_mmu;

Byte GB_mmu_read_byte(GB_mmu*, Word);
Word GB_mmu_read_word(GB_mmu*, Word);
void GB_mmu_write_byte(GB_mmu*, Word, Byte);
void GB_mmu_write_word(GB_mmu*, Word, Word);
int GB_mmu_load(GB_mmu* mem, const char* filePath);
void GB_mmu_reset(GB_mmu* mem);