#ifndef GBShaderTypes_h
#define GBShaderTypes_h

#include <simd/simd.h>

typedef enum GBVertexInputIndex
{
    GBVertexInputIndexVertices = 0,
    GBVertexInputIndexUniforms = 1,
} GBVertexInputIndex;

typedef enum GBTextureIndex
{
    GBTextureIndexInput  = 0,
    GBTextureIndexOutput = 1,
} GBTextureIndex;

typedef struct
{
    // Positions in pixel space (i.e. a value of 100 indicates 100 pixels from the origin/center)
    vector_float2 position;

    // 2D texture coordinate
    vector_float2 textCoord;
} GBVertex;

typedef struct
{
    float scale;
    vector_uint2 viewportSize;
} GBUniforms;

#endif /* GBShaderTypes_h */