#include "APU.h"
#include "MMU.h"
#include "CPU.h"
#include "Device.h"
#include "core/definitions.h"
#include <cups/cups.h>
#include <stdio.h>
#include <sys/_types/_u_int16_t.h>
#include <sys/_types/_u_int32_t.h>
#include <tgmath.h>
#include <stdbool.h>
#include <math.h>
#include <stdint.h>
#include <string.h>
#include <stdint.h>

//#define APU_HZ 1048576
#define APU_HZ 2097152

bool _GBIsDacOn(GB_device* device, GBSoundChannel channel);
bool _GBIsMasterAudioOn(GB_device* device, GBSoundChannel channel);
void _enableChannelIfPossible(GB_device* device, GBSoundChannel channel, Byte value, int regStart, Word lengMax);
void _extractWaveSample(GB_device* device, bool force);
u_int16_t _GBChannelPeriod(GB_device* device, int NRx3);
void _GBWriteChannelPeriod(GB_device* device, int NRx3, u_int16_t value);
void _disableChannelIfOff(GB_device* device, GBSoundChannel channel);
void _GB_updateLengthTimer(GB_device* device, GBSoundChannel channel, int regStart, Word max);
void _handleLenTrigger(GB_device* device, GBSoundChannel channel, Byte newValue, Byte oldValue, int regStart, Word lengMax);
void _triggerCh1Sweep(GB_device* device, bool checkOnly);
void _ch1SweepNegateExitTrigger(GB_device* device, Byte newValue, Byte oldValue);
void _ch1SweepTrigger(GB_device* device, GBSoundChannel channel, Byte newValue, Byte oldValue);
void _ch1SweepUpdate(GB_device* device);
void GBApuReset(GB_device* device);
void GBApuDMGDown(GB_device* device);
void _GB_update_LFSR(GB_device* device);
void _GB_gen_noise_wave(GB_device* device);
void _triggerCh4(GB_device* device, Byte value);
void _GBCh3Trigger(GB_device* device, Byte value, Byte oldValue);

