#pragma once

#include "definitions.h"
#include <stdbool.h>

typedef struct {
    /* 8-bit registers  */
    Byte  a, b, c, d, e, f, h, l;

    /* 16-bit registers*/
    Word pc, sp;
} GB_registers;

struct GB_cpu_s {
    GB_registers registers;
    bool is_halted;

    // Interrupt master enable flag
    bool IME;
    unsigned int divCounter, timaCounter;
};

void GB_deviceCpuReset(GB_device* device);
Byte GB_deviceCpuStep(GB_device* device);