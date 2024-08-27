#include "definitions.h"
#include "PPU.h"
#include "Device.h"
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <stdbool.h>

void GB_deviceResetPPU(GB_device* ppu);

void GB_devicePPUstep(GB_device* device, Byte cycle) {
    GB_ppu *ppu = device->ppu;
    ppu->clock += cycle;

    switch (ppu->lineMode) {
        case GB_PPU_MODE_HBLANK:
            if(ppu->clock >= 456) {
                // exit from HBlank
                ppu->clock = 0;
                ppu->line++;
                if(ppu->line == 144) {
                    // Enter vBlank
                    ppu->lineMode = GB_PPU_MODE_VBLANK;
                    // here Frame is finished to something to allow rendering
                } else {
                    ppu->lineMode = GB_PPU_MODE_OAM_SCAN;
                }
            }
            break;
        case GB_PPU_MODE_VBLANK:
            if(ppu->clock >= 456) {
                // exit from HBlank
                ppu->clock = 0;
                ppu->line++;
                if(ppu->line == 154) {
                    // End of vBlank goto OAM scan
                    ppu->lineMode = GB_PPU_MODE_OAM_SCAN;
                    ppu->line = 0;
                }
            }
            break;
        case GB_PPU_MODE_OAM_SCAN:
            if(ppu->clock >= 80) {
                ppu->clock = 0;
                ppu->lineMode = GB_PPU_MODE_DRAW;
            }
            break;
        case GB_PPU_MODE_DRAW:
            // Between 172 and 289 dots
            // TODO: Handle draw penalities
            if(ppu->clock >= 172) {
                ppu->clock = 0;
                ppu->lineMode = GB_PPU_MODE_HBLANK;
                // TODO: Line can be rendered here.
            }
            break;
    }
}

Byte GB_deviceVramRead(GB_device* device, Word addr) {
    return device->ppu->vRam[addr & 0x1FFF]; // TODO: handle switch for CGB
}

void GB_deviceVramWrite(GB_device* device, Word addr, Byte data) {
    GB_ppu *ppu = device->ppu;
    Word localAddr = addr & 0x1FFF;
    ppu->vRam[localAddr] = data;

    if(localAddr >= 0x1800) {
        return; // not updating tile data
    }
    // Update tile data on the fly to ease the rendering process
    // For example: `12 & 0xFFFE == 12` and `13 & 0xFFFE == 12`
    Word normalized_index = localAddr & 0xFFFE;

    Byte byte1 = ppu->vRam[normalized_index];
    Byte byte2 = ppu->vRam[normalized_index + 1];

    Word tile_index = normalized_index / 16;
    Word row_index = (normalized_index % 16) / 2;

    for (int pixel_index = 0; pixel_index < 8; pixel_index++) {
        unsigned int bit_shift_length = (7 - pixel_index);
        unsigned int mask = 1 << bit_shift_length;
        unsigned int lsb = byte1 & mask;
        unsigned int msb = byte2 & mask;

        GB_tile_pixel_value pixel_value = GB_tile_pixel_value_from_int((msb >> bit_shift_length) | ((lsb >> bit_shift_length) << 1));
        ppu->tiles[tile_index][row_index][pixel_index] = pixel_value;
    }
}

void GB_ppu_update_control_flags(GB_ppu* ppu) {
    ppu->isBGWinEnabled = GB_tile_bit_value_from_int(ppu->controlBit & (1));
    ppu->objEnable = GB_tile_bit_value_from_int(ppu->controlBit & (1 << 1));
    ppu->objSize = GB_tile_bit_value_from_int(ppu->controlBit & (1 << 2));
    ppu->bgTileArea = GB_tile_bit_value_from_int(ppu->controlBit & (1 << 3));
    ppu->bgWinTileArea = GB_tile_bit_value_from_int( ppu->controlBit & (1 << 4));
    ppu->isWindowEnabled = GB_tile_bit_value_from_int(ppu->controlBit & (1 << 5));
    ppu->windowTileMap = GB_tile_bit_value_from_int(ppu->controlBit & (1 << 6));
    ppu->isBGWinEnabled = GB_tile_bit_value_from_int(ppu->controlBit & (1 << 7));
}

void GB_ppu_update_status_bits(GB_ppu* ppu, Byte data) {
    ppu->isMode0InterruptEnabled = (data & 0x8) ? true : false;
    ppu->isMode1InterruptEnabled = (data & 0x10) ? true : false;
    ppu->isMode2InterruptEnabled = (data & 0x20) ? true : false;
    ppu->isLYCInterruptEnabled = (data & 0x40) ? true : false;
}

