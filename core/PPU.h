#include "definitions.h"
#include <stdbool.h>

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

GBNonCBGColors GBNonCBGColors_value_from_int(int);

typedef struct {
    Byte vRam[0x2000];
    Byte oam[0xA0];
    GB_tile_pixel_value tiles[384][8][8];
    unsigned short clock;
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

    GBNonCBGColors bgpIdColors[4];
    GBNonCBGColors objp0IdColor[4];
    GBNonCBGColors objp1IdColor[4];

    Byte vramBankIndex;

    bool isLYCInterruptEnabled;
    bool isMode0InterruptEnabled;
    bool isMode1InterruptEnabled;
    bool isMode2InterruptEnabled;

    //  LCD control bits
    bool isLCDEnabled;
    GB_tile_bit_value windowTileMap;
    bool isWindowEnabled;
    GB_tile_bit_value bgWinTileArea;
    GB_tile_bit_value bgTileArea;
    GB_tile_bit_value objSize;
    GB_tile_bit_value objEnable;
    GB_tile_bit_value isBGWinEnabled;
    Byte controlBit; // storage value to ease reads
    Byte dmaValue;
} GB_ppu;

void GB_ppu_reset(GB_ppu* ppu);
void GB_ppu_step(GB_ppu* ppu, Byte cycle);
Byte GB_ppu_vRam_read(GB_ppu*, Word addr);
void GB_ppu_vRam_write(GB_ppu*, Word addr, Byte data);
void GB_ppu_IO_write(GB_ppu* ppu, Word addr, Byte data);
Byte GB_ppu_IO_read(GB_ppu* ppu, Word addr);
void GB_ppu_gen_tile_bitmap(GB_ppu* ppu, int tileIndex);