void GBWriteToAPURegister(GB_device* device, Word addr, Byte value) {
    GBApu* apu = device->apu;
    Word localAddr = (addr & 0xFF) - 0x10; // start at 0xFF10
    if (localAddr >= 0x30) {
        return; //Out of bounds
    }

    printf("write to %4x value %2x\n", addr, value);
    if (device->apu->data[NR52] == 0) {
        //power off make sur we don't write to reg other than NR52
        if (localAddr != NR52) {
            switch (localAddr) {
                case NR11:case NR21:case NR41:
                    apu->channelLen[localAddr / 0x05] = value & 0x3F;
                    break;
                case NR31:
                    apu->channelLen[localAddr / 0x05] = value; 
                    break;
                default:
                    printf("Ignoring write to %4x value %2x\n", addr, value);
                    break;
            }
            return;
        }
    }
    if (localAddr >= 0x20) {
        apu->data[localAddr] = value;
        return;
    }

    Byte oldValue;

    switch (localAddr) {
        case NR10:
            oldValue = device->apu->data[localAddr];
            device->apu->data[localAddr] = value;
            _ch1SweepNegateExitTrigger(device, value, oldValue);
            break;
        case NR30:
            device->apu->data[localAddr] = value;
            _disableChannelIfOff(device, localAddr / 0x05);
            break;
        case NR11:case NR21:case NR41:
            device->apu->data[localAddr] = value;
            apu->channelLen[localAddr / 0x05] = device->apu->data[localAddr] & 0x3F;
            break;
        case NR31:
            device->apu->data[localAddr] = value;
            apu->channelLen[localAddr / 0x05] = device->apu->data[localAddr]; 
            break;
        case NR52:
            device->apu->data[NR52] = value & 0x80;
            if (device->apu->data[NR52] == 0) {
                GBApuDMGDown(device);
                // TODO: on CGB call APU_RESET instead
                // GBApuReset(device);
            }
            break;
        case NR12: case NR22: case NR42:
            device->apu->data[localAddr] = value;
            apu->envelopeVolume[localAddr / 0x05] = (value >> 4);
            _disableChannelIfOff(device, localAddr / 0x05);
            break;
        case NR14:
            oldValue = apu->data[NR14];
            _enableChannelIfPossible(device, GBSoundCH1, value, NR10, 0x40);
            _ch1SweepTrigger(device, GBSoundCH1, value, oldValue);
            break;
        case NR24:
            _enableChannelIfPossible(device, GBSoundCH2, value, NR20, 0x40);
            break;
        case NR33:
            device->apu->data[localAddr] = value;
            break;
        case NR34:
            oldValue = device->apu->data[localAddr];
            _GBCh3Trigger(device, value, oldValue);
            _enableChannelIfPossible(device, GBSoundCH3, value, NR30, 0x100);
            break;
        case NR44:
            _enableChannelIfPossible(device, GBSoundCH4, value, NR40, 0x40);
            _triggerCh4(device, value);
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
    if (localAddr >= 0x20) {
        // wave read
        if (apu->activeChannels[GBSoundCH3] == false) {
            return apu->data[localAddr];
        } else if (apu->waveReadclock == device->cpu->divCounter) {
            return apu->waveValue;
        }
        return 0xFF;
    }
    Byte out;
    switch (localAddr) {
        case NR52:
            out =  0x70 |
                (apu->activeChannels[GBSoundCH1] ? 1 : 0) |
                (apu->activeChannels[GBSoundCH2] ? 2 : 0) |
                (apu->activeChannels[GBSoundCH3] ? 4 : 0) |
                (apu->activeChannels[GBSoundCH4] ? 8 : 0) |
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

void _handleLenTrigger(GB_device* device, GBSoundChannel channel, Byte newValue, Byte oldValue, int regStart, Word lengMax) {
    if (((oldValue & 0x40) == 0 && newValue & 0x40)) {
        if (device->apu->channelLen[channel] != 0 || (oldValue & 0x80)) {
            if ((device->apu->divApu & 0x01) == 1) {
                _GB_updateLengthTimer(device, channel, regStart, lengMax);
            }
        }
    }
    if ((newValue & 0x80) && device->apu->channelLen[channel] == 0) {
        if ((device->apu->divApu & 0x01) == 1) {
            _GB_updateLengthTimer(device, channel, regStart, lengMax);
        }
    }
}

u_int32_t triggerSinceReset = 0;

void _GBCh3Trigger(GB_device* device, Byte value, Byte oldValue)  {
    bool dmgCorruptEnabled = true;
    if (value & 0x80) { // triggered
        if (dmgCorruptEnabled == true &&
            device->apu->activeChannels[GBSoundCH3] == true &&
            device->apu->channelClockDelay[GBSoundCH3] == 0) {
            Byte idx = (device->apu->waveReaderCursor + 1);
            Byte offset = (idx >> 1) & 0xF;

            // Source: Sameboy apu.c https://github.com/LIJI32/SameBoy/blob/master/Core/apu.c#L634
            /* This glitch varies between models and even specific instances:
                DMG-B:     Most of them behave as emulated. A few behave differently.
                SGB:       As far as I know, all tested instances behave as emulated.
                MGB, SGB2: Most instances behave non-deterministically, a few behave as emulated.
                
                For DMG-B emulation I emulate the most common behavior, which blargg's tests expect (not my own DMG-B, which fails it)
                For MGB emulation, I emulate my Game Boy Light, which happens to be deterministic.

                Additionally, I believe DMGs, including those we behave differently than emulated,
                are all deterministic. */
            if (offset < 4) {
                device->apu->data[APU_WAVE_START] = device->apu->data[APU_WAVE_START + offset];
            } 
            else {
                Byte delta = (offset & ~3);
                memcpy(device->apu->data + APU_WAVE_START,
                       device->apu->data + APU_WAVE_START + delta,
                       4);
            }
        }
        device->apu->waveReaderCursor = 0;
        
        // TODO: refactor
        u_int16_t period = _GBChannelPeriod(device, NR33);
        u_int32_t periodValue = 2048 - period;
        device->apu->channelClockDelay[GBSoundCH3] = periodValue + 2;
    }
}

void _ch1SweepTrigger(GB_device* device, GBSoundChannel channel, Byte newValue, Byte oldValue) {
    if (newValue == 0xC0) {
        newValue = newValue;
    }
    Byte step = device->apu->data[NR10] & 0x7;
    Byte pace = (device->apu->data[NR10] >> 4) & 0x7;
    u_int16_t period = _GBChannelPeriod(device, NR13);

    if (newValue & 0x80) { // triggered
        triggerSinceReset = 0;
        device->apu->ch1SweepEnabled = (pace != 0 || step != 0) ? true : false;
        device->apu->periodSweepTimer = (pace != 0) ? pace : 8;
        device->apu->periodOnTrigger = period;
        if (step != 0) {
            printf("triggered sweep %02x; c: %8x; period = 0x%04x;\n", newValue, device->cpu->divCounter, period);
            _triggerCh1Sweep(device, (device->apu->data[NR10] & 0x8) != 0);
        }
    }
}

void _ch1SweepUpdate(GB_device* device) {
    if (device->apu->ch1SweepEnabled == false || false == device->apu->activeChannels[GBSoundCH1]) {
        return;
    }
    
    GBApu* apu = device->apu;
    Byte pace = (device->apu->data[NR10] >> 4) & 0x7;
    Byte step = apu->data[NR10] & 0x7;

    apu->periodSweepTimer--;
    printf("periode sweep: %d\n", apu->periodSweepTimer);
    if (apu->periodSweepTimer == 0) {
        if (pace == 0) {
            apu->periodSweepTimer = 8;
        } else {
            apu->periodSweepTimer = pace;
            if (step != 0 || apu->ch1StepZero == false) {
                _triggerCh1Sweep(device, false);
                apu->ch1StepZero = step == 0;
            } else {
                device->apu->ch1NegModeUsed = (device->apu->data[NR10] & 0x8) == 0 ? false : true;
            }
        }        
    }
}

void _triggerCh1Sweep(GB_device* device, bool checkOnly) {
    GBApu* apu = device->apu;
    Byte step = apu->data[NR10] & 0x7;
    Byte pace = (device->apu->data[NR10] >> 4) & 0x7;
    
    u_int16_t periodOnTrigger = apu->periodOnTrigger;
    device->apu->ch1NegModeUsed = (device->apu->data[NR10] & 0x8) == 0 ? false : true;

    u_int16_t delta = periodOnTrigger >> step;
    if ((apu->data[NR10] & 0x8) != 0) {
        // uses two's complement for substraction
        delta = ~delta + 1;
    }
    
    u_int16_t newP = periodOnTrigger + delta;
    
    printf("pace = %d; step = %d \n", pace, step);
    printf("triggers : %d; period = 0x%04x; c: %8x\n", triggerSinceReset, newP, device->cpu->divCounter);
    if (newP > 0x7FF && (apu->data[NR10] & 0x8) == 0) {
        apu->activeChannels[GBSoundCH1] = false;
        printf("Channel disabled\n");
    } else if (checkOnly == false) {
        apu->periodOnTrigger = (newP & 0x7FF);
        _GBWriteChannelPeriod(device, NR13, newP);
        triggerSinceReset++;
        printf("Freq updated\n");
    }
}

void _enableChannelIfPossible(GB_device* device, GBSoundChannel channel, Byte value, int regStart, Word lengMax) {
    Byte oldValue = device->apu->data[regStart + 4];
    device->apu->data[regStart + 4] = value;
    _handleLenTrigger(device, channel, value, oldValue, regStart, lengMax);

    if ((value & 0x80) && _GBIsDacOn(device, channel)) {
        printf("channel %02d enabled; value: %02x; old: %02x\n", channel + 1, value, oldValue);
        device->apu->activeChannels[channel] = true;
        device->apu->channelClock[channel] = 0;
        device->apu->channelSweepPace[channel] = device->apu->data[regStart + 2] & 0x7;
    }
}

void _disableChannelIfOff(GB_device* device, GBSoundChannel channel) {
    if (_GBIsDacOn(device, channel) == false) {
        device->apu->activeChannels[channel] = false;
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
 
    if (device->apu->channelClockDelay[GBSoundCH3] == 0) {
        apu->waveReaderCursor = (apu->waveReaderCursor + 1) % 32;
        int index = apu->waveReaderCursor / 2;
        Byte sampleValue = apu->data[APU_WAVE_START + index];
        apu->waveValue = sampleValue;

        if ((apu->waveReaderCursor & 1) == 1) {
            sampleValue = sampleValue & 0x0F;
        } else {
            sampleValue = sampleValue >> 4;
        }
        
        if(sound > 1) {
            sampleValue = sampleValue >> (sound - 1);
        } else if(sound == 0) {
            sampleValue = 0;
        }

        apu->channelValues[GBSoundCH3] = sampleValue;
        apu->waveReadclock = device->cpu->divCounter;
        // TODO: refactor
        u_int16_t period = _GBChannelPeriod(device, NR33);
        u_int32_t periodValue = 2048 - period; 
        apu->channelClockDelay[GBSoundCH3] = periodValue - 1;
    } else {
        device->apu->channelClockDelay[GBSoundCH3]--;
        apu->waveReadclock = 0;
    }
}

u_int16_t _channelFrequency(GB_device *device, GBSoundChannel channel, int regStart) {
    u_int16_t period = _GBChannelPeriod(device, regStart + 3);
    return 131072 / (2048 - period);
}

u_int16_t _GBChannelPeriod(GB_device* device, int NRx3) {
    u_int16_t period =  (device->apu->data[NRx3 + 1] & 0x07) << 8 | device->apu->data[NRx3];
    return period;
}

void _genChannelSquareWave(GB_device* device, GBSoundChannel channel, int registerStart) {
    GBApu* apu = device->apu;

    if (false == apu->activeChannels[channel]) {
        apu->channelValues[channel] = 0;
        return;
    }

    u_int16_t period = _GBChannelPeriod(device, registerStart + 3);
    u_int16_t chFreq = _channelFrequency(device, channel, registerStart);
    
    double volume = (double) apu->envelopeVolume[channel] / 0xF;
    double freq = (double) chFreq;

    double sampleLength = APU_HZ /(double)freq;
    
    double divider = 2;
    double advanceScale = 0;
    switch (apu->data[registerStart + 1] & 0xC0) {
        case 0x00:
            divider = 8;
            advanceScale = 0;
            break;
        case 0x40:
            divider = 4;
            advanceScale = 3.0 / 4.0;
            break;
        case 0x80:
            divider = 2;
            advanceScale = 3.0 / 4.0;
            break;
        case 0xC0:
            divider = 1.0 + (1.0 / 3.0);
            advanceScale = 0;
            break;
    }

    double downTime = (sampleLength / divider);
    u_int32_t advance = sampleLength * advanceScale;
    if (((apu->channelClock[channel] + advance) % (int)sampleLength) <= (sampleLength - downTime)) {
        apu->channelValues[channel] = volume * 0xF;
    } else {
        apu->channelValues[channel] = volume * -0xF;
    }
}

void _GB_gen_noise_wave(GB_device* device) {
    GBApu* apu = device->apu;

    if (false == apu->activeChannels[GBSoundCH4]) {
        apu->channelValues[GBSoundCH4] = 0;
        return;
    }

    Byte lfsrDivider = apu->data[NR43] & 0x7;
    Byte lfsrShift = (apu->data[NR43] >> 4) & 0xF;
    double divider = (double) lfsrDivider;
    if (lfsrDivider == 0) {
        divider = 0.5;
    }

    u_int32_t chFreq = (0x40000 / divider) / (2 << lfsrShift);

    double freq = (double) chFreq;

    double sampleLength = APU_HZ /(double)freq;
    double time = apu->clock / (double)freq;

    if (apu->channelClock[GBSoundCH4] % (int)sampleLength == 0) {
        _GB_update_LFSR(device);
        double volume = (double) apu->envelopeVolume[GBSoundCH4] / 0xF;
        device->apu->channelValues[GBSoundCH4] = (apu->lfsrState & 0x1) * volume * 0xF;
    }
}

void _genSquareWaveSample(GB_device* device, double freq) {
    GBApu* apu = device->apu;

    double sampleLength = APU_HZ / freq;
    double time = apu->clock / freq;

    if ((apu->clock % (int)sampleLength) <= sampleLength / 2) {
        apu->channelValues[GBSoundCH3] = 1;
    } else {
        apu->channelValues[GBSoundCH3] = 0;
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

void _GB_updateLengthTimer(GB_device* device, GBSoundChannel channel, int regStart, Word max) {
    GBApu* apu = device->apu;
    if (apu->data[regStart + 4] & 0x40) {
        apu->channelLen[channel] = (apu->channelLen[channel] + 1) % max;
        printf("channel %d. len = %d\n", channel + 1,apu->channelLen[channel]);
        if (apu->channelLen[channel] == 0) {
            apu->activeChannels[channel] = false;
        }
    }
}

void _gb_update_envelop_pace(GB_device* device, GBSoundChannel channel, int regStart) {
    GBApu* apu = device->apu;
    if (device->apu->activeChannels[channel] == false) {
        return;
    }

    if (apu->envelopeSweepTimer[channel] == 0) {
        Byte pace = apu->data[regStart + 2] & 0x7;
        if (pace == 0) {
            return;
        }
        apu->envelopeSweepTimer[channel] = pace;
    } else {
        apu->envelopeSweepTimer[channel]--;
    }
    if (apu->envelopeSweepTimer[channel] == 0 && apu->envelopeVolume[channel] > 0) {
        apu->envelopeVolume[channel]--;
    }
}

void GBApuDiv(GB_device* device) {
    GBApu* apu = device->apu;
    if ((apu->data[NR52] & 0x80) == 0) {
        return; // APU off
    }

    apu->divApu++;

    if ((apu->divApu & 0x01) == 1) {
        _GB_updateLengthTimer(device, GBSoundCH1, NR10, 0x40);
        _GB_updateLengthTimer(device, GBSoundCH2, NR20, 0x40);
        _GB_updateLengthTimer(device, GBSoundCH3, NR30, 0x100);
        _GB_updateLengthTimer(device, GBSoundCH4, NR40, 0x40);
    }

    if ((apu->divApu & 0x3) == 0x3) { //CH1 sweep
        _ch1SweepUpdate(device);
    }

    if ((apu->divApu & 0x7) == 0x7) { //CH1 sweep
        _gb_update_envelop_pace(device, GBSoundCH1, NR10);
        _gb_update_envelop_pace(device, GBSoundCH2, NR20);
        _gb_update_envelop_pace(device, GBSoundCH3, NR40);
    }

    for (int ch = 0; ch < GBSoundChannelCount; ch++) {
        if (_GBIsDacOn(device, ch) == false) {
            apu->activeChannels[ch] = false;
        }
    }
}

void _triggerCh4(GB_device* device, Byte value) {
    if ((value & 0x80) == 0 || _GBIsDacOn(device, GBSoundCH4)) {
        return;
    }
    GBApu *apu = device->apu;
    apu->lfsrState = 0;
}

void _GBWriteChannelPeriod(GB_device* device, int NRx3, u_int16_t value) {
    device->apu->data[NRx3] = value & 0xFF;
    device->apu->data[NRx3 + 1] = (device->apu->data[NRx3 + 1] & 0xF8) | ((value >> 8) & 0x07);
}

GBSample highPass(GB_device *device, GBSample sample) {
    GBApu *apu = device->apu;
    double cutoff_frequency = 1.0;
    double gain = 0.5;//cutoff_frequency / (2 * M_PI * device->apu->sampleRate);

    GBSample output = { 
        (sample.left) + gain * (apu->lastSampleOutput.left - sample.left),
        (sample.right) + gain * (apu->lastSampleOutput.right - sample.right) 
    };
    apu->lastSample = sample;
    apu->lastSampleOutput = output;
    return output;
}

void GBApuStep(GB_device* device, Byte cycles) {
    Byte ticks = cycles / 2;

    if((device->apu->data[NR52] & 0x80) == 0) { // master audio disabled
        return;
    }

    u_int32_t cyclesPerSample = 1;
    GBApu* apu = device->apu;
    if(device->apu->sampleRate != 0) {
        cyclesPerSample = APU_HZ / device->apu->sampleRate;
    }
    
    for (int i = 0; i < ticks; i++) {
        device->apu->clock++;
        GBSample sample = {0};

        _genChannelSquareWave(device, GBSoundCH1, NR10);
        _genChannelSquareWave(device, GBSoundCH2, NR20);
        _extractWaveSample(device, false);
        _GB_gen_noise_wave(device);

        for (int ch = 0; ch < GBSoundChannelCount; ch++) {
            //int nrStart = NR10 + (ch * 5);

            if (false == apu->activeChannels[ch]) {
                continue;
            }
            if (apu->data[NR51] & (0x01 << ch)) {
                sample.left += apu->channelValues[ch];
            }
            if (apu->data[NR51] & (0x10 << ch)) { 
                sample.right += apu->channelValues[ch];
            }
            if (apu->activeChannels[ch]) {
                device->apu->channelClock[ch] += 1;
            }
        }

        sample.left *= 0x100;
        sample.right *= 0x100;

        if(device->apu->clock % cyclesPerSample == 0) {
            GBSample out = highPass(device, sample);
            apu->sampleReadyCallback(apu->sampleReadyCallbackSender ,device, out);
        }
    }
}

void GBApuSetSampleReadyCallback(GB_device* device, GBApuSampleReady callback, void* sender) {
    device->apu->sampleReadyCallback = callback;
    device->apu->sampleReadyCallbackSender = sender;
}

void _ch1SweepNegateExitTrigger(GB_device* device, Byte newValue, Byte oldValue) {
    if ((newValue & 0x8) != 0 || (oldValue & 0x8) == 0) {
        device->apu->ch1NegModeUsed = false;
        return;
    }
    if ((device->apu->divApu % 0x3) == 0x0 || device->apu->ch1NegModeUsed == true) {
        printf("exit negate mode disabled: %2x\n", newValue);
        device->apu->activeChannels[GBSoundCH1] = false;
    }
    device->apu->ch1NegModeUsed = false;
}

void GBApuDMGDown(GB_device* device) {
    GBApu* apu = device->apu;
    apu->clock = 0;
    apu->sampleRate = 0;
    apu->periodOnTrigger = 0;
    apu->ch1SweepEnabled = false;
    apu->ch1NegModeUsed = false;
    apu->divApu = 0;
    apu->waveReaderCursor = 0;

    apu->periodSweepTimer = 0;
    memset(apu->envelopeSweepTimer, 0, GBSoundChannelCount);
    memset(apu->envelopeVolume, 0, GBSoundChannelCount);
    memset(apu->activeChannels, false, GBSoundChannelCount);
    memset(apu->channelValues, 0, GBSoundChannelCount);
    // memset(apu->channelClock, 0, GBSoundChannelCount);
    // memset(apu->channelLen, 0, GBSoundChannelCount);
    memset(apu->data, 0, 0x20);
}

void GBApuReset(GB_device* device) {
    GBApu* apu = device->apu;
    apu->clock = 0;
    apu->sampleRate = 0;
    apu->periodOnTrigger = 0;
    apu->ch1SweepEnabled = false;
    apu->ch1NegModeUsed = false;
    apu->divApu = 0;
    apu->waveReaderCursor = 0;

    apu->periodSweepTimer = 0;
    memset(apu->envelopeSweepTimer, 0, GBSoundChannelCount);
    memset(apu->envelopeVolume, 0, GBSoundChannelCount);
    memset(apu->activeChannels, false, GBSoundChannelCount);
    memset(apu->channelValues, 0, GBSoundChannelCount);
    memset(apu->channelClock, 0, GBSoundChannelCount);
    memset(apu->channelLen, 0, GBSoundChannelCount);
    memset(apu->data, 0, 0x20);
}

void _GB_update_LFSR(GB_device* device) {
    if (false == device->apu->activeChannels[GBSoundCH4]) {
        return;
    }
    GBApu* apu = device->apu;

    u_int16_t lfsr = apu->lfsrState;

    u_int16_t bit = (lfsr & 0x1) == ((lfsr >> 1) & 0x1);
    lfsr = (lfsr & 0x7FFF) | (bit << 15);
    if ((apu->data[NR43] & 0x8) != 0) {
        lfsr = (lfsr & 0xFF7F) | (bit << 7);
    }
    lfsr = lfsr >> 1;

    apu->lfsrState = lfsr;
}