Byte GB_ppu_status_value(GB_ppu* ppu) {
    Byte result = (ppu->lineMode & 0x3)
    | ((ppu->line == ppu->lineCMP)? 0x4 : 0)
    | ((ppu->isMode0InterruptEnabled)? 0x8 : 0)
    | ((ppu->isMode1InterruptEnabled)? 0x10 : 0)
    | ((ppu->isMode2InterruptEnabled)? 0x20 : 0)
    | ((ppu->isLYCInterruptEnabled)? 0x40 : 0);

    return result;
}

void GB_devicePPUIOWrite(GB_device* device, Word addr, Byte data) {
    GB_ppu* ppu = device->ppu;
    Word localAddr = addr & 0x4F;
    switch (localAddr) {
        case 0x40:
            ppu->controlBit = data;
            GB_ppu_update_control_flags(ppu);
            break;
        case 0x41:
            GB_ppu_update_status_bits(ppu, data);
            break;
        case 0x42:
            ppu->scrollY = data;
            break;
        case 0x43:
            ppu->scrollX = data;
            break;
        case 0x45:
            ppu->lineCMP = data;
            break;
        case 0x47:
            ppu->bgpIdColors[0] = GBNonCBGColors_value_from_int(data & 0x3);
            ppu->bgpIdColors[1] = GBNonCBGColors_value_from_int((data >> 2) & 0x3);
            ppu->bgpIdColors[2] = GBNonCBGColors_value_from_int((data >> 4) & 0x3);
            ppu->bgpIdColors[3] = GBNonCBGColors_value_from_int((data >> 6) & 0x3);
            break;
        case 0x48:
            ppu->objp0IdColor[0] = GBNonCBGColors_value_from_int(data & 0x3);
            ppu->objp0IdColor[1] = GBNonCBGColors_value_from_int((data >> 2) & 0x3);
            ppu->objp0IdColor[2] = GBNonCBGColors_value_from_int((data >> 4) & 0x3);
            ppu->objp0IdColor[3] = GBNonCBGColors_value_from_int((data >> 6) & 0x3);
            break;
        case 0x49:
            ppu->objp1IdColor[0] = GBNonCBGColors_value_from_int(data & 0x3);
            ppu->objp1IdColor[1] = GBNonCBGColors_value_from_int((data >> 2) & 0x3);
            ppu->objp1IdColor[2] = GBNonCBGColors_value_from_int((data >> 4) & 0x3);
            ppu->objp1IdColor[3] = GBNonCBGColors_value_from_int((data >> 6) & 0x3);
            break;
        case 0x4A:
            ppu->windowY = data;
            break;
        case 0x4B:
            ppu->windowX = data;
            break;
        case 0x4D:
            // TODO: Double-speed mode handling 
            break;
        case 0x4F:
            ppu->vramBankIndex = (data > 0)? 1 : 0;
    }
}

Byte GB_devicePPUIORead(GB_device* device, Word addr) {
    GB_ppu* ppu = device->ppu;
    Word localAddr = addr & 0x4F;
    switch (localAddr) {
        case 0x40:
            return ppu->controlBit;
        case 0x41:
            return GB_ppu_status_value(ppu);
        case 0x42:
            return ppu->scrollY;
        case 0x43:
            return ppu->scrollX;
        case 0x44:
            return ppu->line;
        case 0x45:
            return ppu->lineCMP;
        case 0x46:
            return ppu->dmaValue;
        case 0x47:
            return ppu->bgpIdColors[0] 
            | (ppu->bgpIdColors[1] << 2)
            | (ppu->bgpIdColors[2] << 4)
            | (ppu->bgpIdColors[3] << 6);
        case 0x48:
            return ppu->objp0IdColor[0] 
            | (ppu->objp0IdColor[0] << 2)
            | (ppu->objp0IdColor[1] << 4)
            | (ppu->objp0IdColor[3] << 6);
        case 0x49:
            return ppu->objp1IdColor[0] 
            | (ppu->objp1IdColor[0] << 2)
            | (ppu->objp1IdColor[1] << 4)
            | (ppu->objp1IdColor[3] << 6);
        case 0x4A:
            return ppu->windowY;
        case 0x4B:
            return ppu->windowX;
        case 0x4D:
            // TODO: Double-speed mode handling 
            break;
        case 0x4F:
            return ppu->vramBankIndex;
    }
    return  0;
}

GB_tile_pixel_value GB_tile_pixel_value_from_int(int value) {
    switch (value) {
        case 0:
            return GB_Tile_pixel_0;
        case 1:
            return GB_Tile_pixel_1;
        case 2:
            return GB_Tile_pixel_2;
        case 3:
            return GB_Tile_pixel_3;
        default:
            return GB_Tile_pixel_3;
    }
}

GB_tile_bit_value GB_tile_bit_value_from_int(int value) {
    switch (value) {
        case 0:
            return GB_tile_bit_value_0;
        default:
            return GB_tile_bit_value_1;
    }
}

