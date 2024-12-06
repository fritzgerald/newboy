#include "Serial.h"

#include "Device.h"
#include "Helper.h"
#include "MMU.h"
#include "core/definitions.h"
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

const uint16_t clockMask[] = {0x80, 0x02};

int _GBSerialClockMaskIndex(GB_device* device) {
    if (device->isCGB == false) {
        return 0;
    }
    return (device->serialBus->sc & 0x02) != 0 ? 1 : 0;
}

bool getSerialBit(GB_device* device) {
    GBSerial* serial = device->serialBus;
    if((serial->sc & 0x80) == 0 && (serial->sc & 0x01) == 1) {
        return false;
    }
    return (serial->sb & 0x80) ? true : false;
}

void GBSerialprocessData(GB_device* device) {
    //data ready
    GBSerial* serial = device->serialBus;
    serial->sb = serial->sb << 1;
    serial->sb |= serial->incomingBit;
    serial->bitsToSend++;
    if (serial->bitsToSend == 8) {
        GBprintf("transfer completed\n");
        GBprintf("%s Serial: sb = %02x\n", device->name, serial->sb);
        serial->bitsToSend = 0;
        serial->sc = serial->sc & 0x03;
        GB_interrupt_request(device, GB_INTERRUPT_FLAG_SERIAL);
    }
}

void GBSerialDataEvent(GB_device* device) {
    //data ready
    GBSerial* serial = device->serialBus;
    if ((serial->sc & 0x80) != 0 && (serial->sc & 0x01) == 1  && serial->onMasterReady != NULL) {
        serial->onMasterReady(device, serial->sb, serial->masterEventInfo);
    } else {
        serial->incomingBit = true;
    }

    GBSerialprocessData(device);
}

void GB_serial_write(GB_device* device, Word addr, Byte value) {
    GBSerial* serial = device->serialBus;
    switch (addr) {
        case 0xFF01:
            device->serialBus->sb = value;
            GBprintf("%s Serial: sb = %02x\n", device->name, value);
            break;
        case 0xFF02:
            device->serialBus->sc = value;
            GBprintf("%s Serial: sc = %02x\n", device->name, value);
            if ((value & 0x80) != 0 && (value & 0x01) == 1) {
                device->serialBus->clock = 0;
            }
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
    Byte ticks = cycles / 4;
    GBSerial* serial = device->serialBus;
    
    for (int i = 0; i < ticks; i++) {
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