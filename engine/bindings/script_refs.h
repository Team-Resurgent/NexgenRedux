#pragma once

#include "bindings/bindings.h"

#include <angelscript.h>

#include <string>

// What the files of bindings share.
namespace nx
{
    inline Services& services()
    {
        return *static_cast<Services*>(asGetActiveContext()->GetEngine()->GetUserData());
    }

    inline void scriptError(const std::string& message)
    {
        asGetActiveContext()->SetException(message.c_str());
    }

    // A script's hold on a cached asset.
    struct AssetHandle
    {
        Asset* asset;
        Assets* owner;
        int refs;

        void addRef() { refs++; }
        void release()
        {
            if (--refs == 0)
            {
                owner->release(asset);
                destroy(this);
            }
        }
        float width() const { return static_cast<float>(asset->width); }
        float height() const { return static_cast<float>(asset->height); }
        bool failed() const { return asset->failed; }
        bool ready() const { return asset->ready; }
    };

    struct TextureRef : AssetHandle
    {
    };

    struct FontRef : AssetHandle
    {
    };

    struct SoundRef : AssetHandle
    {
    };

    struct ModelRef : AssetHandle
    {
    };

    // bind_scene.cpp
    void bindScene(Binder& binder);
    // bind_space.cpp
    void bindSpaceTypes(Binder& binder);
    void bindSpace(Binder& binder);
    // bind_net.cpp
    void bindNet(Binder& binder);
    // bind_xbox.cpp
    void bindXbox(Binder& binder);
}
