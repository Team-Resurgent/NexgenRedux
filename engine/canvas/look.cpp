#include "canvas/look.h"

#include "base/memory.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

namespace nx
{
    namespace
    {
        float luma(const float (&c)[3]) { return c[0] * 0.2126f + c[1] * 0.7152f + c[2] * 0.0722f; }
        float clamp01(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }

        // Towards a colour, keeping how bright it is.
        void tone(float (&c)[3], const gpu::Color& with, float much)
        {
            if (with.a <= 0 || much <= 0)
            {
                return;
            }
            const float rgb[3] = { with.r, with.g, with.b };
            const float l = luma(rgb);
            for (u32 i = 0; i < 3; i++)
            {
                c[i] += (rgb[i] - l) * with.a * much;
            }
        }

        struct Preset
        {
            const char* name;
            ColorLook::Settings settings;
        };

        ColorLook::Settings make(float exposure, float warmth, float tint, float contrast, float saturation, float fade, float gamma, gpu::Color shadows, gpu::Color highlights)
        {
            ColorLook::Settings s;
            s.exposure = exposure;
            s.warmth = warmth;
            s.tint = tint;
            s.contrast = contrast;
            s.saturation = saturation;
            s.fade = fade;
            s.gamma = gamma;
            s.shadows = shadows;
            s.highlights = highlights;
            return s;
        }

        const gpu::Color kNone = { 0, 0, 0, 0 };

        const Preset kPresets[] = {
            { "none", ColorLook::Settings() },
            { "warm", make(0, 0.45f, 0.05f, 1.06f, 1.08f, 0, 1, kNone, { 1, 0.75f, 0.45f, 0.35f }) },
            { "cold", make(0, -0.45f, -0.05f, 1.05f, 0.88f, 0, 1, { 0.3f, 0.5f, 1, 0.45f }, kNone) },
            { "faded", make(0.05f, 0.15f, 0, 0.88f, 0.7f, 0.7f, 1.05f, { 0.4f, 0.5f, 0.6f, 0.3f }, { 1, 0.9f, 0.75f, 0.25f }) },
            { "noir", make(-0.1f, 0, 0, 1.35f, 0, 0, 0.95f, kNone, kNone) },
            { "sepia", make(0.05f, 0.1f, 0, 1.05f, 0, 0.2f, 1, { 0.6f, 0.35f, 0.1f, 0.5f }, { 1, 0.8f, 0.55f, 0.45f }) },
            { "dusk", make(-0.05f, 0.3f, 0.25f, 1.08f, 1, 0, 1, { 0.35f, 0.25f, 0.85f, 0.5f }, { 1, 0.55f, 0.35f, 0.4f }) },
            { "vivid", make(0, 0.05f, 0, 1.15f, 1.4f, 0, 1, kNone, kNone) },
            { "bleach", make(0.05f, 0, 0, 1.3f, 0.45f, 0, 1, kNone, kNone) },
            { "night", make(-0.6f, -0.6f, 0, 1.1f, 0.55f, 0, 1.1f, { 0.2f, 0.3f, 0.85f, 0.5f }, kNone) },
        };

        bool same(const ColorLook::Settings& a, const ColorLook::Settings& b) { return memcmp(&a, &b, sizeof(a)) == 0; }

        // The next word on a line, and where the line after it starts.
        const char* skipSpace(const char* at, const char* end)
        {
            while (at < end && (*at == ' ' || *at == '\t'))
            {
                at++;
            }
            return at;
        }
    }

    ColorLook::~ColorLook()
    {
        release();
        memFree(m_cube);
    }

    bool ColorLook::preset(const char* name)
    {
        for (const Preset& p : kPresets)
        {
            if (strcmp(p.name, name) == 0)
            {
                settings = p.settings;
                return true;
            }
        }
        return false;
    }

    bool ColorLook::readCube(const char* text, size_t size)
    {
        const char* at = text;
        const char* end = text + size;
        u32 side = 0, count = 0;
        float low[3] = { 0, 0, 0 }, high[3] = { 1, 1, 1 };
        float* cube = nullptr;
        char line[256];
        bool fine = true;
        while (fine && at < end)
        {
            const char* stop = at;
            while (stop < end && *stop != '\n' && *stop != '\r')
            {
                stop++;
            }
            const size_t length = static_cast<size_t>(stop - at) < sizeof(line) - 1 ? static_cast<size_t>(stop - at) : sizeof(line) - 1;
            memcpy(line, at, length);
            line[length] = 0;
            at = stop < end ? stop + 1 : end;
            const char* word = skipSpace(line, line + length);
            if (*word == 0 || *word == '#')
            {
                continue;
            }
            if (*word == '-' || *word == '.' || (*word >= '0' && *word <= '9'))
            {
                if (cube == nullptr || count >= side * side * side)
                {
                    fine = false;
                    break;
                }
                char* next = nullptr;
                const char* from = word;
                for (u32 i = 0; i < 3; i++)
                {
                    const float v = strtof(from, &next);
                    if (next == from)
                    {
                        fine = false;
                        break;
                    }
                    cube[count * 3 + i] = (v - low[i]) / (high[i] - low[i] > 1e-6f ? high[i] - low[i] : 1);
                    from = next;
                }
                count++;
            }
            else if (strncmp(word, "LUT_3D_SIZE", 11) == 0)
            {
                const long n = strtol(word + 11, nullptr, 10);
                if (cube != nullptr || n < 2 || n > static_cast<long>(kMostCube))
                {
                    fine = false;
                    break;
                }
                side = static_cast<u32>(n);
                cube = static_cast<float*>(memAlloc(sizeof(float) * 3 * side * side * side, MemTag::Texture));
                fine = cube != nullptr;
            }
            else if (strncmp(word, "LUT_1D_SIZE", 11) == 0)
            {
                fine = false;
            }
            else if (strncmp(word, "DOMAIN_MIN", 10) == 0 || strncmp(word, "DOMAIN_MAX", 10) == 0)
            {
                float* into = word[8] == 'I' ? low : high;
                char* next = nullptr;
                const char* from = word + 10;
                for (u32 i = 0; i < 3; i++)
                {
                    into[i] = strtof(from, &next);
                    from = next;
                }
            }
            // TITLE, and anything else, says nothing about colours.
        }
        if (!fine || cube == nullptr || count != side * side * side)
        {
            memFree(cube);
            return false;
        }
        memFree(m_cube);
        m_cube = cube;
        m_cubeSide = side;
        m_cubeVersion++;
        return true;
    }

