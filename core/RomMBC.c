#include "RomMBC.h"
#include "Cartridge.h"
#include "core/definitions.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

u_int32_t GB_cartridgeRomSize(u_int8_t rawRomSize);
u_int32_t GB_cartridgeRamSize(u_int8_t rawRamSize);
Byte GBLoadRamFromFile(Byte* ram, u_int32_t ramSize, const char* filePath);
Byte GBSaveRamForFile(Byte* ram, u_int32_t ramSize, const char* filePath);

Byte GBReadFromRom(GB_device* device, GBRomMBC* cartridge, Word addr) {
    switch (addr & 0xF000) {
        case 0x0000: case 0x1000: case 0x2000: case 0x3000:
            return cartridge->rom[(0x4000 * cartridge->rom0BankIndex) + (addr & 0x3FFF)];
        case 0x4000: case 0x5000: case 0x6000: case 0x7000:
            return cartridge->rom[(0x4000 * cartridge->romBankIndex) + (addr & 0x3FFF)];
        // MARK: External RAM
        case 0xA000: case 0xB000:
            if (cartridge->isRamEnabled && cartridge->ram) {
                return cartridge->ram[(0x2000 * cartridge->ramBankIndex) + addr & 0x1FFF]; // TODO: handle Switch
            }
            return 0xFF;
    }
    return 0xFF;
}

void GBWriteToRom(GB_device* device, GBRomMBC* cartridge, Word addr, Byte value) {
    switch (addr & 0xF000) {
        case 0x0000: case 0x1000: 
            cartridge->isRamEnabled = value == 0x0A ? true : false;
            break;
        case 0x2000: case 0x3000: 
            cartridge->romBankIndex = (value & 0x1F);
            if (cartridge->romBankIndex == 0) {
                cartridge->romBankIndex++;
            }
            break;
        case 0x4000: case 0x5000: 
            cartridge->ramBankIndex = value & 0x03;
            cartridge->romBankIndex = cartridge->romBankIndex | (value & 0x03) << 5;
            if (cartridge->isAdvanceBankModeEnabled) {
                cartridge->rom0BankIndex = (value & 0x03) << 5;
            }
            break;
        case 0x6000: case 0x7000:
            cartridge->isAdvanceBankModeEnabled = value & 0x1 ? true : false;
            break;
        case 0xA000: case 0xB000:
            if(cartridge->isRamEnabled && cartridge->ram) {
                cartridge->ram[(0x2000 * cartridge->ramBankIndex) + addr & 0x1FFF] = value; // TODO: wrong should be handle By MBCs
            }

        default:
            break;
    }
}

GBRomMBC* GBNewRom(const char* filePath) {
    GBRomMBC* rom = malloc(sizeof(GBRomMBC));
    memset(rom, 0, sizeof(GBRomMBC));

    rom->romBankIndex = 1;
    if (filePath != NULL) {
        if(GBLoadRomFromFile(rom, filePath) == GB_CARTRIDGE_SUCCESS) {
            GBLoadRamFromFile(rom->ram, rom->ramSize, filePath);
        }
    }

    return rom;
}

void GBRomSave(GBRomMBC* rom) {
    GBSaveRamForFile(rom->ram, rom->ramSize, rom->filePath);
}

