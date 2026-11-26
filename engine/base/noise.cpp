#include "base/noise.h"

#include <math.h>

namespace nx
{
    namespace
    {
        u8 g_order[512];
        bool g_made = false;

        void shuffle(u32 seed)
        {
            u8 order[256];
            for (u32 i = 0; i < 256; i++)
            {
                order[i] = static_cast<u8>(i);
            }
            // A small generator of its own, so that a seed means the same on every machine.
            u32 state = seed * 747796405u + 2891336453u;
            for (u32 i = 255; i > 0; i--)
            {
                state = state * 1664525u + 1013904223u;
                const u32 j = (state >> 8) % (i + 1);
                const u8 t = order[i];
                order[i] = order[j];
                order[j] = t;
            }
            for (u32 i = 0; i < 512; i++)
            {
                g_order[i] = order[i & 255];
            }
            g_made = true;
        }

        float fade(float t) { return t * t * t * (t * (t * 6 - 15) + 10); }
        float mix(float a, float b, float t) { return a + (b - a) * t; }

        // One of twelve directions, from the edges of a cube.
        float grad(u8 hash, float x, float y, float z)
        {
            const u8 h = hash & 15;
            const float u = h < 8 ? x : y;
            const float v = h < 4 ? y : (h == 12 || h == 14) ? x : z;
            return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
        }
    }

    void setNoiseSeed(u32 seed)
    {
        shuffle(seed);
    }

    float noise(float x, float y, float z)
    {
        if (!g_made)
        {
            shuffle(0);
        }
        const float fx = floorf(x), fy = floorf(y), fz = floorf(z);
        const u32 X = static_cast<u32>(static_cast<i32>(fx)) & 255, Y = static_cast<u32>(static_cast<i32>(fy)) & 255, Z = static_cast<u32>(static_cast<i32>(fz)) & 255;
        x -= fx;
        y -= fy;
        z -= fz;
        const float u = fade(x), v = fade(y), w = fade(z);
        const u8* p = g_order;
        const u32 A = p[X] + Y, AA = p[A] + Z, AB = p[A + 1] + Z, B = p[X + 1] + Y, BA = p[B] + Z, BB = p[B + 1] + Z;
        const float n = mix(mix(mix(grad(p[AA], x, y, z), grad(p[BA], x - 1, y, z), u), mix(grad(p[AB], x, y - 1, z), grad(p[BB], x - 1, y - 1, z), u), v),
                            mix(mix(grad(p[AA + 1], x, y, z - 1), grad(p[BA + 1], x - 1, y, z - 1), u), mix(grad(p[AB + 1], x, y - 1, z - 1), grad(p[BB + 1], x - 1, y - 1, z - 1), u), v), w);
        // Its range is a little less than -1 to 1; stretched to nearly fill it.
        const float stretched = n * 1.1f;
        return stretched < -1 ? -1 : stretched > 1 ? 1 : stretched;
    }

    float fractalNoise(float x, float y, float z, u32 octaves, float roughness)
    {
        octaves = octaves < 1 ? 1 : octaves > 12 ? 12 : octaves;
        float total = 0, strength = 1, most = 0, scale = 1;
        for (u32 i = 0; i < octaves; i++)
        {
            // Each layer a little aside, so that their hills don't all meet at whole numbers.
            const float shift = static_cast<float>(i) * 17.13f;
            total += noise(x * scale + shift, y * scale + shift, z * scale + shift) * strength;
            most += strength;
            strength *= roughness;
            scale *= 2;
        }
        return most > 0 ? total / most : 0;
    }
}
