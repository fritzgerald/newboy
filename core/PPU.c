#include "definitions.h"
#include "PPU.h"
#include "MMU.h"
#include "Device.h"
#include <Security/cssmconfig.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include <sys/_types/_u_int32_t.h>
#include "CPU.h"

#define CLOCK_INC 2

void GB_deviceResetPPU(GB_device* ppu);
uint16_t _GB_tileindexWithOffset(GB_device* device, Word offset, bool isWindow);
uint16_t _GB_backgroundTileindexWithOffset(GB_device* device, Word offset);
void GB_RenderProcessFrame(GB_device* device, Byte cycles);
void GB_updateBackgroundPixel(GB_device* device, Byte line, Byte xScan);
void GB_ClearFrame(GB_device* device);
void GB_updateWindowPixel(GB_device* device, Byte line, Byte xScan);

void GB_devicePPUstep(GB_device* device, Byte cycle) {
    GB_ppu *ppu = device->ppu;
    Byte tick = cycle / 4;

    bool sendStatInterrupt = false;
    for (int update = 0; update < tick; update++) {
        switch (ppu->lineMode & 0x3) {
            case GB_PPU_MODE_HBLANK:
                if(ppu->clock >= 204) {
                    // exit from HBlank
                    ppu->clock = 0;
                    ppu->line++;
                    if (ppu->lineCMP == ppu->line && ppu->isLYCInterruptEnabled) {
                        sendStatInterrupt = true;
                    }
                    if(ppu->line == 144) {
                        // Enter vBlank             
                        ppu->lineMode = GB_PPU_MODE_VBLANK;
                        GB_interrupt_request(device, GB_INTERRUPT_FLAG_VBLANK);
                        ppu->frameReady = true;
                    } else {
                        ppu->lineMode = GB_PPU_MODE_OAM_SCAN;
                        if (ppu->isMode2InterruptEnabled) {
                            sendStatInterrupt = true;
                        }
                    }
                } else {
                    u_int32_t newClock = ppu->clock + CLOCK_INC;
                    ppu->clock = newClock;
                }
                break;
            case GB_PPU_MODE_VBLANK:
                if(ppu->clock >= 456) {
                    // exit from HBlank
                    ppu->clock = 0;
                    ppu->line++;
                    if(ppu->line == 154) {
                        // End of vBlank goto OAM scan
                        GB_ClearFrame(device);
                        ppu->lineMode = GB_PPU_MODE_OAM_SCAN;
                        ppu->line = 0;
                        if (ppu->isMode2InterruptEnabled) {
                            sendStatInterrupt = true;
                        }
                    }
                    if (ppu->lineCMP == ppu->line && ppu->isLYCInterruptEnabled) {
                        sendStatInterrupt = true;
                    }
                } else {
                    u_int32_t newClock = ppu->clock + CLOCK_INC;
                    ppu->clock = newClock;
                }
                break;
            case GB_PPU_MODE_OAM_SCAN:
                if(ppu->clock >= 80) {
                    ppu->clock = 0;
                    ppu->lineMode = GB_PPU_MODE_DRAW;
                }  else {
                    u_int32_t newClock = ppu->clock + CLOCK_INC;
                    ppu->clock = newClock;
                }
                break;
            case GB_PPU_MODE_DRAW:
                GB_RenderProcessFrame(device, cycle);
                u_int32_t newClock = ppu->clock + CLOCK_INC;
                ppu->clock = newClock;
        }
        if (sendStatInterrupt) {
            GB_interrupt_request(device, GB_INTERRUPT_FLAG_LCD_STAT);
        }
    }
}

void GB_ClearFrame(GB_device* device) {
    memset(device->ppu->objPriorities, 0, 160 * 144);
    memset(device->ppu->objPalettes0, 0, 160 * 144);
    memset(device->ppu->frameBuffer[GBObjectFrameBuffer], 0, sizeof(GBObjectFrameBuffer) * 160 * 144);
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

        GB_tile_pixel_value pixel_value = GB_tile_pixel_value_from_int((lsb >> bit_shift_length) | ((msb >> bit_shift_length) << 1));
        ppu->tiles[tile_index][row_index][pixel_index] = pixel_value;
    }
}

