#include "APU.h"
#include "MMU.h"
#include "Device.h"
#include "core/definitions.h"
#include <tgmath.h>
#include <stdbool.h>
#include <math.h>
#include <stdint.h>
#include <string.h>
#include <sys/_types/_u_int16_t.h>
#include <sys/_types/_u_int32_t.h>

#define APU_HZ 1048576

bool _GBIsDacOn(GB_device* device, GBSoundChannel channel);
bool _GBIsMasterAudioOn(GB_device* device, GBSoundChannel channel);
void _enableChannelIfPossible(GB_device* device, GBSoundChannel channel, Byte value);
void _extractWaveSample(GB_device* device, bool force);
u_int16_t _GBChannelPeriod(GB_device* device, int NRx3);

void GBWriteToAPURegister(GB_device* device, Word addr, Byte value) {
    GBApu* apu = device->apu;
    Word localAddr = (addr & 0xFF) - 0x10; // start at 0xFF10
    if (localAddr >= 0x30) {
        return; //Out of bounds
    }

    if (device->apu->data[NR52] == 0) {
        //power off make sur we don't write to reg other than NR52
        if (localAddr != NR52 || localAddr < NR30) {
            return;
        }
    }
    switch (localAddr) {
        case NR10:
            device->apu->data[NR10] = value;
            break;
        case NR52:
            device->apu->data[NR52] = value & 0x80;
            if (device->apu->data[NR52] == 0) {
                apu->activeChannels[GBSoundCH1] = false;
                apu->activeChannels[GBSoundCH2] = false;
                apu->activeChannels[GBSoundCH3] = false;
                apu->activeChannels[GBSoundCH4] = false;
                memset(apu->data, 0, 0x20);
            }
            break;
        case NR14:
            _enableChannelIfPossible(device, GBSoundCH1, value);
            device->apu->data[NR14] = value;
            break;
        case NR24:
            _enableChannelIfPossible(device, GBSoundCH2, value);
            device->apu->data[NR24] = value;
            break;
        case NR34:
            _enableChannelIfPossible(device, GBSoundCH3, value);
            device->apu->data[NR34] = value;
            break;
        case NR44:
            _enableChannelIfPossible(device, GBSoundCH4, value);
            device->apu->data[NR44] = value;
            break;
        default:
            device->apu->data[localAddr] = value;
            break;
    }
}

Byte GBReadAPURegister(GB_device* device, Word addr) {
    GBApu* apu = device->apu;
    Word localAddr = (addr & 0xFF) - 0x10;
    if (localAddr >= 0x30) {
        return 0; //Out of bounds
    }
    Byte out;
    switch (localAddr) {
        case NR52:
            out =  0x70 |
                apu->activeChannels[GBSoundCH1] |
                apu->activeChannels[GBSoundCH2] << 1 |
                apu->activeChannels[GBSoundCH3] << 2 |
                apu->activeChannels[GBSoundCH4] << 3 |
                ((device->apu->data[NR52] & 0x80) != 0 ? 0x80 : 0);
            return out;
        case NR10:
            return 0x80 | apu->data[localAddr];
        case NR11: case NR21:
            return 0x3F | apu->data[localAddr];
        case NR13: case NR23:
            return 0xFF;
        case NR14: case NR24: case NR34: case NR44:
            return 0xBF | apu->data[localAddr];
        case NR14 + 1: case NR31: case NR33: case NR34 +1: case NR41:
        case 0x17: case 0x18: case 0x19: case 0x1A: case 0x1B:
        case 0x1C: case 0x1D: case 0x1E: case 0x1F:
            return 0xFF;
        case NR30:
            return 0x7F | apu->data[localAddr];
        case NR32:
            return 0x9F | apu->data[localAddr];
        default:
            return apu->data[localAddr];
    }
}

void _enableChannelIfPossible(GB_device* device, GBSoundChannel channel, Byte value) {
    if ((value & 0x80) && _GBIsDacOn(device, channel)) {
        device->apu->activeChannels[channel] = true;
    }
}

bool _GBIsDacOn(GB_device* device, GBSoundChannel channel) {
    GBApu* apu = device->apu;
    switch (channel) {
        case GBSoundCH1:
            return  (apu->data[NR12] & 0xF8) != 0 ? true : false;
        case GBSoundCH2:
            return  (apu->data[NR22] & 0xF8) != 0 ? true : false;
        case GBSoundCH3:
            return  (apu->data[NR30] & 0x80) != 0 ? true : false;
        case GBSoundCH4:
            return  (apu->data[NR42] & 0xF8) != 0 ? true : false;
        default:
            return  false;
    }
}

bool _GBIsMasterAudioOn(GB_device* device, GBSoundChannel channel) {
    return (device->apu->data[NR52] & 0x80) != 0 ? true : false;
}

int _GBSquareChannelUpPosition(int duty) {
    switch (duty) {
        case 0:
            return 6;
        case 1:
            return 5;
        case 2:
            return 3;
        case 3:
            return 1;
        default:
            return 6;
    }
}

void _extractWaveSample(GB_device* device, bool force) {
    GBApu* apu = device->apu;
    if (force == false && apu->activeChannels[GBSoundCH3] == false) {
        return;
    }

    Byte sound = (apu->data[NR32] & 0x60) >> 5;
    sound = sound & 0x3;

    if(sound == 0) {
        apu->channelValues[GBSoundCH3] = 0; // mute
        return;
    }

    u_int16_t period = _GBChannelPeriod(device, NR33);
 
    if ((apu->clock % period) == 0 || force) {
        int cursor = (apu->clock / period) % 0x20;
        int index = cursor / 2;
        Byte sampleValue = apu->data[0x20 + cursor];
        if (cursor & 1) {
            sampleValue = sampleValue & 0x0F;
            apu->waveReaderCursor = (apu->waveReaderCursor + 1) % 0x10;
        } else {
            sampleValue = sampleValue >> 4;
        }
        if(sound > 1) {
            sampleValue = sampleValue >> sound;
        }
        apu->channelValues[GBSoundCH3] = 300 * sampleValue;
    }
}

