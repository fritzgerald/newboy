/*
    see doc for how to compile
    https://developer.apple.com/documentation/metal/shader_libraries/metal_libraries/building_a_shader_library_by_precompiling_source_files?language=objc
*/

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

// Include header shared between this Metal shader code and C code executing Metal API commands
#include "GBShaderTypes.h"

// Vertex shader outputs and per-fragment inputs
struct RasterizerData
{
    float4 clipSpacePosition [[position]];
    float2 textureCoordinate;
};

vertex RasterizerData
vertexShader(uint vertexID [[ vertex_id ]],
             constant GBVertex *vertexArray [[ buffer(GBVertexInputIndexVertices) ]],
             constant GBUniforms &uniforms  [[ buffer(GBVertexInputIndexUniforms) ]])

{
    RasterizerData out;

    float2 pixelSpacePosition = vertexArray[vertexID].position.xy;

    // Scale the vertex by scale factor of the current frame
    pixelSpacePosition *= uniforms.scale;

    float2 viewportSize = float2(uniforms.viewportSize);

    // Divide the pixel coordinates by half the size of the viewport to convert from positions in
    // pixel space to positions in clip space
    out.clipSpacePosition.xy = pixelSpacePosition / (viewportSize / 1.0);
    out.clipSpacePosition.z = 0.0;
    out.clipSpacePosition.w = 1.0;

    out.textureCoordinate = vertexArray[vertexID].textCoord;

    return out;
}

fragment float4
fragmentShader(RasterizerData in [[stage_in]], texture2d<half> colorTexture [[ texture(GBTextureIndexOutput) ]])
{
    constexpr sampler textureSampler (mag_filter::nearest, min_filter::nearest);

    // Sample the texture and return the color to colorSample
    const half4 colorSample = colorTexture.sample (textureSampler, in.textureCoordinate);
    return float4(colorSample);
}