#pragma once

#include "gpu/gpu.h"

namespace nx
{
    // A look for a whole picture: what each colour becomes. It is worked out once, into a table the GPU looks colours
    // up in, and again only when something about it changes.
    class ColorLook : NonCopyable
    {
    public:
        // Cells of the table on each side.
        static constexpr u32 kSide = 32;
        // Most cells on a side of a table that is read.
        static constexpr u32 kMostCube = 64;

        struct Settings
        {
            float exposure = 0;      // stops brighter
            float warmth = 0;        // -1 blue to 1 orange
            float tint = 0;          // -1 green to 1 magenta
            float contrast = 1;
            float saturation = 1;
            float fade = 0;          // blacks lifted, as old film
            float gamma = 1;         // more than 1 lifts the middle
            gpu::Color shadows = { 0, 0, 0, 0 };     // a colour for what is dark; its alpha is how much
            gpu::Color highlights = { 0, 0, 0, 0 };  // and for what is bright
        };

        ColorLook() = default;
        ~ColorLook();

        Settings settings;

        // Settings by name: none, warm, cold, faded, noir, sepia, dusk, vivid, bleach or night. False if it isn't one.
        bool preset(const char* name);
        // An Adobe or Resolve .cube 3D table, up to 64 a side, that colours go through before the settings. False, with
        // nothing changed, if it can't be read.
        bool readCube(const char* text, size_t size);
        void forgetCube();
        bool hasCube() const { return m_cube != nullptr; }

        // What a colour becomes, each part 0 to 1.
        void apply(float (&c)[3]) const;
        // The table, made again when anything has changed. None where colours can't be graded.
        gpu::Texture table(gpu::Device& device);
        // Lets go of the table, as the GPU goes.
        void release();

    private:
        void sampleCube(float (&c)[3]) const;

        float* m_cube = nullptr;
        u32 m_cubeSide = 0;
        u32 m_cubeVersion = 0;

        gpu::Device* m_device = nullptr;
        gpu::Texture m_table = {};
        Settings m_built;
        u32 m_builtCube = 0;
    };
}
