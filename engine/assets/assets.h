#pragma once

#include "audio/audio.h"
#include "base/thread.h"
#include "gpu/gpu.h"
#include "platform/platform.h"

#include <atomic>
#include <vector>

namespace nx
{
    constexpr u32 kFirstGlyph = 32;
    constexpr u32 kLastGlyph = 255;  // Latin-1
    constexpr u32 kGlyphCount = kLastGlyph - kFirstGlyph + 1;

    struct Glyph
    {
        float u0, v0, u1, v1;
        float x, y, width, height;  // pixels, from the pen on the baseline
        float advance;
    };

    struct FontData
    {
        Glyph glyphs[kGlyphCount];
        float ascent = 0;      // pixels
        float lineHeight = 0;  // pixels
        float scale = 1;       // pixels to the size that was asked for
    };

    struct ModelData;

    enum class AssetType : u8 { Texture, Font, Sound, Model, Effect, Count };

    // One loaded file. Owned by the cache and shared by everyone who asks for it.
    struct Asset
    {
        AssetType type = AssetType::Texture;
        bool ready = false;        // until then a texture is invisible and a sound silent
        bool failed = false;       // missing or broken; a texture is then a placeholder
        gpu::Texture texture;      // the image, or a font's glyphs
        u32 width = 0, height = 0;
        FontData* font = nullptr;
        SoundData* sound = nullptr;
        ModelData* model = nullptr;
        gpu::Effect effect;
        u32 refs = 0;
        size_t bytes = 0;
        u64 lastUsed = 0;
        u64 hash = 0;
        u64 fileTime = 0;          // for reloading when it changes
        char key[208] = {};        // empty: this slot is free
        char path[192] = {};

        bool live() const { return key[0] != 0; }
    };

    struct AssetStats
    {
        u32 inUse = 0;     // assets someone holds
        u32 warm = 0;      // loaded, but not held
        size_t inUseBytes = 0;
        size_t warmBytes = 0;
        size_t typeBytes[static_cast<size_t>(AssetType::Count)] = {};
        u32 loads = 0;     // files read
        u32 hits = 0;      // requests served without reading
        u32 evictions = 0;
        u32 trims = 0;     // times everything spare was unloaded on request
        u32 thrash = 0;    // reloaded soon after being evicted
    };

    // Loads each file once, away from the frame, and shares it. Releasing an asset keeps it warm; it is
    // only unloaded, least recently used first, when its type's budget needs the room.
    class Assets : NonCopyable
    {
    public:
        ~Assets() { shutdown(); }

        // Budgets are per type; fonts and models share the textures' budget.
        bool init(gpu::Device& device, Platform& platform, size_t textureBudget, size_t soundBudget);
        void shutdown();

        // Each adds a reference, which release() gives back, and never returns nullptr. Textures and
        // sounds arrive later, in update(); fonts at once, because their size is needed straight away.
        // Paths are relative to the media folder.
        Asset* texture(const char* path);
        // A picture to draw into, of its own: nobody else can ask for it, and it goes when it is let go. nullptr where
        // the GPU can't.
        Asset* target(u32 width, u32 height);
        Asset* sound(const char* path);
        // glTF. Arrives like a texture; until then there is nothing to draw.
        Asset* model(const char* path);
        // How an image is read from the web, where there is one: called away from the game's thread.
        using WebRead = bool (*)(const char* url, FileData& out);
        void setWeb(WebRead read) { m_webRead = read; }
        // A shader. Ready, but no effect, where the GPU has none.
        Asset* effect(const char* path);
        // Size is in design units; the glyphs are made at the size they will appear on screen.
        Asset* font(const char* path, u32 size);

        void addRef(Asset* asset) { asset->refs++; }
        void release(Asset* asset);

        // Once a frame: takes in what has finished loading.
        void update();
        // Requests not yet finished.
        u32 pending() const { return m_pending; }
        // Waits for all of them.
        void finish();

        Platform& platform() const { return *m_platform; }

        // Looks for files that have changed and loads them again, in place. For development.
        void reloadChanged();

        // Unloads everything that isn't held.
        void trim();
        void setBudget(AssetType type, size_t bytes);
        size_t budget(AssetType type) const { return m_budget[budgetIndex(type)]; }

        // How many pixels a design unit covers, for fonts loaded from now on.
        void setFontScale(float pixelsPerUnit) { m_fontScale = pixelsPerUnit; }

        AssetStats stats() const;

        template <typename Fn>
        void forEach(Fn fn) const
        {
            for (u32 i = 0; i < kCapacity; i++)
            {
                if (m_assets[i].live())
                {
                    fn(m_assets[i]);
                }
            }
        }

        // "A\\b/../C.png" becomes "a/c.png", so one file has one name. False if it leaves the folder.
        static bool cleanPath(const char* path, char* out, size_t size, bool lowerCase);

    private:
        static constexpr u32 kCapacity = 512;
        static constexpr u32 kEvictedMemory = 64;

        struct Load;

        static size_t budgetIndex(AssetType type) { return type == AssetType::Sound ? 1 : 0; }
        static void loadInBackground(void* load);
        static bool readBeside(const char* name, std::vector<u8>& out, void* user);
        Asset* request(const char* path, AssetType type);
        Asset* find(const char* key, u64 hash);
        Asset* add(const char* key, u64 hash, AssetType type);
        void startLoad(Asset& asset);
        void takeIn(size_t bytes);
        void finishLoad(Load& load);
        void fail(Asset& asset);
        bool loadFont(Asset& asset, const char* path, u32 size);
        void makeRoom(AssetType type, size_t bytes);
        bool evictOne(size_t budget);
        bool evictAny();
        void unload(Asset& asset);
        void unloadData(Asset& asset);

        gpu::Device* m_device = nullptr;
        Platform* m_platform = nullptr;
        JobQueue m_jobs;
        JobQueue m_webJobs[2];    // which may wait for a long time
        bool m_webStarted = false;
        WebRead m_webRead = nullptr;
        Asset* m_assets = nullptr;
        gpu::Texture m_missing;
        gpu::Texture m_invisible;
        Load* m_loading = nullptr;
        u32 m_pending = 0;
        size_t m_budget[2] = {};
        size_t m_bytes[2] = {};
        u64 m_clock = 0;
        float m_fontScale = 1;
        AssetStats m_stats;
        struct
        {
            u64 hash;
            double when;
        } m_evicted[kEvictedMemory] = {};
        u32 m_evictedNext = 0;
    };
}
