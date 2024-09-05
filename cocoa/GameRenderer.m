#import "GameRenderer.h"
#include <Foundation/NSObjCRuntime.h>
#include <stdbool.h>
#include <CoreFoundation/CFCGTypes.h>
#include <objc/objc.h>
#include <CoreGraphics/CGGeometry.h>
#include <CoreGraphics/CGImage.h>
#include <AppKit/AppKit.h>
#include <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import "GBShaderTypes.h"
#include "core/Device.h"
#include "core/CPU.h"
#include "core/MMU.h"
#include "core/PPU.h"

@implementation GameRenderer
{
    // renderer global ivars
    id <MTLDevice>              _device;
    id <MTLCommandQueue>        _commandQueue;
    id <MTLRenderPipelineState> _pipelineState;
    id <MTLTexture>             _depthTarget;

    // Render pass descriptor which creates a render command encoder to draw to the drawable
    // textures
    MTLRenderPassDescriptor *_drawableRenderDescriptor;
    MTKTextureLoader* _textureLoader;

    vector_uint2 _viewportSize;
    
    NSUInteger _frameNum;
    id<MTLTexture> _texture;
    GB_device* _gameboydevice;
}

- (nonnull instancetype)initWithMetalDevice:(nonnull id<MTLDevice>)device
                        drawablePixelFormat:(MTLPixelFormat)drawabklePixelFormat
{
    self = [super init];

    _gameboydevice = GB_newDevice();
    GB_deviceloadRom(_gameboydevice, "testroms/tetris.gb");

    _frameNum = 0;

    _device = device;

    _commandQueue = [_device newCommandQueue];

    _drawableRenderDescriptor = [MTLRenderPassDescriptor new];
    _drawableRenderDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
    _drawableRenderDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
    _drawableRenderDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(0, 0, 0, 1);

    _textureLoader = [[MTKTextureLoader alloc] initWithDevice:device];

    __weak GameRenderer* weakSelf = self;

    [self createRenderPipeline:drawabklePixelFormat];

    return self;
}

-(CGImageRef)renderBackgroundFrame {
    int strIdx = 0;
    char console[100];
    // if(_gameboydevice->cpu->is_halted == false)
    while (_gameboydevice->ppu->frameReady == false){
        GBUpdateJoypadState(_gameboydevice, self.joypad);
        unsigned char cycles = GB_deviceCpuStep(_gameboydevice);
        GB_devicePPUstep(_gameboydevice, cycles);
    }
    // TODO: Render Frame
    // Frame done
    _gameboydevice->ppu->frameReady = false;
    uint8_t* data =  GB_ppu_gen_frame_bitmap(_gameboydevice);
    NSBitmapImageRep* img = [[NSBitmapImageRep alloc] 
        initWithBitmapDataPlanes: &data 
        pixelsWide:160 
        pixelsHigh:144 
        bitsPerSample:8 
        samplesPerPixel: 4 
        hasAlpha:YES isPlanar:NO
        colorSpaceName:NSDeviceRGBColorSpace 
        bitmapFormat:NSBitmapFormatThirtyTwoBitLittleEndian 
        bytesPerRow:160 * 4
        bitsPerPixel:32];
    
    //free(data);
    return [img CGImage];
}

-(CGImageRef)renderBackground {
    int strIdx = 0;
    char console[100];
    // if(_gameboydevice->cpu->is_halted == false)
    while (_gameboydevice->ppu->frameReady == false){
        GBUpdateJoypadState(_gameboydevice, self.joypad);
        unsigned char cycles = GB_deviceCpuStep(_gameboydevice);
        GB_devicePPUstep(_gameboydevice, cycles);
    }
    // TODO: Render Frame
    // Frame done
    _gameboydevice->ppu->frameReady = false;
    uint8_t* data =  GB_ppu_gen_background_bitmap(_gameboydevice);
    NSBitmapImageRep* img = [[NSBitmapImageRep alloc] 
        initWithBitmapDataPlanes: &data 
        pixelsWide:256 
        pixelsHigh:256 
        bitsPerSample:8 
        samplesPerPixel: 4 
        hasAlpha:YES isPlanar:NO
        colorSpaceName:NSDeviceRGBColorSpace 
        bitmapFormat:NSBitmapFormatThirtyTwoBitLittleEndian 
        bytesPerRow:256 * 4
        bitsPerPixel:32];
    
    //free(data);
    return [img CGImage];
}