Byte GBLoadRomFromFile(GBRomMBC *cartridge, const char *filePath) {
    FILE *cartridgeFile = fopen(filePath, "rb");
    if(cartridgeFile == NULL) {
        fclose(cartridgeFile);
        return GB_CARTRIDGE_FILE_ERROR;
    }

    if(fseek(cartridgeFile, GB_CARTRIDGE_NAME, SEEK_SET) != 0) {
        fclose(cartridgeFile);
        return GB_CARTRIDGE_FILE_ERROR;
    }
    char title[0x10];
    fread(title, 1, 0x10, cartridgeFile);
    if(fseek(cartridgeFile, GB_CARTRIDGE_TYPE, SEEK_SET) != 0) {
        fclose(cartridgeFile);
        return GB_CARTRIDGE_FILE_ERROR;
    }
    u_int8_t rawCartType;
    fread(&rawCartType, 1, 1, cartridgeFile);
    if(fseek(cartridgeFile, GB_CARTRIDGE_ROM_SIZE, SEEK_SET) != 0) {
        fclose(cartridgeFile);
        return GB_CARTRIDGE_FILE_ERROR;
    }

    u_int8_t rawRomSize;
    fread(&rawRomSize, 1, 1, cartridgeFile);
    u_int32_t romSize = GB_cartridgeRomSize(rawRomSize);
    if(fseek(cartridgeFile, GB_CARTRIDGE_RAM_SIZE, SEEK_SET) != 0) {
        fclose(cartridgeFile);
        return GB_CARTRIDGE_FILE_ERROR;
    }
    u_int8_t rawRamSize;
    fread(&rawRamSize, 1, 1, cartridgeFile);
    u_int32_t ramSize = GB_cartridgeRamSize(rawRamSize);

    cartridge->rom = (u_int8_t *) malloc(romSize);

    fseek(cartridgeFile, 0, SEEK_SET);
    fread(cartridge->rom, romSize, 1, cartridgeFile);

    // close the file here no longer needed
    fclose(cartridgeFile);

    // Handle eRam sizes
    if (ramSize > 0) {
        cartridge->ram = (u_int8_t *) malloc(ramSize);
        memset(cartridge->ram, 0, ramSize);
    } else {
        cartridge->ram = NULL;
    }

    // copy file path
    cartridge->filePath = (char *) malloc( strlen(filePath) + 1 );
    strcpy(cartridge->filePath, filePath);

    cartridge->cartridgeType = rawCartType;
    cartridge->romSize = romSize;
    cartridge->ramSize = ramSize;

    return GB_CARTRIDGE_SUCCESS;
}

char* GBRAMSavePath(const char* filePath) {
    char* extension = ".dat";
    char* saveFilePath = malloc(strlen(filePath) + strlen(extension) + 1);
    strcat(saveFilePath, filePath);
    strcat(saveFilePath, extension);
    return saveFilePath;
}

Byte GBLoadRamFromFile(Byte* ram, u_int32_t ramSize, const char* filePath) {
    char* saveFilePath = GBRAMSavePath(filePath);

    FILE *ramFile = fopen(saveFilePath, "rb");
    if (ramFile == NULL) {
        free(saveFilePath);
        return GB_CARTRIDGE_NOSAVE_ERROR; // no ram file found
    }

    Byte* saveData = (Byte *) malloc(ramSize);
    u_int32_t readBytes = fread(ram, ramSize, 1, ramFile);
    if (readBytes == 1) {
        // Make sure file size match RAM size
        memcpy(ram, saveData, readBytes);
    }

    fclose(ramFile);
    free(saveFilePath);
    free(saveData);
    return readBytes == ramSize ? GB_CARTRIDGE_SUCCESS : GB_CARTRIDGE_NOSAVE_ERROR;
}

Byte GBSaveRamForFile(Byte* ram, u_int32_t ramSize, const char* filePath) {
    char* saveFilePath = GBRAMSavePath(filePath);
    FILE *ramFile = fopen(saveFilePath, "wb");
    if (ramFile == NULL) {
        free(saveFilePath);
        return GB_CARTRIDGE_FILE_ERROR; // no ram file found
    }

    fwrite(ram, sizeof(Byte), ramSize, ramFile);
    fclose(ramFile);
    free(saveFilePath);
    return GB_CARTRIDGE_SUCCESS;
}

u_int32_t GB_cartridgeRomSize(u_int8_t rawRomSize) {
    switch (rawRomSize)
    {
    case 0:
        return 0x7fff; // 32 Kib
    case 1:
        return 0x7fff * 2; // 64 Kib;
    case 2:
        return 0x7fff * 4; // 128 Kib;
    case 3:
        return 0x7fff * 8; // 256 Kib;
    case 4:
        return 0x7fff * 16; // 512 Kib;
    case 5:
        return 0x7fff * 32; // 1 Mib;
    case 6:
        return 0x7fff * 64; // 2 Mib;
    case 7:
        return 0x7fff * 128; // 4 Mib;
    case 8:
        return 0x7fff * 256; // 8 Mib;
    default:
        return 0x7fff; // 32 Kib
    };
}

u_int32_t GB_cartridgeRamSize(u_int8_t rawRamSize) {
    switch (rawRamSize)
    {
    case 2:
        return 0x2000; // 8Kib
    case 3:
        return 0x8000; // 32Kib
    case 4:
        return 0x20000; // 128Kib
    case 5:
        return 0x10000; // 64Kib
    default:
        return 0; // No RAM
    };
}