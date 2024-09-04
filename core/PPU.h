#include "definitions.h"
#include <stdbool.h>
#include <stdint.h>
#include <sys/_types/_u_int32_t.h>

#pragma once

typedef enum { 
    GB_PPU_MODE_HBLANK, 
    GB_PPU_MODE_VBLANK, 
    GB_PPU_MODE_OAM_SCAN, 
    GB_PPU_MODE_DRAW 
} GB_ppu_mode;

typedef enum {
    GB_Tile_pixel_0,
    GB_Tile_pixel_1,
    GB_Tile_pixel_2,
    GB_Tile_pixel_3
} GB_tile_pixel_value;

GB_tile_pixel_value GB_tile_pixel_value_from_int(int);

typedef enum {
    GB_tile_bit_value_0,
    GB_tile_bit_value_1
} GB_tile_bit_value;

GB_tile_bit_value GB_tile_bit_value_from_int(int);

typedef enum {
    GBNonCBGColorWhite,
    GBNonCBGColorLightGray,
    GBNonCBGColorDarkGray,
    GBNonCBGColorBlack
} GBNonCBGColors;

typedef enum {
    GBBackgroundFrameBuffer,
    GBObjectFrameBuffer
} GBFrameBufferIndex;

GBNonCBGColors GBNonCBGColors_value_from_int(int);

struct GB_ppu_s {
    u_int32_t clock;
    GB_ppu_mode lineMode;
    // LY: current line being handled
    Byte line;
    // LYC: compare value for LY to trigger STAT interrupt
    Byte lineCMP;
    // SCY: Background Viewport y position
    Byte scrollY;
    // SCX: Background Viewport x position
    Byte scrollX;
    // SCY: Window y position
    Byte windowY;
    // SCX: Window x position
    Byte windowX;

    Byte vramBankIndex;

    bool isLYCInterruptEnabled;
    bool isMode0InterruptEnabled;
    bool isMode1InterruptEnabled;
    bool isMode2InterruptEnabled;

    bool frameReady;

    //  LCD control bits
    bool isLCDEnabled;
    GB_tile_bit_value windowTileMap;
    bool isWindowEnabled;
    GB_tile_bit_value bgWinTileArea;
    GB_tile_bit_value bgTileArea;
    GB_tile_bit_value objSize;
    bool objEnable;
    bool isBGWinEnabled;
    GB_tile_bit_value LcdPpuEnable;
    Byte controlBit; // storage value to ease reads
    Byte dmaValue;

    GBNonCBGColors bgpIdColors[4];
    GBNonCBGColors objp0IdColor[4];
    GBNonCBGColors objp1IdColor[4];

    Byte vRam[0x2000];
    Byte oam[0xA0];
    GB_tile_pixel_value tiles[384][8][8];
    GB_tile_pixel_value frameBuffer[2][160 * 144];
};

void GB_deviceResetPPU(GB_device* device);
void GB_devicePPUstep(GB_device* device, Byte cycle);
Byte GB_deviceVramRead(GB_device* device, Word addr);
void GB_deviceVramWrite(GB_device* device, Word addr, Byte data);
void GB_devicePPUIOWrite(GB_device* device, Word addr, Byte data);
Byte GB_devicePPUIORead(GB_device* device, Word addr);

// TODO: just for tests. remove later
void GB_ppu_gen_tile_bitmap(GB_ppu* ppu, int tileIndex);
uint8_t* GB_ppu_gen_background_bitmap(GB_device* device);
uint8_t* GB_ppu_gen_frame_bitmap(GB_device* device);