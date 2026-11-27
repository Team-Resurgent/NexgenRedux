#pragma once

#include "base/types.h"

namespace nx
{
    // What Platform::nativeWindow() points to where the GPU is OpenGL. The platform makes the window
    // and its 3.3 core context, which is current on the calling thread.
    struct GlWindow
    {
        void* user = nullptr;
        void* (*load)(const char* function) = nullptr;
        void (*size)(void* user, u32& width, u32& height) = nullptr;  // in pixels; 0 when minimised
        void (*swap)(void* user) = nullptr;
        void (*wait)(void* user, bool on) = nullptr;  // for the screen's refresh before swapping, or not; may be missing
    };
}
