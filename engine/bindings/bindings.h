#pragma once

#include "assets/assets.h"
#include "audio/audio.h"
#include "canvas/canvas.h"
#include "input/input.h"
#include "platform/platform.h"
#include "save/save.h"
#include "scene/world.h"
#include "script/binder.h"
#include "script/coroutines.h"

namespace nx
{
    class Screens;

    // What scripts can reach. Found through the script engine, so nothing is global.
    struct Services
    {
        Platform* platform = nullptr;
        gpu::Device* device = nullptr;
        Assets* assets = nullptr;
        Canvas* canvas = nullptr;
        Input* input = nullptr;
        ScriptHost* host = nullptr;
        Audio* audio = nullptr;
        Save* save = nullptr;
        Coroutines* coroutines = nullptr;
        Screens* screens = nullptr;
        World* world = nullptr;         // of the screen whose script is running
        asITypeInfo* entityArray = nullptr;
        char folder[96] = {};  // of the screen whose script is running
        double time = 0;
        float fps = 0;
        u32 draws = 0;
        u32 reloads = 0;
        bool development = false;
        u32 random = 2463534242u;
    };

    // Volumes and bindings, to and from the save.
    void loadSettings(Services& services);
    void saveSettings(Services& services);

    // Registers everything. The services must outlive the script host.
    bool bindEngine(Binder& binder, Services& services);

    // Code that every script module is given.
    const char* behaviourPrelude();

    // "asset:ship.png" is in the running screen's asset folder and "global:x" in the shared one.
    bool resolveAsset(const Services& services, const char* name, char* out, size_t size);
}
