#pragma once

#include "definitions.h"
#include <stdbool.h>

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

struct GB_mmu_s {
    bool in_bios;

    Byte bios[0x100];
    Byte* rom; // TODO: handle multiple rom sizes
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
};

Byte GB_deviceReadByte(GB_device*, Word);
Word GB_deviceReadWord(GB_device*, Word);
void GB_deviceWriteByte(GB_device*, Word, Byte);
void GB_deviceWriteWord(GB_device*, Word, Word);
int  GB_deviceloadRom(GB_device* device, const char* filePath);
void GB_deviceResetMMU(GB_device* device);