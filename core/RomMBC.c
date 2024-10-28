#include "RomMBC.h"
#include "Cartridge.h"
#include "core/definitions.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include "Helper.h"

uint32_t GB_cartridgeRomSize(uint8_t rawRomSize);
uint32_t GB_cartridgeRamSize(uint8_t rawRamSize);
Byte GBLoadRamFromFile(Byte* ram, uint32_t ramSize, const char* filePath);
Byte GBSaveRamForFile(Byte* ram, uint32_t ramSize, const char* filePath);
GBMbcType GBMbcTypeFromCode(Byte cartridgeCode);

Byte GBReadFromMBC1Rom(GB_device* device, GBRomMBC* cartridge, Word addr);
void GBWriteToMBC1Rom(GB_device* device, GBRomMBC* cartridge, Word addr, Byte value);

Byte GBReadFromMBC2Rom(GB_device* device, GBRomMBC* cartridge, Word addr);
void GBWriteToMBC2Rom(GB_device* device, GBRomMBC* cartridge, Word addr, Byte value);

Byte GBReadFromMBC3Rom(GB_device* device, GBRomMBC* cartridge, Word addr);
void GBWriteToMBC3Rom(GB_device* device, GBRomMBC* cartridge, Word addr, Byte value);

Byte GBReadFromRom(GB_device* device, GBRomMBC* cartridge, Word addr) {
        switch (cartridge->mbcType) {
        case GBMbcNone:
        case GBMbc1:
            return GBReadFromMBC1Rom(device, cartridge, addr);
        case GBMbc2:
            return GBReadFromMBC2Rom(device, cartridge, addr);
        case GBMbcMMM01:
        case GBMbc3:
            return GBReadFromMBC3Rom(device, cartridge, addr);
        case GBMbc5:
        case GBMbc6:
        case GBMbc7:
        case GBMbcCamera:
        case GBMbcBandaiTama5:
        case GBMbcHUC1:
        case GBMbcHUC3:
          break;
    }
    return 0xFF;
}

void GBWriteToRom(GB_device* device, GBRomMBC* cartridge, Word addr, Byte value) {
    switch (cartridge->mbcType) {
        case GBMbcNone:
            break; // ignore writes
        case GBMbc1:
            GBWriteToMBC1Rom(device, cartridge, addr, value);
            break;
        case GBMbc2:
            GBWriteToMBC2Rom(device, cartridge, addr, value);
            break;
        case GBMbcMMM01:
        case GBMbc3:
            GBWriteToMBC3Rom(device, cartridge, addr, value);
            break;
        case GBMbc5:
        case GBMbc6:
        case GBMbc7:
        case GBMbcCamera:
        case GBMbcBandaiTama5:
        case GBMbcHUC1:
        case GBMbcHUC3:
          break;
    }
}

