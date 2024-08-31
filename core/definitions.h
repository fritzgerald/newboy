#pragma once

#define Byte unsigned char
#define Word unsigned short

#define GB_PC_START      0x100
#define GB_PC_VBLANK_IR  0x40
#define GB_PC_STAT_IR    0x48
#define GB_PC_TIMER_IR   0x50
#define GB_PC_SERIAL_IR  0x58
#define GB_PC_JOYPAD_IR  0x60

#define GB_INTERRUPT_FLAG_VBLANK         0x01
#define GB_INTERRUPT_FLAG_LCD_STAT       0x02
#define GB_INTERRUPT_FLAG_TIMER          0x04
#define GB_INTERRUPT_FLAG_SERIAL         0x08
#define GB_INTERRUPT_FLAG_JOYPAD         0x10

struct GB_mmu_s;
typedef struct GB_mmu_s GB_mmu;

struct GB_ppu_s;
typedef struct GB_ppu_s GB_ppu;

struct GB_cpu_s;
typedef struct GB_cpu_s GB_cpu;

struct GB_device_s;
typedef struct GB_device_s GB_device;