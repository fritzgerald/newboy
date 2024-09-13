#import "GBAudioClient.h"
#include "core/definitions.h"
#include <Foundation/NSObjCRuntime.h>
#import <Foundation/Foundation.h>
#include <sys/_types/_u_int32_t.h>
#import <AudioToolbox/AudioToolbox.h>
#import "core/APU.h"
#import "core/Device.h" 

OSStatus render(GBAudioClient *self,
                AudioUnitRenderActionFlags *ioActionFlags,
                const AudioTimeStamp *inTimeStamp,
                UInt32 inBusNumber,
                UInt32 inNumberFrames,
                AudioBufferList *ioData);

void _GBOnSampleReadyBack(void* sender, GB_device *device, GBSample sample);


@interface GBAudioClient()
    
-(void) onSampleBlockReady:(GB_device *)device  sample:(GBSample) sample;

-(void) onRenderBlockRequest:(UInt32)sampleRate  nFrame:(UInt32) nFrames buffer: (GBSample *) buffer;

@end

@implementation GBAudioClient {
    AudioComponentInstance audioUnit;
    GBSample _audioBuffer[96000];
    u_int32_t _audioBufferPosition;
    NSCondition* _lock;
}

-(id)initWithSampleRate:(NSInteger)rate andDevice:(GB_device*) device {
    self = [super init];

    _lock = [[NSCondition alloc] init];

    AudioComponentDescription defaultOutputDescription;
    defaultOutputDescription.componentType = kAudioUnitType_Output;
    defaultOutputDescription.componentSubType = kAudioUnitSubType_DefaultOutput;
    defaultOutputDescription.componentManufacturer = kAudioUnitManufacturer_Apple;
    defaultOutputDescription.componentFlags = 0;
    defaultOutputDescription.componentFlagsMask = 0;

        // Get the default playback output unit
    AudioComponent defaultOutput = AudioComponentFindNext(NULL, &defaultOutputDescription);
    if (!defaultOutput) {
        NSLog(@"Can't find default output");
        return nil;
    }

    // Create a new unit based on this that we'll use for output
    OSErr err = AudioComponentInstanceNew(defaultOutput, &audioUnit);
    if (!audioUnit) {
        NSLog(@"Error creating unit: %hd", err);
        return nil;
    }

    // Set our tone rendering function on the unit

    AURenderCallbackStruct input;
    input.inputProc = (void*)render;
    input.inputProcRefCon = (__bridge void *)(self);
    err = AudioUnitSetProperty(audioUnit,
                               kAudioUnitProperty_SetRenderCallback,
                               kAudioUnitScope_Input,
                               0,
                               &input,
                               sizeof(input));
    if (err) {
        NSLog(@"Error setting callback: %hd", err);
        return nil;
    }

    AudioStreamBasicDescription streamFormat;
    streamFormat.mSampleRate = rate;
    streamFormat.mFormatID = kAudioFormatLinearPCM;
    streamFormat.mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked | kAudioFormatFlagsNativeEndian;
    streamFormat.mBytesPerPacket = 4;
    streamFormat.mFramesPerPacket = 1;
    streamFormat.mBytesPerFrame = 4;
    streamFormat.mChannelsPerFrame = 2;
    streamFormat.mBitsPerChannel = 8 * 2;
    err = AudioUnitSetProperty (audioUnit,
                                kAudioUnitProperty_StreamFormat,
                                kAudioUnitScope_Input,
                                0,
                                &streamFormat,
                                sizeof(AudioStreamBasicDescription));
    
    if (err) {
        NSLog(@"Error setting stream format: %hd", err);
        return nil;
    }
    err = AudioUnitInitialize(audioUnit);
    if (err) {
        NSLog(@"Error initializing unit: %hd", err);
        return nil;
    }

    self.sampleRate = rate;
    device->apu->sampleRate = 96000;
    GBApuSetSampleReadyCallback(device, _GBOnSampleReadyBack, (__bridge void *)(self));
    //
    [self start];

    return self;
}

-(void) onRenderBlockRequest:(UInt32)sampleRate  nFrame:(UInt32) nFrames buffer: (GBSample *) buffer {
    [_lock lock];
    if (_audioBufferPosition < nFrames) {
        [_lock waitUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.33]];
    }

    if (_audioBufferPosition < nFrames) {
        // Not enough audio
        memset(buffer, 0, (nFrames - _audioBufferPosition) * sizeof(*buffer));
        memcpy(buffer, _audioBuffer, _audioBufferPosition * sizeof(*buffer));
        // Do not reset the audio position to avoid more underflows
    } else {
        memcpy(buffer, _audioBuffer + (_audioBufferPosition - nFrames), nFrames * sizeof(*buffer));
        _audioBufferPosition = 0;
    }
    
    [_lock unlock];
}

-(void) onSampleBlockReady:(GB_device *)device  sample:(GBSample) sample {
    [_lock lock];
    if(_audioBufferPosition >= (96000)) {
        [_lock unlock];
        return; // buffer full
    }
    _audioBuffer[_audioBufferPosition++] = sample;
    [_lock unlock];
}

-(void) start
{
    OSErr err = AudioOutputUnitStart(audioUnit);
    if (err) {
        NSLog(@"Error starting unit: %hd", err);
        return;
    }

}

OSStatus render(
                    GBAudioClient *self,
                    AudioUnitRenderActionFlags *ioActionFlags,
                    const AudioTimeStamp *inTimeStamp,
                    UInt32 inBusNumber,
                    UInt32 inNumberFrames,
                    AudioBufferList *ioData)

{
    GBSample *buffer = (GBSample *)ioData->mBuffers[0].mData;
    [self onRenderBlockRequest:self.sampleRate nFrame:inNumberFrames buffer:buffer];
    return noErr;
}

void _GBOnSampleReadyBack(void* sender, GB_device *device, GBSample sample) {
    GBAudioClient* audioClient = (__bridge GBAudioClient *)sender;
    [audioClient onSampleBlockReady: device sample: sample];
}

@end
