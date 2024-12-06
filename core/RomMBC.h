#pragma once

#include "definitions.h"
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

typedef enum {
    GBMbcNone,
    GBMbc1,
    GBMbc2,
    GBMbcMMM01,
    GBMbc3,
    GBMbc5,
    GBMbc6,
    GBMbc7,
    GBMbcCamera,
    GBMbcBandaiTama5,
    GBMbcHUC1,
    GBMbcHUC3
} GBMbcType;

struct GBRomMBC_s {
    Byte* rom;
    Byte* ram;
    char* filePath;
    Byte cartridgeTypeCode;
    GBMbcType mbcType;
    Word romBankIndex;
    Byte ramBankIndex;
    Byte rom0BankIndex;
    bool isAdvanceBankModeEnabled;
    bool isRamEnabled;
    bool isMbc1M;
    uint32_t romSize;
    uint32_t ramSize;
    Byte rtcRegister;
    time_t startTime;
    bool rtcTrigger;
    Byte rtcValue;
    bool rtcOverflow;
    char title[0x10];
};

GBRomMBC* GBNewRom(const char* filePath);
Byte GBReadFromRom(GB_device* device, GBRomMBC* cartridge, Word addr);
void GBWriteToRom(GB_device* device, GBRomMBC* cartridge, Word addr, Byte value);
Byte GBLoadRomFromFile(GBRomMBC* cartridge, const char* filePath);
void GBRomSave(GBRomMBC* rom);