GBNonCBGColors GBNonCBGColors_value_from_int(int value) {
    switch (value) {
        case 0:
            return GBNonCBGColorWhite;
        case 1:
            return GBNonCBGColorLightGray;
        case 2:
            return GBNonCBGColorDarkGray;
        case 3:
            return GBNonCBGColorBlack;
        default:
            return GBNonCBGColorBlack;
    }
}

void GB_deviceResetPPU(GB_device* device) {
    GB_ppu* ppu = device->ppu;
    memset(ppu->vRam, 0, 0x2000);
    memset(ppu->oam, 0, 0xA0);
    memset(ppu->tiles, 0, 384 * 8 * 8);

    unsigned short clock = 0;
    ppu->lineMode = GB_PPU_MODE_HBLANK;
    ppu->line  = 0;
    ppu->lineCMP  = 0;
    ppu->scrollY  = 0;
    ppu->scrollX  = 0;
    ppu->windowY  = 0;
    ppu->windowX  = 0;

    memset(ppu->bgpIdColors,  0, 4);
    memset(ppu->objp0IdColor, 0, 4);
    memset(ppu->objp1IdColor, 0, 4);

    ppu->vramBankIndex = 0;

    ppu->isLYCInterruptEnabled = false;
    ppu->isMode0InterruptEnabled = false;
    ppu->isMode1InterruptEnabled = false;
    ppu->isMode2InterruptEnabled = false;

    ppu->isLCDEnabled = false;
    ppu->windowTileMap = GB_tile_bit_value_0;
    ppu->isWindowEnabled = false;
    ppu->bgWinTileArea = GB_tile_bit_value_0;
    ppu->bgTileArea = GB_tile_bit_value_0;
    ppu->objSize = GB_tile_bit_value_0;
    ppu->objEnable = GB_tile_bit_value_0;
    ppu->isBGWinEnabled = GB_tile_bit_value_0;
    ppu->controlBit = 0;
    ppu->dmaValue = 0;
}

unsigned int GB_ppu_getBackgroundPaletteColor(GB_ppu* ppu, GB_tile_pixel_value tileId) {
    // TODO: handle CGB
    switch (tileId) {
        case GB_Tile_pixel_0:
            return 0xFFFFFFFF;
        case GB_Tile_pixel_1:
            return 0xD3D3D3FF;
        case GB_Tile_pixel_2:
            return 0xA9A9A9FF;
        case GB_Tile_pixel_3:
            return 0x000000FF;
        default:
            return 0xFFFFFFFF; // white by default
    }
}

unsigned char* GB_ppu_gen_tile_bitmap_data(GB_ppu* ppu, int tileIndex) {
    int width = 8;
    int height = 8;
    int size = 0x100; // 8 * 8 * 4 for 32-bit bitmap only
    unsigned char *pixels = malloc(size);
    for(int row = height - 1; row >= 0; row--) {
        for(int column = 0; column < width; column++) {
            GB_tile_pixel_value value = ppu->tiles[tileIndex][row][column];
            unsigned int color = GB_ppu_getBackgroundPaletteColor(ppu, value);
            int p = ((height - (row + 1)) * width + column) * 4;
            // convert RGB to BGR
            pixels[p + 0] = (color >> 8) & 0xFF; //blue
            pixels[p + 1] = (color >> 16) & 0xFF; //green
            pixels[p + 2] = (color >> 24) & 0xFF; //red
            pixels[p + 3] = color & 0xFF; //Alpha
        }
    }
    return pixels;
}

void GB_ppu_gen_background_bitmap(GB_ppu* ppu, int tileIndex) {

}

void GB_ppu_gen_tile_bitmap(GB_ppu* ppu, int tileIndex) {
    int width = 8;
    int height = 8;
    int size = width * height * 4; //for 32-bit bitmap only

    char header[54] = { 0 };
    strcpy(header, "BM");
    memset(&header[2],  (int)(54 + size), 1);
    memset(&header[10], (int)54, 1);//always 54
    memset(&header[14], (int)40, 1);//always 40
    memset(&header[18], width, 1);
    memset(&header[22], height, 1);
    memset(&header[26], (short)1, 1);
    memset(&header[28], (short)32, 1);//32bit
    memset(&header[34], (int)size, 1);//pixel size

    unsigned char* pixels = GB_ppu_gen_tile_bitmap_data(ppu, tileIndex);

    char *fileName;
    asprintf(&fileName, "bit%d.bmp", tileIndex);
    FILE *fout = fopen(fileName, "wb");
    fwrite(header, 1, 54, fout);
    fwrite(pixels, 1, size, fout);
    free(pixels);
    fclose(fout);
}