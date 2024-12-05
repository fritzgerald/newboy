#pragma once

#include "definitions.h"
#include <stdbool.h>
#include <stdint.h>

typedef void (*GBSerialMasterClock)(GB_device *device, Byte data, void* infos);

struct GBSerial_s {
    Byte sb;
    Byte sc;
    Byte bitsToSend;
    Byte incomingSB;
    uint32_t clock;
    void* masterEventInfo;
    GBSerialMasterClock onMasterReady;
};

void GBSerialprocessData(GB_device* device);
void GBSerialDataEvent(GB_device* device);
void GBSerialUpdate(GB_device* device, Byte cycles);
void GB_serial_write(GB_device* device, Word addr, Byte value);
Byte GB_serial_read(GB_device* device, Word addr);
void GB_serial_register_master_event(GB_device* device, GBSerialMasterClock onMasterReady, void* infos);