void GB_ppu_update_control_flags(GB_ppu* ppu) {
    ppu->isBGWinEnabled = GB_tile_bit_value_from_int(ppu->controlBit & 1) == GB_tile_bit_value_0 ? false : true;
    ppu->objEnable = GB_tile_bit_value_from_int(ppu->controlBit & 0x2) == GB_tile_bit_value_0 ? false : true;
    ppu->objSize = GB_tile_bit_value_from_int(ppu->controlBit & 0x4);
    ppu->bgTileArea = GB_tile_bit_value_from_int(ppu->controlBit & 0x8);
    ppu->bgWinTileArea = GB_tile_bit_value_from_int(ppu->controlBit & 0x10);
    ppu->isWindowEnabled = GB_tile_bit_value_from_int(ppu->controlBit & 0x20);
    ppu->windowTileMap = GB_tile_bit_value_from_int(ppu->controlBit & 0x40);
    ppu->LcdPpuEnable = GB_tile_bit_value_from_int(ppu->controlBit & 0x80);
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
        case 0x46:
            // TODO: handle OAM DMA Transfer
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
            break;
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
    switch (value & 0x3) {
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

    memset(ppu->objPriorities, 0, 160 * 144);
    memset(ppu->objPalettes0, 0, 160 * 144);

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

    ppu->bgpIdColors[0] = GBNonCBGColorWhite;
    ppu->bgpIdColors[1] = GBNonCBGColorLightGray;
    ppu->bgpIdColors[2] = GBNonCBGColorDarkGray;
    ppu->bgpIdColors[3] = GBNonCBGColorBlack;

    ppu->objp0IdColor[0] = GBNonCBGColorWhite;
    ppu->objp0IdColor[1] = GBNonCBGColorLightGray;
    ppu->objp0IdColor[2] = GBNonCBGColorDarkGray;
    ppu->objp0IdColor[3] = GBNonCBGColorBlack;

    ppu->objp1IdColor[0] = GBNonCBGColorWhite;
    ppu->objp1IdColor[1] = GBNonCBGColorLightGray;
    ppu->objp1IdColor[2] = GBNonCBGColorDarkGray;
    ppu->objp1IdColor[3] = GBNonCBGColorBlack;

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
    ppu->frameReady = false;
}

unsigned int GB_ppu_getBackgroundPaletteColor(GB_ppu* ppu, GB_tile_pixel_value tileId) {
    // TODO: handle CGB
    if (tileId == 0) {
        tileId = tileId;
    }
    GBNonCBGColors color = ppu->bgpIdColors[tileId & 0x3];
    switch (color) {
        case GBNonCBGColorWhite:
            return 0xFFFFFFFF;
        case GBNonCBGColorLightGray:
            return 0x606060FF;
        case GBNonCBGColorDarkGray:
            return 0x202020FF;
        case GBNonCBGColorBlack:
            return 0x000000FF;
        default:
            return 0xFFFFFFFF; // white by default
    }
}

unsigned int GB_ppu_getObjPaletteColor(GB_ppu* ppu, GB_tile_pixel_value tileId, int paletteIndex) {
    // TODO: handle CGB
    GBNonCBGColors *palette = paletteIndex == 0 ? ppu->objp0IdColor : ppu->objp1IdColor;
    GBNonCBGColors color = palette[tileId & 0x3];
    switch (color) {
        case GBNonCBGColorWhite:
            return 0xFFFFFF00;
        case GBNonCBGColorLightGray:
            return 0x606060FF;
        case GBNonCBGColorDarkGray:
            return 0x202020FF;
        case GBNonCBGColorBlack:
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

void setInt(char* tab, int startIdx, int value) {
    tab[startIdx] = value;
    tab[startIdx + 1] = (char) value >> 8;
    tab[startIdx + 2] = (char) (value >> 0x10);
    tab[startIdx + 3] = (char) (value >> 0x18);
}

void setShort(char* tab, int startIdx, short value) {
    tab[startIdx] = value;
    tab[startIdx + 1] = (char)value >> 8;
}

void GB_updateBackgroundPixel(GB_device* device, Byte line, Byte xScan) {
    uint32_t frameIndex = ((uint32_t)line * 160) + xScan;
    if (device->ppu->isBGWinEnabled == false) {
        // window and background disabled set pixel to white
        device->ppu->frameBuffer[GBBackgroundFrameBuffer][frameIndex] = GB_Tile_pixel_0;
        return;
    }
    Word scx = device->ppu->scrollX;
    Word scy = device->ppu->scrollY;

    u_int32_t pixelX = (xScan + scx) % 256;
    u_int32_t pixelY = (line + scy) % 256;
    Byte tilex = pixelX / 8;
    Byte tiley = pixelY / 8;
    uint32 bgPixelOffset = (tiley * 32) + tilex;

    uint16_t tileIndex = _GB_backgroundTileindexWithOffset(device, bgPixelOffset);
    uint32_t column = pixelX % 8;
    uint32_t row  = pixelY % 8;
    GB_tile_pixel_value value = device->ppu->tiles[tileIndex][row][column];
    device->ppu->frameBuffer[GBBackgroundFrameBuffer][frameIndex] = value;
}

void GB_updateWindowPixel(GB_device* device, Byte line, Byte xScan) {
    if (device->ppu->isWindowEnabled == false) {
        return;
    } else if (xScan + 7 < device->ppu->windowX || line < device->ppu->windowY) {
        return;
    }

    if (line == 0) {
        line = line;
    }

    uint32_t frameIndex = ((uint32_t)line * 160) + xScan;
    Word scx = device->ppu->windowX;
    Word scy = device->ppu->windowY;

    u_int32_t pixelX = (xScan  + 7) - scx;
    u_int32_t pixelY = line + scy;
    Byte tilex = pixelX / 8;
    Byte tiley = pixelY / 8;
    uint32 bgPixelOffset = (tiley * 32) + tilex;

    uint16_t tileIndex = _GB_tileindexWithOffset(device, bgPixelOffset, true);
    uint32_t column = pixelX % 8;
    uint32_t row  = pixelY % 8;
    GB_tile_pixel_value value = device->ppu->tiles[tileIndex][row][column];
    device->ppu->frameBuffer[GBBackgroundFrameBuffer][frameIndex] = value;
}

void GB_updateObjectPixel(GB_device* device, Byte line, Byte xScan) {
    uint32_t frameIndex = ((uint32_t)line * 160) + xScan;
    if (device->ppu->objEnable == false) {
        // obj disabled set pixel to transparent
        device->ppu->frameBuffer[GBObjectFrameBuffer][frameIndex] = GB_Tile_pixel_0;
        return;
    }

    Byte objHeight = device->ppu->objSize == GB_tile_bit_value_0 ? 8 : 16;
    
    for (int index = 0; index < 0xA0; index += 4) {
        Byte yPos = device->ppu->oam[index] - 16;
        Byte xPos = device->ppu->oam[index + 1] - 8;

        Byte yBottom = yPos + objHeight;
        Byte xRight = xPos + 8;
        if (yBottom <= line || yPos > line || xRight <= xScan || xPos > xScan) {
            // object outside vertical draw area
            continue;
        }
        Byte tileIndex = device->ppu->oam[index + 2];
        Byte attributes = device->ppu->oam[index + 3];

        uint32_t column = (xScan - xPos) % 8;
        uint32_t row  = (line - yPos) % 8;

        if(objHeight == 16) {
            if (line >= (yPos + 8)) { // obj is 16 pixel long
                if (attributes & 0x40) { // flip y
                    // force selection of first tile
                    tileIndex &= 0xFE;
                } else {
                    // select the second tile 
                    tileIndex |= 0x1;
                }
                
            } else {
                if (attributes & 0x40) { // flip y
                    // select the second tile 
                    tileIndex |= 0x1;
                } else {
                    // force selection of first tile
                    tileIndex &= 0xFE;
                }
            }
        }
        
        if (attributes & 0x20) { // flip x
            column = 7 - column;
        }
        if (attributes & 0x40) { // flip y
            row = 7 - row;
        }
        GB_tile_pixel_value color = device->ppu->tiles[tileIndex][row][column];

        if (color != GB_Tile_pixel_0) {
            device->ppu->objPriorities[frameIndex] = (attributes & 0x80) ? false : true;
            device->ppu->objPalettes0[frameIndex] = (attributes & 0x10) == 0 ? true : false;
            device->ppu->frameBuffer[GBObjectFrameBuffer][frameIndex] = color;
            break;
        }
    }
}

void GB_RenderProcessFrame(GB_device* device, Byte cycles) {
    // Between 172 and 289 dots
    // TODO: Handle draw penalities
    GB_ppu* ppu = device->ppu;
    if(ppu->clock >= 172) {
        ppu->clock = -CLOCK_INC;
        ppu->lineMode = GB_PPU_MODE_HBLANK;
        if (ppu->isMode0InterruptEnabled) {
            GB_interrupt_request(device, GB_INTERRUPT_FLAG_LCD_STAT);
        }
        // printf("finished rendering line %d\n", ppu->line);
        // TODO: Line can be rendered here.
    }
    if(ppu->clock < 160) {
        int fetchToPerform = cycles;
        for (int i = 0; i < fetchToPerform; i++) {
            int scanX = ppu->clock + i;
            if (scanX >= 160) {
                break;
            }
            GB_updateBackgroundPixel(device, ppu->line, scanX);
            GB_updateWindowPixel(device, ppu->line, scanX);
            GB_updateObjectPixel(device, ppu->line, scanX);
            //printf("finished rendering scanX %d\n", scanX);
        }
    }
}

uint16_t _GB_backgroundTileindexWithOffset(GB_device* device, Word offset) {
    return _GB_tileindexWithOffset(device, offset, false);
}

uint16_t _GB_tileindexWithOffset(GB_device* device, Word offset, bool isWindow) {
    Word startAddr;
    if(isWindow == false) {
        startAddr = (device->ppu->bgTileArea == GB_tile_bit_value_0) ? 0x9800 : 0x9C00;
    } else {
        startAddr = (device->ppu->bgTileArea == GB_tile_bit_value_0) ? 0x9C00 : 0x9800;
    }

    if (device->ppu->bgWinTileArea == GB_tile_bit_value_0) {
        uint8_t tileIdxData = GB_deviceReadByte(device, startAddr + offset);
        if (tileIdxData >= 128) {
            return tileIdxData;
        }
        return 256 + tileIdxData;
    }
    return GB_deviceReadByte(device, startAddr + offset);
}

uint8_t* GB_ppu_gen_frame_bitmap(GB_device* device) {
    GB_ppu* ppu = device->ppu;
    int width = 160; // 32 * 8. 32 tiles of 8 pixels
    int height = 144;
    uint32_t size = width * height * 4; //for 32-bit bitmap only

    uint8_t *pixels = malloc(size);

    for (uint16_t i = 0; i < width * height; i++) {
        GB_tile_pixel_value bgColorId = ppu->frameBuffer[GBBackgroundFrameBuffer][i];
        GB_tile_pixel_value objColorId = ppu->frameBuffer[GBObjectFrameBuffer][i];
        bool objPriority = ppu->objPriorities[i];
        int palette = ppu->objPalettes0[i] ? 0 : 1;

        uint32_t color;
        if (objColorId != GB_Tile_pixel_0 && (objPriority ||  bgColorId == GB_Tile_pixel_0)) {
            color = GB_ppu_getObjPaletteColor(device->ppu, objColorId, palette);
        } else {
            color = GB_ppu_getBackgroundPaletteColor(device->ppu, bgColorId);
        }
        int p = i * 4;
        pixels[p + 0] = (color >> 8) & 0xFF; //blue
        pixels[p + 1] = (color >> 16) & 0xFF; //green
        pixels[p + 2] = (color >> 24) & 0xFF; //red
        pixels[p + 3] = color & 0xFF; //Alpha
    }
    return pixels;
}


uint8_t* GB_ppu_gen_background_bitmap(GB_device* device) {
    int width = 256; // 32 * 8. 32 tiles of 8 pixels
    int height = 256;
    uint32_t size = width * height * 4; //for 32-bit bitmap only

    uint8_t *pixels = malloc(size);

    // here we ignore ppu->isBGWinEnabled
    Word bgAddrLen = 0x400;
    for (uint16_t i = 0; i < bgAddrLen; i++) {
        int16_t tileIndex = _GB_tileindexWithOffset(device, i, false);

        int tileWidth = 8;
        int tileHeight = 8;
        uint32_t startBGx = (i % 32) * 8 * 4;
        uint32_t startBGy = (i / 32) * 256 * 8 * 4;
        int bgWidth = 256 * 4;

        for(int row = 0; row < tileHeight; row++) {
            for(int column = 0; column < tileWidth; column++) {            
                GB_tile_pixel_value value = device->ppu->tiles[tileIndex][row][column];
                unsigned int color = GB_ppu_getBackgroundPaletteColor(device->ppu, value);
                //color = 0x000000FF;

                int p = startBGx + (column * 4) + (row * bgWidth) + startBGy;
                // convert RGB to BGR
                pixels[p + 0] = (color >> 8) & 0xFF;
                pixels[p + 1] = (color >> 16) & 0xFF;
                pixels[p + 2] = (color >> 24) & 0xFF;
                pixels[p + 3] = color & 0xFF; //Alpha
            }
        }
    }
    return pixels;
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