#pragma once

#include "definitions.h"
#include "MMU.h"
#include <stdbool.h>

typedef struct {
    /* 8-bit registers  */
    Byte  a, b, c, d, e, f, h, l;

    /* 16-bit registers*/
    Word pc, sp;
} GB_registers;

typedef struct {
    GB_registers registers;
    GB_mmu memory;
    bool is_halted;

    // Interrupt master enable flag
    bool IME;
    unsigned int divCounter, timaCounter;
} GB_cpu;

void GB_cpu_reset(GB_cpu* cpu);
Byte GB_cpu_step(GB_cpu* cpu);