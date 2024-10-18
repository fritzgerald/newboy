#include "definitions.h"
#include "Device.h"
#include "PPU.h"
#include "MMU.h"
#include "APU.h"
#include "Helper.h"
#include "Bios.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

Byte GB_mmu_read_FF00(GB_mmu* mem, Word addr);
void GB_mmu_write_FF00(GB_mmu* mem, Word addr, Byte value);
Byte _GBJoypadByteRepresentation(GB_mmu* mem);

Byte GB_deviceReadByte(GB_device* device, Word addr) {
    GB_mmu* mem = device->mmu;
    switch (addr & 0xF000) {
        // MARK: ROM bank or BIOS
        case 0x0000:
            if(mem->in_bios) {
                if(addr >= 0x100) {
                    unsigned char txt = mem->rom[addr];
                    return txt;
                }
                return  mem->bios[addr];
            } else {
                return mem->rom[addr];
            }
        case 0x1000: case 0x2000: case 0x3000:
            return mem->rom[addr];
        case 0x4000: case 0x5000: case 0x6000: case 0x7000:
            return mem->rom[addr]; //TODO: handle ROM Bank switch here

        // MARK: VRAM
        case 0x8000: case 0x9000:
            return GB_deviceVramRead(device, addr);
        // MARK: External RAM
        case 0xA000: case 0xB000:
            return mem->eRam[addr & 0x1FFF]; // TODO: handle Switch
        // MARK: Work RAM and echo
        case 0xC000: case 0xD000: case 0xE000:
            return mem->wRam[addr & 0x1FFF];
        case 0xF000:
            switch (addr & 0x0F00) {
                // Echo RAM
                case 0x000: case 0x100: case 0x200: case 0x300:
                case 0x400: case 0x500: case 0x600: case 0x700:
                case 0x800: case 0x900: case 0xA00: case 0xB00:
                case 0xC00: case 0xD00:
                    return mem->wRam[addr & 0x1FFF];
                
                // Graphics: MARK: OAM
                case 0xE00:
                    // OAM is 0xA0 bytes, remaining bytes read as 0
                    if(addr < 0xFEA0) {
                        return device->ppu->oam[addr & 0xFF];
                    }
                    return 0;
                		    // Zero-page
                case 0xF00:
                    if(addr == 0xFFFF) {
                        return mem->interruptEnable;
                    } else if(addr >= 0xFF80) {
			            return mem->zRam[addr & 0x7F];
			        } else if(addr == 0xFF50) {
                        return mem->in_bios;
                    }  else if (addr == 0xFF4D) {
                        return mem->KEY1;
                    } else {
			            // I/O registers
			            // TODO: handle I/O read here
                        switch (addr & 0xF0) {
                            case 0x00:
                                return GB_mmu_read_FF00(mem, addr);
                            case 0x10: case 0x20: case 0x30:
                                return GBReadAPURegister(device, addr);
                            case 0x40:
                                return GB_devicePPUIORead(device, addr);
                        }
			            return 0;
			        }
            }
    }
    return 0;
}

Word GB_deviceReadWord(GB_device* device, Word addr) {
    Byte lower = GB_deviceReadByte(device, addr);
    Word strongByte = GB_deviceReadByte(device, addr+1);
    return  (strongByte << 8) + lower;
}

void GB_device_OAM_DMA(GB_device* device, Byte data) {
    device->ppu->dmaValue = data;
    Word addr = ((Word)data) * 0x100;
    for (int delta = 0; delta < 0xA0; delta++) {
        Word copyAddr = addr + delta;
        device->ppu->oam[delta] = GB_deviceReadByte(device, copyAddr);
    }
}

