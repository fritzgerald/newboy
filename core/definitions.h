#pragma once

#define Byte unsigned char
#define Word unsigned short

#define GB_PC_START      0x100

struct GB_mmu_s;
typedef struct GB_mmu_s GB_mmu;

struct GB_ppu_s;
typedef struct GB_ppu_s GB_ppu;

struct GB_cpu_s;
typedef struct GB_cpu_s GB_cpu;

struct GB_device_s;
typedef struct GB_device_s GB_device;