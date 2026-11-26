#pragma once

#include "base/types.h"

// Smooth noise, the same everywhere for the same seed: Ken Perlin's improved gradient noise, and layers of it.
namespace nx
{
    // -1 to 1, changing smoothly, with hills and dips about a unit apart.
    float noise(float x, float y, float z);
    // Several layers, each twice as fine and `roughness` as strong as the last, kept within -1 to 1.
    float fractalNoise(float x, float y, float z, u32 octaves, float roughness);
    // A different but repeatable pattern. 0 is the one it starts with.
    void setNoiseSeed(u32 seed);
}
