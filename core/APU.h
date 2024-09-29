#pragma once

#include "definitions.h"
#include <stdbool.h>
#include <stdint.h>
#include <sys/_types/_u_int16_t.h>
#include <sys/_types/_u_int32_t.h>

#define NR10 0x00
#define NR11 0x01
#define NR12 0x02
#define NR13 0x03
#define NR14 0x04

#define NR20 0x5
#define NR21 0x6
#define NR22 0x7
#define NR23 0x8
#define NR24 0x9

#define NR30 0xA
#define NR31 0xB
#define NR32 0xC
#define NR33 0xD
#define NR34 0xE

#define NR40 0x0F
#define NR41 0x10
#define NR42 0x11
#define NR43 0x12
#define NR44 0x13

#define NR50 0x14
#define NR51 0x15
#define NR52 0x16

typedef enum {
    GBSoundPaddingNone,
    GBSoundPaddingRight,
    GBSoundPaddingLeft,
    GBSoundPaddingCenter
} GBSoundPadding;

typedef enum {
    GBSoundCH1,
    GBSoundCH2,
    GBSoundCH3,
    GBSoundCH4,
    GBSoundChannelCount
} GBSoundChannel;

struct GBSample_s {
    int16_t left;
    int16_t right;
};

typedef void (*GBApuSampleReady)(void* sender, GB_device *device, GBSample sample);

typedef enum {
    GBSweepAddition,
    GBSweepSubtraction
} GBSweepDirection;

typedef enum {
    GBEnvDirectionDecrease,
    GBEnvDirectionIncrease
} GBEnvDirection;

struct GBAPU_s {
    u_int32_t clock;
    u_int32_t sampleRate;
    u_int16_t periodDelta;
    bool ch1SweepEnabled;
    bool ch1SweepStop;
    Byte divApu;
    Byte waveReaderCursor;
    bool waveUpperRead;
    GBSample output;
    GBApuSampleReady sampleReadyCallback;
    void* sampleReadyCallbackSender;
    bool divBitUp;
    u_int8_t periodSweepTimer;
    u_int16_t envelopeSweepTimer[GBSoundChannelCount];
    Byte envelopeVolume[GBSoundChannelCount];
    bool activeChannels[GBSoundChannelCount];
    short channelValues[GBSoundChannelCount];
    u_int32_t channelClock[GBSoundChannelCount];
    Byte channelLen[GBSoundChannelCount];
    Byte data[0x30];
};

void GBWriteToAPURegister(GB_device* device, Word addr, Byte value);
Byte GBReadAPURegister(GB_device* device, Word addr);
void GBApuStep(GB_device* device, Byte cycles);
void GBApuSetSampleReadyCallback(GB_device* device, GBApuSampleReady callback, void* sender);
void GBApuDiv(GB_device* device);