void GB_deviceWriteByte(GB_device* device, Word addr, Byte value) {
    GB_mmu* mem = device->mmu;
    switch (addr & 0xF000) {
        case 0x0000: case 0x1000: case 0x2000: case 0x3000: case 0x4000:
        case 0x5000: case 0x6000: case 0x7000:
            break; // TODO: Handle MBCs to define behavior
        case 0x8000: case 0x9000:
            GB_deviceVramWrite(device, addr, value);
            break;
        case 0xA000: case 0xB000:
            mem->eRam[addr & 0x1FFF] = value; // TODO: wrong should be handle By MBCs
            break;
        // Work RAM and echo
        case 0xC000: case 0xD000: case 0xE000:
            mem->wRam[addr & 0x1FFF] = value;
	        break;
        case 0xF000:
            switch (addr & 0x0F00) {
                // Echo RAM
                case 0x000: case 0x100: case 0x200: case 0x300:
                case 0x400: case 0x500: case 0x600: case 0x700:
                case 0x800: case 0x900: case 0xA00: case 0xB00:
                case 0xC00: case 0xD00:
                    mem->wRam[addr & 0x1FFF] = value;
                    break;
                
                // Graphics: MARK: OAM
                case 0xE00:
                    // OAM is 0xA0 bytes, remaining bytes read as 0
                    if(addr < 0xFEA0) {
                        device->ppu->oam[addr & 0xFF] = value;
                    }
                    break;
                // Zero-page
                case 0xF00:
                    if(addr == 0xFFFF) { 
                        mem->interruptEnable = value & 0x1F;
                    } else if (addr > 0xFF7F) {
                        mem->zRam[addr & 0x7F] = value;
                    } else if (addr == 0xFF46) {
                        GB_device_OAM_DMA(device, value);
                    } else if (addr == 0xFF50) {
                        mem->in_bios = (value > 0) ? true : false;
                    } else if (addr == 0xFF4D) {
                        mem->KEY1 = value;
                    } else {
                        // TODO: Handle I/O Ranges
                        switch (addr & 0xF0) {
                            case 0x00:
                                GB_mmu_write_FF00(mem, addr, value);
                                break;
                            case 0x10: case 0x20: case 0x30:
                                GBWriteToAPURegister(device, addr, value);
                                break;
                            case 0x40:
                                GB_devicePPUIOWrite(device, addr, value);
                                break;
                        }
                    }
                    break;
            }
    }
}

