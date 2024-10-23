#pragma once

#include "definitions.h"
#include <stdbool.h>
#include <stdint.h>

struct GBRomMBC_s {
    Byte* rom;
    Byte* ram;
    char* filePath;
    Byte cartridgeType;
    Byte romBankIndex;
    Byte ramBankIndex;
    Byte rom0BankIndex;
    bool isAdvanceBankModeEnabled;
    bool isRamEnabled;
    u_int32_t romSize;
    u_int32_t ramSize;
};

GBRomMBC* GBNewRom(const char* filePath);
Byte GBReadFromRom(GB_device* device, GBRomMBC* cartridge, Word addr);
void GBWriteToRom(GB_device* device, GBRomMBC* cartridge, Word addr, Byte value);
Byte GBLoadRomFromFile(GBRomMBC* cartridge, const char* filePath);
void GBRomSave(GBRomMBC* rom);