-(void) createRenderPipeline: (MTLPixelFormat)drawabklePixelFormat {
    id<MTLLibrary> shaderLib = [_device newDefaultLibrary];
    if(!shaderLib) {
        NSLog(@" ERROR: Couldnt create a default shader library");
        return;
    }
    id <MTLFunction> vertexProgram = [shaderLib newFunctionWithName:@"vertexShader"];
    if(!vertexProgram) {
        NSLog(@">> ERROR: Couldn't load vertex function from default library");
        return;
    }

    id <MTLFunction> fragmentProgram = [shaderLib newFunctionWithName:@"fragmentShader"];
    if(!fragmentProgram) {
        NSLog(@" ERROR: Couldn't load fragment function from default library");
        return;
    }

    // Create a pipeline state descriptor to create a compiled pipeline state object
    MTLRenderPipelineDescriptor *pipelineDescriptor = [[MTLRenderPipelineDescriptor alloc] init];

    pipelineDescriptor.label                           = @"MyPipeline";
    pipelineDescriptor.vertexFunction                  = vertexProgram;
    pipelineDescriptor.fragmentFunction                = fragmentProgram;
    pipelineDescriptor.colorAttachments[0].pixelFormat = drawabklePixelFormat;

    NSError *error;
    _pipelineState = [_device newRenderPipelineStateWithDescriptor:pipelineDescriptor
                                                             error:&error];
    if(!_pipelineState)
    {
        NSLog(@"ERROR: Failed aquiring pipeline state: %@", error);
        assert(false);
        return;
    }
}

-(id<MTLBuffer>) genVertexBuffer {
    int32_t length = MIN(_viewportSize.x, _viewportSize.y);
    const GBVertex quadVertices[] = {
        // Pixel positions, Color coordinates
        { {  length,  -length },  { 1.f, 1.f } },
        { { -length,  -length },  { 0.f, 1.f } },
        { { -length,   length },  { 0.f, 0.f } },

        { {  length,  -length },  { 1.f, 1.f } },
        { { -length,   length },  { 0.f, 0.f } },
        { {  length,   length },  { 1.f, 0.f } },
    };

    // Create a vertex buffer, and initialize it with the vertex data.
    id<MTLBuffer> vertices = [_device newBufferWithBytes:quadVertices
                                     length:sizeof(quadVertices)
                                    options:MTLResourceStorageModeShared];
    vertices.label = @"Quad";
    return vertices;
}

- (void)renderToMetalLayer:(nonnull CAMetalLayer*)metalLayer {
    CGImageRef frame = [self renderBackgroundFrame];
    // Create a new command buffer for each render pass to the current drawable.
    id <MTLCommandBuffer> commandBuffer = [_commandQueue commandBuffer];

    id<CAMetalDrawable> currentDrawable = [metalLayer nextDrawable];

    // If the current drawable is nil, skip rendering this frame
    if(!currentDrawable)
    {
        return;
    }

    id<MTLBuffer> vertices = [self genVertexBuffer];

    _drawableRenderDescriptor.colorAttachments[0].texture = currentDrawable.texture;
    
    id <MTLRenderCommandEncoder> renderEncoder =
        [commandBuffer renderCommandEncoderWithDescriptor:_drawableRenderDescriptor];

    [renderEncoder setRenderPipelineState:_pipelineState];
    [renderEncoder setVertexBuffer:vertices offset:0 atIndex:GBVertexInputIndexVertices];

    GBUniforms uniforms;
    uniforms.scale = 1.0;
    uniforms.viewportSize = _viewportSize;

    id<MTLTexture> texture = [_textureLoader newTextureWithCGImage:frame options:nil error:nil];

    [renderEncoder setVertexBytes:&uniforms
                           length:sizeof(uniforms)
                          atIndex:GBVertexInputIndexUniforms];
    [renderEncoder setFragmentTexture:texture atIndex:GBTextureIndexOutput];
    [renderEncoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6];

    [renderEncoder endEncoding];
    [commandBuffer presentDrawable:currentDrawable];
    [commandBuffer commit];
}

- (void)drawableResize:(CGSize)drawableSize {
    _viewportSize.x = drawableSize.width;
    _viewportSize.y = drawableSize.height;
}

@end