void GB_deviceWriteWord(GB_device* device, Word addr, Word value) {
    GB_deviceWriteByte(device, addr, value & 0xff);
    GB_deviceWriteByte(device, addr + 1, value >> 8);
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

int GB_deviceloadRom(GB_device* device, const char* filePath) {
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

    device->mmu->rom = (u_int8_t *) malloc(romSize);

    fseek(cartridgeFile, 0, SEEK_SET);
    fread(device->mmu->rom, romSize, 1, cartridgeFile);

    // Handle eRam sizes

    fclose(cartridgeFile);
    return GB_CARTRIDGE_SUCCESS;
}

Byte GB_mmu_read_FF00(GB_mmu* mem, Word addr) {
    int localAddress = addr & 0xFF;
    
    switch (localAddress) {
        case 0x00:
            return _GBJoypadByteRepresentation(mem);
        case 0x01:
            return mem->sb;
        case 0x02:
            return  mem->sc;
        case 0x04:
            return mem->div;
        case 0x05:
            if (mem->timaStatus == GBTimaReloading) {
                return 0;
            }
            return mem->tima;
        case 0x06:
            return mem->tma;
        case 0x07:
            return mem->tac;
        case 0x0F:
            return mem->interruptRequest;
    }
    return 0;
}

GBTimaClockCycles GBTimaClockCyclesFromInt(int value) {
    switch (value & 0x3) {
        case 0:
            return GBTimaClockCycles256;
        case 1:
            return GBTimaClockCycles4;
        case 2:
            return GBTimaClockCycles16;
        case 3:
            return GBTimaClockCycles64;
        default:
            return GBTimaClockCycles4;
    }
}

void GB_mmu_write_FF00(GB_mmu* mem, Word addr, Byte value) {
    int localAddress = addr & 0xFF;
    switch (localAddress) {
        case 0x00:
            mem->joypadDpadSelected = (value & 0x10) ? false : true;
            mem->joypadButtonSelected = (value & 0x20) ? false : true;
            break;
        case 0x01:
            mem->sb = value;
            break;
        case 0x02:
            mem->sc = value;
            mem->period = 0x1000;
            if (value & 0x80) {
                mem->nextEvent = 0x1000;
                // mem->remainingBits = 8;
            }
            break;
        case 0x04:
            mem->div = 0;
            break;
        case 0x05:
            if(mem->timaStatus == GBTimaReloading) {
                mem->tima = value;
                mem->timaStatus = GBTimaRunning;
            } else if (mem->timaStatus == GBTimaRunning) {
                mem->tima = value;
            }
            break;
        case 0x06:
            mem->tma = value;
            if (mem->timaStatus == GBTimaReloaded) {
                mem->tima = value;
            }
            break;
        case 0x07:
            mem->tac = value;
            mem->isTimaEnabled = ((value & 0x4) > 0)? true : false;
            mem->timaClockCycles = GBTimaClockCyclesFromInt(value);
            break;
        case 0x0F:
            mem->interruptRequest = value;
            break;
    }
}

void GB_deviceResetMMU(GB_device* device) {
    GB_mmu* mem = device->mmu;
    mem->in_bios = true;
    memcpy(mem->bios, GBDMGBios, GBDMGBiosLength);
    memset(mem->eRam, 0, 0x2000);
    memset(mem->wRam, 0, 0x2000);
    memset(mem->zRam, 0, 0x80);

    mem->sb = 0xFF;
    mem->sc = 0;
    mem->div = 0;
    mem->isTimaEnabled = false;
    mem->timaClockCycles = GBTimaClockCycles4;
    mem->tima = 0;
    mem->tma = 0;
    mem->tac = 0;
    mem->interruptRequest = 0;
    mem->KEY1 = 0;
    mem->timaCounter = 0;
    mem->joypadState = (GBJoypadState) { false, false, false, false, false, false, false, false };
    mem->pendingSB = 0xFF;
    mem->remainingBits = 8;
}

Byte _GBJoypadByteRepresentation(GB_mmu* mem) {
    if (mem->joypadButtonSelected) {
        return 0x20 |
            (mem->joypadState.startPressed ? 0 : 0x8) |
            (mem->joypadState.selectPressed ? 0 : 0x4)|
            (mem->joypadState.bPressed ? 0 : 0x2) |
            (mem->joypadState.aPressed ? 0 : 0x1);
    } else if (mem->joypadDpadSelected) {
        return 0x10 |
            (mem->joypadState.rightPressed ? 0 : 0x1) |
            (mem->joypadState.leftPressed ? 0 : 0x2)|
            (mem->joypadState.upPressed ? 0 : 0x4) |
            (mem->joypadState.downPressed ? 0 : 0x8);
    }
    return 0x0f; // nothing selected return default value
}

void GBUpdateJoypadState(GB_device* device, GBJoypadState joypad) {
    if (
        joypad.aPressed != device->mmu->joypadState.aPressed ||
        joypad.bPressed != device->mmu->joypadState.bPressed ||
        joypad.startPressed != device->mmu->joypadState.startPressed ||
        joypad.selectPressed != device->mmu->joypadState.selectPressed ||
        joypad.rightPressed != device->mmu->joypadState.rightPressed ||
        joypad.leftPressed != device->mmu->joypadState.leftPressed ||
        joypad.upPressed != device->mmu->joypadState.upPressed ||
        joypad.aPressed != device->mmu->joypadState.aPressed ||
        joypad.downPressed != device->mmu->joypadState.downPressed
        ) {
        device->mmu->joypadState = joypad;
        GB_interrupt_request(device, GB_INTERRUPT_FLAG_JOYPAD);
    }
}

void GB_interrupt_request(GB_device *device, unsigned char ir) {
    device->mmu->interruptRequest = (device->mmu->interruptRequest | ir) & 0x1f;
}

int32_t GBProcessMemEvents(GB_device* device, Byte cycles) {
    if ((device->mmu->sc & 0x80) == 0) {
        return 0;
    }
    if (device->mmu->nextEvent != 2147483647) {
 		device->mmu->nextEvent -= cycles;
 	}

 	if (device->mmu->nextEvent <= 0) {
 		--device->mmu->remainingBits;
 		device->mmu->sb &= ~(8 >> device->mmu->remainingBits);
 		device->mmu->sb |= device->mmu->pendingSB & ~(8 >> device->mmu->remainingBits);
 		if (!device->mmu->remainingBits) {
            GB_interrupt_request(device, GB_INTERRUPT_FLAG_SERIAL);
 			device->mmu->sc = device->mmu->sc & 0x80;
 			device->mmu->nextEvent = 2147483647;
            if (device->mmu->pendingSB == 0xff) {
                device->mmu->pendingSB = 0x01;
                device->mmu->remainingBits = 8;
            }
 		} else {
 			device->mmu->nextEvent += device->mmu->period;
 		}
 	}
 	return device->mmu->nextEvent;
}

//GBJoypadState GBJoypadStateDefault() { return (GBJoypadState) { false, false, false, false, false, false, false, false }; }