void _genCH1SquareWave(GB_device* device) {
    GBApu* apu = device->apu;

    if (false == apu->activeChannels[GBSoundCH1]) {
        apu->channelValues[GBSoundCH1] = 0;
        return;
    }

    u_int16_t period = _GBChannelPeriod(device, NR13);
    double freq = APU_HZ / (double) period / 8.0;

    double sampleLength = APU_HZ / freq;
    double time = apu->clock / freq;

    if ((apu->clock % (int)sampleLength) <= sampleLength / 2) {
        apu->channelValues[GBSoundCH1] = 8000;
    } else {
        apu->channelValues[GBSoundCH1] = -8000;
    }
}

void _genCH2SquareWave(GB_device* device) {
    GBApu* apu = device->apu;

    if (false == apu->activeChannels[GBSoundCH1]) {
        apu->channelValues[GBSoundCH2] = 0;
        return;
    }

    u_int16_t period = _GBChannelPeriod(device, NR23);
    double freq = APU_HZ / (double) period / 8.0;

    double sampleLength = APU_HZ / freq;
    double time = apu->clock / freq;

    if ((apu->clock % (int)sampleLength) <= sampleLength / 2) {
        apu->channelValues[GBSoundCH2] = 8000;
    } else {
        apu->channelValues[GBSoundCH2] = -8000;
    }
}

void _genSquareWaveSample(GB_device* device, double freq) {
    GBApu* apu = device->apu;

    double sampleLength = APU_HZ / freq;
    double time = apu->clock / freq;

    if ((apu->clock % (int)sampleLength) <= sampleLength / 2) {
        apu->channelValues[GBSoundCH3] = 8000;
    } else {
        apu->channelValues[GBSoundCH3] = -8000;
    }
}

void _genSinWaveSample(GB_device* device, double freq) {
    GBApu* apu = device->apu;

    double frq = APU_HZ / freq;
    double time = (apu->clock) / frq;
    double rad = (M_PI * 2 * time);
    double val = sin(rad);
    apu->channelValues[GBSoundCH3] = 16000 * val;
}

void _updateEvents(GB_device* device) {
    GBApu* apu = device->apu;
    if (apu->divBitUp == true || (device->mmu->div & 0x10) == 0) {
        apu->divBitUp = (device->mmu->div & 0x10) ? true : false;
        return;
    }
    // DIV-APU event
    apu->divApu++;

    if ((apu->divApu % 2) == 0) {
        if (apu->activeChannels[GBSoundCH3] && apu->data[NR34] & 0x40) { // length enable
            apu->data[NR31]++;
            if (apu->data[NR31] == 0) {
                apu->activeChannels[GBSoundCH3] = false;
            }
        }
    }
}

u_int16_t _GBChannelPeriod(GB_device* device, int NRx3) {
    u_int16_t period =  (device->apu->data[NRx3 + 1] & 0x07) << 8 | device->apu->data[NRx3];
    period = 2048 - period;
    return period;
}

void _GBWriteChannelPeriod(GB_device* device, int NRx3, u_int16_t value) {
    device->apu->data[NRx3] = value & 0xFF;
    device->apu->data[NRx3 + 1] = (device->apu->data[NRx3 + 1] & 0x40) | ((value >> 8) & 0x07);
}

void GBApuStep(GB_device* device, Byte cycles) {
    Byte ticks = cycles / 4;
    if((device->apu->data[NR52] & 0x80) == 0) { // master audio disabled
        return;
    }

    u_int32_t apuBaseFrequency = 1048576;
    u_int32_t sampleScale = 1;
    GBApu* apu = device->apu;
    if(device->apu->sampleRate != 0) {
        sampleScale = apuBaseFrequency / device->apu->sampleRate;
    }
    
    for (int i = 0; i < ticks; i++) {
        GBSample sample = {0};

        //_extractWaveSample(device, false);
        //_genSinWaveSample(device, 441);
        _genCH1SquareWave(device);
        _genCH2SquareWave(device);
        // _genSquareWaveSample(device, 441);
        _updateEvents(device);

        if (apu->data[NR51] & 0x01) {
            sample.left += apu->channelValues[GBSoundCH1];
        }
        if (apu->data[NR51 & 0x10]) {
            sample.right += apu->channelValues[GBSoundCH1];
        }
        if (apu->data[NR51] & 0x012) {
            sample.left += apu->channelValues[GBSoundCH2];
        }
        if (apu->data[NR51 & 0x20]) {
            sample.right += apu->channelValues[GBSoundCH2];
        }

        if (apu->data[NR51] & 0x40) {
            sample.left += apu->channelValues[GBSoundCH3];
        }
        if (apu->data[NR51 & 0x04]) {
            sample.right += apu->channelValues[GBSoundCH3];
        }

        device->apu->clock++;
        if(device->apu->clock % sampleScale == 0) {
            apu->sampleReadyCallback(apu->sampleReadyCallbackSender ,device, sample);
        }
    }
}

void GBApuSetSampleReadyCallback(GB_device* device, GBApuSampleReady callback, void* sender) {
    device->apu->sampleReadyCallback = callback;
    device->apu->sampleReadyCallbackSender = sender;
}