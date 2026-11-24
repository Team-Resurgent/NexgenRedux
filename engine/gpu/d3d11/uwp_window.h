#pragma once

#include "base/types.h"

struct IUnknown;

namespace nx
{
    // What Platform::nativeWindow() points to on UWP, where there is no HWND.
    struct UwpWindow
    {
        IUnknown* window = nullptr;  // the CoreWindow
        void* user = nullptr;
        void (*size)(void* user, u32& width, u32& height) = nullptr;  // in pixels
    };
}