Byte GBReadFromMBC1Rom(GB_device* device, GBRomMBC* cartridge, Word addr) {
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

void GBWriteToMBC1Rom(GB_device* device, GBRomMBC* cartridge, Word addr, Byte value) {
    Byte lowMask = cartridge->isMbc1M ? 0x0F : 0x1F;
    Byte highMask = cartridge->isMbc1M ? 0x30 : 0x60;
    Byte highMaskShift = cartridge->isMbc1M ? 4 : 5;
    switch (addr & 0xF000) {
        case 0x0000: case 0x1000: 
            cartridge->isRamEnabled = value == 0x0A ? true : false;
            break;
        case 0x2000: case 0x3000: 
            cartridge->romBankIndex =  (cartridge->romBankIndex & highMask) | (value & lowMask);
            if ((cartridge->romBankIndex & lowMask) == 0) {
                cartridge->romBankIndex++;
            }
            if (cartridge->isAdvanceBankModeEnabled) {
                cartridge->rom0BankIndex = (cartridge->romBankIndex & highMask);
            }
            // GBprintf("b0: %02x, b:%02x\n", cartridge->rom0BankIndex, cartridge->romBankIndex);
            break;
        case 0x4000: case 0x5000: 
            cartridge->ramBankIndex = value & 0x03;
            cartridge->romBankIndex = cartridge->romBankIndex | (value & 0x03) << highMaskShift;
            if (cartridge->isAdvanceBankModeEnabled) {
                cartridge->rom0BankIndex = (value & 0x03) << highMaskShift;
            }
            // GBprintf("b0: %02x, b:%02x\n", cartridge->rom0BankIndex, cartridge->romBankIndex);
            break;
        case 0x6000: case 0x7000:
            cartridge->isAdvanceBankModeEnabled = value & 0x1 ? true : false;
            if (cartridge->isAdvanceBankModeEnabled == false) {
                cartridge->rom0BankIndex = 0;
                // GBprintf("Advance Mode off\n");
            } else {
                cartridge->rom0BankIndex = (cartridge->romBankIndex & highMask);
                // GBprintf("Advance Mode on\n");
            }
            
            // GBprintf("b0: %02x, b:%02x\n", cartridge->rom0BankIndex, cartridge->romBankIndex);
            break;
        case 0xA000: case 0xB000:
            if(cartridge->isRamEnabled && cartridge->ram) {
                cartridge->ram[(0x2000 * cartridge->ramBankIndex) + addr & 0x1FFF] = value; // TODO: wrong should be handle By MBCs
            }
        default:
            break;
    }
}

Byte GBReadFromMBC2Rom(GB_device* device, GBRomMBC* cartridge, Word addr) {
    switch (addr & 0xF000) {
        case 0x0000: case 0x1000: case 0x2000: case 0x3000:
        case 0x4000: case 0x5000: case 0x6000: case 0x7000:
            return GBReadFromMBC1Rom(device, cartridge, addr); // Same has MBC1
        case 0xA000: case 0xB000:
            // Only 512 half Bytes available
            if (cartridge->isRamEnabled && cartridge->ram) {
                return cartridge->ram[addr & 0x1FF];
            }
            return 0xFF;
    }
    return 0xFF;
}

void GBWriteToMBC2Rom(GB_device* device, GBRomMBC* cartridge, Word addr, Byte value) {
    switch (addr & 0xF000) {
        case 0x0000: case 0x1000: 
        case 0x2000: case 0x3000: 
            if ((addr & 0x100) == 0) { // RAM control
                cartridge->isRamEnabled = value == 0x0A ? true : false;
            } else {
                cartridge->romBankIndex =  value & 0x0F;
            }
            break;
        case 0xA000: case 0xB000:
            // Only 512 half Bytes available
            if (cartridge->isRamEnabled && cartridge->ram) {
                cartridge->ram[addr & 0x1FF] = value & 0x0F;
            }
        default:
            break;
    }
}

Byte GBReadFromMBC3Rom(GB_device* device, GBRomMBC* cartridge, Word addr) {
    switch (addr & 0xF000) {
        case 0x0000: case 0x1000: case 0x2000: case 0x3000:
        case 0x4000: case 0x5000: case 0x6000: case 0x7000:
            return GBReadFromMBC1Rom(device, cartridge, addr);
        // MARK: External RAM
        case 0xA000: case 0xB000:
            if (cartridge->isRamEnabled == false) {
                return 0xFF;
            }
            if (cartridge->rtcRegister != 0) {
                return cartridge->rtcValue;
            } else {
                return GBReadFromMBC1Rom(device, cartridge, addr);
            }
    }
    return 0xFF;
}

void GBWriteToMBC3Rom(GB_device* device, GBRomMBC* cartridge, Word addr, Byte value) {
    switch (addr & 0xF000) {
        case 0x0000: case 0x1000: 
            cartridge->isRamEnabled = value == 0x0A ? true : false;
            break;
        case 0x2000: case 0x3000:
            cartridge->romBankIndex = value & 0x7FF;
            break;
        case 0x4000: case 0x5000:
            if (value <= 0x3) {
                cartridge->ramBankIndex = value;
                cartridge->rtcRegister = 0;
            } else if (value >= 0x08 && value <= 0x0C) {
                cartridge->rtcRegister = value;
            }
            break;
        case 0x6000: case 0x7000:
            if (value == 0) {
                cartridge->rtcTrigger = false;
            } else if (value == 1 && cartridge->rtcTrigger == false) {
                if (cartridge->startTime != 0) {
                    // compute rtc value;
                    time_t now = time(NULL);
                    unsigned long secondes = (unsigned long) difftime(now, cartridge->startTime);
                    switch (cartridge->rtcRegister) {
                        case 0x08:
                            cartridge->rtcValue = secondes % 60;
                            break;
                        case 0x09:
                            cartridge->rtcValue = (secondes / 60) % 60;
                            break;
                        case 0x0A:
                            cartridge->rtcValue = (secondes / 3600) % 24;
                            break;
                        case 0x0B:
                            cartridge->rtcValue = (secondes / (3600 * 24));
                            break;
                        case 0x0C:
                            if (cartridge->startTime != 0 && cartridge->rtcOverflow == false) {
                                cartridge->rtcOverflow = (secondes / (3600 * 24)) > 0x1F ? true : false;
                            }
                            cartridge->rtcValue = ((secondes / (3600 * 24)) & 0x10) >> 8 | 
                            (cartridge->startTime != 0) << 6 |
                            cartridge->rtcOverflow << 7;
                            break;
                    }
                } else { // timer not started
                    cartridge->rtcValue = 0;
                }
            }
        case 0xA000: case 0xB000:
            if(cartridge->isRamEnabled) {
                if (cartridge->rtcRegister == 0x0C) {
                    cartridge->startTime = (value & 0x40) ? time(NULL) : 0;
                    if((value & 0x80) == 0) {
                        cartridge->rtcOverflow = false;
                    }
                    
                } else if(cartridge->ram){
                    cartridge->ram[(0x2000 * cartridge->ramBankIndex) + addr & 0x1FFF] = value;
                }
            }
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
    uint8_t rawCartType;
    fread(&rawCartType, 1, 1, cartridgeFile);
    if(fseek(cartridgeFile, GB_CARTRIDGE_ROM_SIZE, SEEK_SET) != 0) {
        fclose(cartridgeFile);
        return GB_CARTRIDGE_FILE_ERROR;
    }

    cartridge->cartridgeTypeCode = rawCartType;
    cartridge->mbcType = GBMbcTypeFromCode(rawCartType);

    uint8_t rawRomSize;
    fread(&rawRomSize, 1, 1, cartridgeFile);
    uint32_t romSize = GB_cartridgeRomSize(rawRomSize);
    if(fseek(cartridgeFile, GB_CARTRIDGE_RAM_SIZE, SEEK_SET) != 0) {
        fclose(cartridgeFile);
        return GB_CARTRIDGE_FILE_ERROR;
    }
    uint8_t rawRamSize;
    fread(&rawRamSize, 1, 1, cartridgeFile);
    uint32_t ramSize = GB_cartridgeRamSize(rawRamSize);

    cartridge->rom = (uint8_t *) malloc(romSize);

    fseek(cartridgeFile, 0, SEEK_SET);
    fread(cartridge->rom, romSize, 1, cartridgeFile);

    // close the file here no longer needed
    fclose(cartridgeFile);

    // Handle eRam sizes
    if(cartridge->mbcType == GBMbc2) {
        ramSize = 512;
    }
    if (ramSize > 0) {
        cartridge->ram = (uint8_t *) malloc(ramSize);
        memset(cartridge->ram, 0, ramSize);
    } else {
        cartridge->ram = NULL;
    }

    // copy file path
    cartridge->filePath = (char *) malloc( strlen(filePath) + 1 );
    strcpy(cartridge->filePath, filePath);

    cartridge->romSize = romSize;
    cartridge->ramSize = ramSize;
    cartridge->isMbc1M = false;

    if (cartridge->mbcType == GBMbc1) {
        // try to detect MBC1M
        // 0x44000 = 10 rom bank + bank 0
        // cf: https://gbdev.io/pandocs/MBC1.html header in bank $10 should contain header
        if (romSize >= 0x44000 && memcmp(cartridge->rom + 104, cartridge->rom + 0x40104, 0x30) == 0) {
            cartridge->isMbc1M = true;
        }
    }

    return GB_CARTRIDGE_SUCCESS;
}

char* GBRAMSavePath(const char* filePath) {
    char* extension = ".dat";
    char* saveFilePath = malloc(strlen(filePath) + strlen(extension) + 1);
    strcat(saveFilePath, filePath);
    strcat(saveFilePath, extension);
    return saveFilePath;
}

Byte GBLoadRamFromFile(Byte* ram, uint32_t ramSize, const char* filePath) {
    char* saveFilePath = GBRAMSavePath(filePath);

    FILE *ramFile = fopen(saveFilePath, "rb");
    if (ramFile == NULL) {
        free(saveFilePath);
        return GB_CARTRIDGE_NOSAVE_ERROR; // no ram file found
    }

    Byte* saveData = (Byte *) malloc(ramSize);
    uint32_t readBytes = fread(ram, ramSize, 1, ramFile);
    if (readBytes == 1) {
        // Make sure file size match RAM size
        memcpy(ram, saveData, readBytes);
    }

    fclose(ramFile);
    free(saveFilePath);
    free(saveData);
    return readBytes == ramSize ? GB_CARTRIDGE_SUCCESS : GB_CARTRIDGE_NOSAVE_ERROR;
}

Byte GBSaveRamForFile(Byte* ram, uint32_t ramSize, const char* filePath) {
    if (ramSize == 0 || ram == NULL) {
        GB_CARTRIDGE_SUCCESS;
    }
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

uint32_t GB_cartridgeRomSize(uint8_t rawRomSize) {
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

uint32_t GB_cartridgeRamSize(uint8_t rawRamSize) {
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

GBMbcType GBMbcTypeFromCode(Byte cartridgeCode) {
    switch (cartridgeCode) {
        case GBCART_NO_MBC:
            return GBMbcNone;
        case GBCART_MBC1: case GBCART_MBC1_ram: case GBCART_MBC1_ram_b:
            return GBMbc1;
        case GBCART_MBC2: case GBCART_MBC2_b:
            return GBMbc2;
        case GBCART_MMM01: case GBCART_MMM01_ram: case GBCART_MMM01_ram_b:
            return GBMbcMMM01;
        case GBCART_MBC3: case GBCART_MBC3_ram: case GBCART_MBC3_ram_b:
        case GBCART_MBC3_t_ram_b: case GBCART_MBC3_t_b:
            return GBMbc3;
        case GBCART_MBC5: case GBCART_MBC5_RAM: case GBCART_MBC5_RAM_B:
        case GBCART_MBC5_RU: case GBCART_MBC5_RU_RAM: case GBCART_MBC5_RU_RAM_B:
            return GBMbc5;
        case GBCART_MBC6:
            return GBMbc6;
        case GBCART_MBC7:
            return GBMbc7;
        case GBCART_CAMERA:
            return GBMbcCamera;
        case GBCART_BANDAI_TAMA5:
            return GBMbcBandaiTama5;
        case GBCART_HuC3:
            return GBMbcHUC3;
        case GBCART_HuC1:
            return GBMbcHUC1;
    }
    return GBMbc1;
}