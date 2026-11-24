#pragma once

// How effects are written: in HLSL's words, using only what GLSL has too, which is given the same words
// here. Around them goes what differs.
//
// A picture effect has:
//     float4 effect(float2 uv, float4 color)
// and may use picture(uv), which is what was drawn there, size (in pixels), time (seconds) and values[0]
// to values[3].
//
// A surface effect has:
//     float4 paint(float4 color, float2 uv, float3 place, float3 normal, inout float glow)
// which is lit afterwards; glow 1 is as bright as its colour. picture(uv) is its image. With
// #define MOVES it also has, to move each corner before anything else:
//     float3 move(float3 place, float3 normal, float2 uv)
namespace nx::gpu::shaders
{
    struct EffectUniforms
    {
        float values[16];
        float area[4];   // of the picture: from and size, 0 to 1
        float info[4];   // size, time
    };

    inline const char kEffectGlslWords[] = R"(
#define float2 vec2
#define float3 vec3
#define float4 vec4
#define float2x2 mat2
#define float3x3 mat3
#define int2 ivec2
#define lerp mix
#define frac fract
#define fmod mod
#define ddx dFdx
#define ddy dFdy
#define saturate(x) clamp(x, 0.0, 1.0)
layout(std140) uniform EffectValues
{
    vec4 values[4];
    vec4 effectArea;
    vec4 effectInfo;
};
#define size effectInfo.xy
#define time effectInfo.z
)";

    inline const char kEffectHlslWords[] = R"(
cbuffer EffectValues : register(b3)
{
    float4 values[4];
    float4 effectArea;
    float4 effectInfo;
};
#define size effectInfo.xy
#define time effectInfo.z
)";
}
