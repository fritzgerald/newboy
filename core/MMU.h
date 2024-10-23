#pragma once

#include "definitions.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    GBTimaClockCycles256,
    GBTimaClockCycles4,
    GBTimaClockCycles16,
    GBTimaClockCycles64
} GBTimaClockCycles;

typedef enum {
    GBCPUSpeedSingle,
    GBCPUSpeedDouble
} GBCPUSpeed;

typedef enum {
    GBTimaRunning,
    GBTimaReloading,
    GBTimaReloaded
} GBTimaState;

struct GBJoypadState_s {
    bool aPressed;
    bool bPressed;
    bool selectPressed;
    bool startPressed;
    bool rightPressed;
    bool leftPressed;
    bool upPressed;
    bool downPressed;
};

typedef struct GBJoypadState_s GBJoypadState;

typedef Byte (*GBCartrigeReadFunc)(GB_device *device, void* sender, Word addr);
typedef void (*GBCartridgeWriteFunc)(GB_device* device, void* sender, Word addr, Byte value);

struct GBCartridgeDef_s {
    void* sender;
    GBCartrigeReadFunc read;
    GBCartridgeWriteFunc write;
};
typedef struct GBCartridgeDef_s GBCartridgeDef;

struct GB_mmu_s {
    bool in_bios;

    Byte bios[0x100];
    Byte wRam[0x2000];
    Byte zRam[0x80];

    GBCartridgeDef* cartridge;

    Byte sb;
    Byte sc;
    Byte div;
    bool isTimaEnabled;
    GBTimaClockCycles timaClockCycles;
    Byte tima;
    Byte tma;
    Byte tac;
    Byte interruptEnable;
    Byte interruptRequest;
    Byte KEY1;
    Byte romBankIndex;
    Byte ramBankIndex;
    bool useAdvanceBankMode;
    bool ramEnabled;

    GBTimaState timaStatus;
    uint32_t timaCounter;

    bool joypadDpadSelected;
    bool joypadButtonSelected;
    GBJoypadState joypadState;

    //Rom data
    Byte cartridgeType;

    // WIP
    int32_t nextEvent;
 	int32_t period;
 	int remainingBits;

 	uint8_t pendingSB;
};

Byte GB_deviceReadByte(GB_device*, Word);
Word GB_deviceReadWord(GB_device*, Word);
void GB_deviceWriteByte(GB_device*, Word, Byte);
void GB_deviceWriteWord(GB_device*, Word, Word);
void GB_deviceResetMMU(GB_device* device);
void GB_interrupt_request(GB_device* device, Byte ir);
void GBUpdateJoypadState(GB_device* device, GBJoypadState joypad);
int32_t GBProcessMemEvents(GB_device* device, Byte cycles);
void GBLoadBios(GB_device* device);
void GB_emulationLoadCartdrige(GB_device* device, GBCartridgeDef* cartridge);