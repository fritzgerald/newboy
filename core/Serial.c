#include "Serial.h"

#include "Device.h"
#include "MMU.h"
#include "core/definitions.h"
#include <stdlib.h>

const uint16_t clockMask[] = {0x40, 0x02};

int _GBSerialClockMaskIndex(GB_device* device) {
    if (device->isCGB == false) {
        return 0;
    }
    return (device->serialBus->sc & 0x02) != 0 ? 1 : 0;
}

void GBSerialprocessData(GB_device* device) {
    //data ready
    GBSerial* serial = device->serialBus;
    serial->sb = serial->incomingSB;
    serial->sc = serial->sc & 0x7F;
    serial->incomingSB = 0xFF;
    GB_interrupt_request(device, GB_INTERRUPT_FLAG_SERIAL);
}

void GBSerialDataEvent(GB_device* device) {
    //data ready
    GBSerial* serial = device->serialBus;
    if ((serial->sc & 0x80) != 0 && (serial->sc & 0x01) == 1  && serial->onMasterReady != NULL) {
        serial->onMasterReady(device, serial->sb, serial->masterEventInfo);
    }

    GBSerialprocessData(device);
}

void GB_serial_write(GB_device* device, Word addr, Byte value) {
    GBSerial* serial = device->serialBus;
    switch (addr) {
        case 0xFF01:
            device->serialBus->sb = value;
            break;
        case 0xFF02:
            device->serialBus->sc = value;
            break;
        default:
            break;
    }
}

Byte GB_serial_read(GB_device* device, Word addr) {
    switch (addr) {
        case 0xFF01:
            return device->serialBus->sb;
        case 0xFF02:
            return device->serialBus->sc;
        default:
            return 0xFF;
    }
}

void GBSerialUpdate(GB_device* device, Byte cycles) {
    if ((device->serialBus->sc & 0x80) == 0 || (device->serialBus->sc & 0x1) == 0) {
        return;
    }

    uint16_t bitTracked = clockMask[_GBSerialClockMaskIndex(device)];
    Byte mCycles = cycles / 4;
    GBSerial* serial = device->serialBus;
    
    for (int i = 0; i < cycles; i++) {
        // update clock
        uint32_t newClock = device->serialBus->clock + 1;
        uint32_t triggers = device->serialBus->clock & ~newClock;

        device->serialBus->clock = newClock;

        if ((triggers & bitTracked)) {
            GBSerialDataEvent(device);
        }
    }
}

void GB_serial_register_master_event(GB_device* device, GBSerialMasterClock onMasterReady, void* infos) {
    device->serialBus->onMasterReady = onMasterReady;
    device->serialBus->masterEventInfo = infos;
}