    void ColorLook::forgetCube()
    {
        if (m_cube)
        {
            memFree(m_cube);
            m_cube = nullptr;
            m_cubeSide = 0;
            m_cubeVersion++;
        }
    }

    void ColorLook::sampleCube(float (&c)[3]) const
    {
        const u32 n = m_cubeSide;
        float f[3];
        u32 i0[3], i1[3];
        for (u32 k = 0; k < 3; k++)
        {
            const float p = clamp01(c[k]) * static_cast<float>(n - 1);
            i0[k] = static_cast<u32>(p);
            i0[k] = i0[k] > n - 2 ? n - 2 : i0[k];
            f[k] = p - static_cast<float>(i0[k]);
            i1[k] = i0[k] + 1;
        }
        float out[3] = { 0, 0, 0 };
        for (u32 corner = 0; corner < 8; corner++)
        {
            const u32 r = corner & 1 ? i1[0] : i0[0], g = corner & 2 ? i1[1] : i0[1], b = corner & 4 ? i1[2] : i0[2];
            const float w = (corner & 1 ? f[0] : 1 - f[0]) * (corner & 2 ? f[1] : 1 - f[1]) * (corner & 4 ? f[2] : 1 - f[2]);
            const float* v = m_cube + (r + g * n + b * n * n) * 3;
            out[0] += v[0] * w;
            out[1] += v[1] * w;
            out[2] += v[2] * w;
        }
        c[0] = out[0];
        c[1] = out[1];
        c[2] = out[2];
    }

    void ColorLook::apply(float (&c)[3]) const
    {
        if (m_cube)
        {
            sampleCube(c);
        }
        const Settings& s = settings;
        const float bright = powf(2.0f, s.exposure);
        c[0] *= bright * (1 + 0.12f * s.warmth + 0.04f * s.tint);
        c[1] *= bright * (1 + 0.02f * s.warmth - 0.1f * s.tint);
        c[2] *= bright * (1 - 0.12f * s.warmth + 0.04f * s.tint);
        for (float& v : c)
        {
            v = (v - 0.5f) * s.contrast + 0.5f;
        }
        const float l = luma(c);
        for (float& v : c)
        {
            v = l + (v - l) * s.saturation;
        }
        const float lit = clamp01(luma(c));
        tone(c, s.shadows, (1 - lit) * (1 - lit));
        tone(c, s.highlights, lit * lit);
        for (float& v : c)
        {
            v += s.fade * (0.12f - 0.22f * v);
            v = clamp01(v);
            if (s.gamma != 1 && s.gamma > 0.01f)
            {
                v = powf(v, 1 / s.gamma);
            }
        }
    }

    gpu::Texture ColorLook::table(gpu::Device& device)
    {
        if (m_table && m_device == &device && same(settings, m_built) && m_builtCube == m_cubeVersion)
        {
            return m_table;
        }
        release();
        constexpr u32 n = kSide;
        u8* pixels = static_cast<u8*>(memAlloc(n * n * n * 4, MemTag::Texture));
        if (pixels == nullptr)
        {
            return {};
        }
        constexpr float step = 1.0f / static_cast<float>(n - 1);
        for (u32 b = 0; b < n; b++)
        {
            for (u32 g = 0; g < n; g++)
            {
                for (u32 r = 0; r < n; r++)
                {
                    float c[3] = { static_cast<float>(r) * step, static_cast<float>(g) * step, static_cast<float>(b) * step };
                    apply(c);
                    u8* out = pixels + (r + g * n + b * n * n) * 4;
                    for (u32 k = 0; k < 3; k++)
                    {
                        out[k] = static_cast<u8>(c[k] * 255.0f + 0.5f);
                    }
                    out[3] = 255;
                }
            }
        }
        m_table = device.createGrade(pixels, n);
        memFree(pixels);
        m_device = m_table ? &device : nullptr;
        m_built = settings;
        m_builtCube = m_cubeVersion;
        return m_table;
    }

    void ColorLook::release()
    {
        if (m_table && m_device)
        {
            m_device->destroyTexture(m_table);
        }
        m_table = {};
        m_device = nullptr;
    }
}
