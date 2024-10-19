#pragma once

#import <Foundation/Foundation.h>
#include <objc/NSObjCRuntime.h>
#include "core/definitions.h"

@interface GBAudioClient : NSObject

@property (assign, nonatomic) NSInteger sampleRate;

-(id)initWithSampleRate:(NSInteger) rate andDevice:(GB_device*) device;

-(void)start;
-(void)stop;

@end