#include "assets/assets.h"
#include "assets/decode.h"
#include "assets/formats.h"
#include "assets/model.h"
#include "base/log.h"

#include <stdio.h>
#include <string.h>
#include <string>

namespace nx
{
    namespace
    {
        bool isWeb(const char* path)
        {
            return strncmp(path, "http://", 7) == 0 || strncmp(path, "https://", 8) == 0;
        }

        constexpr double kThrashSeconds = 5.0;
        constexpr size_t kTakenInPerUpdate = 4 * 1024 * 1024;

        u64 hashText(const char* text)
        {
            u64 hash = 14695981039346656037ull;  // FNV-1a
            for (; *text; text++)
            {
                hash = (hash ^ static_cast<u8>(*text)) * 1099511628211ull;
            }
            return hash;
        }
    }

    // One file on its way in. The worker fills it and sets done; the game thread does the rest.
    struct Assets::Load
    {
        Load* next = nullptr;
        Asset* asset = nullptr;
        Platform* platform = nullptr;
        AssetType type = AssetType::Texture;
        char path[192] = {};

        FileData file;                // a cooked texture is used where it lies
        u8* decoded = nullptr;        // or a decoded one, to be freed with freeImage
        gpu::TextureData texture;
        SoundData* sound = nullptr;
        ModelData* model = nullptr;
        WebRead webRead = nullptr;
        u64 fileTime = 0;
        std::atomic<bool> done{ false };
    };

    bool Assets::cleanPath(const char* path, char* out, size_t size, bool lowerCase)
    {
        size_t length = 0;
        size_t starts[32];  // where each part begins, for ".."
        size_t depth = 0;
        const char* p = path;
        while (*p)
        {
            while (*p == '/' || *p == '\\')
            {
                p++;
            }
            const char* end = p;
            while (*end && *end != '/' && *end != '\\')
            {
                end++;
            }
            const size_t part = static_cast<size_t>(end - p);
            if (part == 0 || (part == 1 && p[0] == '.'))
            {
                p = end;
                continue;
            }
            if (part == 2 && p[0] == '.' && p[1] == '.')
            {
                if (depth == 0)
                {
                    return false;
                }
                length = starts[--depth];
                p = end;
                continue;
            }
            if (depth == sizeof(starts) / sizeof(starts[0]) || length + part + 2 > size || memchr(p, ':', part))
            {
                return false;
            }
            starts[depth++] = length;
            if (length)
            {
                out[length++] = '/';
            }
            for (; p < end; p++)
            {
                const char c = *p;
                out[length++] = lowerCase && c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
            }
        }
        out[length] = 0;
        return length > 0;
    }

    bool Assets::init(gpu::Device& device, Platform& platform, size_t textureBudget, size_t soundBudget)
    {
        NX_ASSERT(m_assets == nullptr);
        m_device = &device;
        m_platform = &platform;
        m_budget[0] = textureBudget;
        m_budget[1] = soundBudget;
        m_assets = static_cast<Asset*>(memAlloc(sizeof(Asset) * kCapacity, MemTag::General));
        if (m_assets == nullptr)
        {
            return false;
        }
        for (u32 i = 0; i < kCapacity; i++)
        {
            new (&m_assets[i]) Asset();
        }

        // A checkerboard nobody could mistake for real art.
        u8 pixels[16 * 16 * 4];
        for (u32 i = 0; i < 16 * 16; i++)
        {
            const bool on = ((i % 16) / 4 + (i / 16) / 4) % 2 == 0;
            pixels[i * 4 + 0] = on ? 255 : 0;
            pixels[i * 4 + 1] = 0;
            pixels[i * 4 + 2] = on ? 255 : 0;
            pixels[i * 4 + 3] = 255;
        }
        m_missing = m_device->createTexture(gpu::rgbaTexture(pixels, 16, 16));
        const u8 nothing[4] = { 255, 255, 255, 0 };
        m_invisible = m_device->createTexture(gpu::rgbaTexture(nothing, 1, 1));
        return m_missing && m_invisible && m_jobs.start();
    }

    void Assets::shutdown()
    {
        if (m_assets == nullptr)
        {
            return;
        }
        finish();
        m_jobs.stop();
        m_webJobs[0].stop();
        m_webJobs[1].stop();
        m_webStarted = false;
        for (u32 i = 0; i < kCapacity; i++)
        {
            if (m_assets[i].live())
            {
                if (m_assets[i].refs)
                {
                    NX_WARN("asset still held at shutdown: %s (%u)", m_assets[i].key, static_cast<unsigned>(m_assets[i].refs));
                }
                unload(m_assets[i]);
            }
        }
        m_device->destroyTexture(m_missing);
        m_device->destroyTexture(m_invisible);
        memFree(m_assets);
        m_assets = nullptr;
    }

    Asset* Assets::find(const char* key, u64 hash)
    {
        for (u32 i = 0; i < kCapacity; i++)
        {
            Asset& a = m_assets[i];
            if (a.hash == hash && a.live() && strcmp(a.key, key) == 0)
            {
                a.refs++;
                a.lastUsed = ++m_clock;
                m_stats.hits++;
                return &a;
            }
        }
        return nullptr;
    }

    Asset* Assets::add(const char* key, u64 hash, AssetType type)
    {
        for (int attempt = 0; attempt < 2; attempt++)
        {
            for (u32 i = 0; i < kCapacity; i++)
            {
                Asset& a = m_assets[i];
                if (!a.live())
                {
                    a = Asset();
                    a.type = type;
                    a.hash = hash;
                    a.refs = 1;
                    a.lastUsed = ++m_clock;
                    a.texture = m_invisible;
                    snprintf(a.key, sizeof(a.key), "%s", key);
                    for (const auto& e : m_evicted)
                    {
                        if (e.hash == hash && m_platform->seconds() - e.when < kThrashSeconds)
                        {
                            m_stats.thrash++;
                            NX_WARN("reloading %s just after evicting it: the asset budget is too small", key);
                        }
                    }
                    return &a;
                }
            }
            if (!evictAny())
            {
                break;
            }
        }
        NX_ASSERT(!"too many assets in use");
        return nullptr;
    }

    Asset* Assets::texture(const char* path)
    {
        return request(path, AssetType::Texture);
    }

    Asset* Assets::sound(const char* path)
    {
        return request(path, AssetType::Sound);
    }

    Asset* Assets::model(const char* path)
    {
        return request(path, AssetType::Model);
    }

    Asset* Assets::effect(const char* path)
    {
        return request(path, AssetType::Effect);
    }

    Asset* Assets::request(const char* path, AssetType type)
    {
        char clean[192];
        char key[208];
        char lower[192];
        // On the web, names are as they are.
        const bool web = isWeb(path) && type == AssetType::Texture && strlen(path) < sizeof(clean);
        if (web)
        {
            snprintf(clean, sizeof(clean), "%s", path);
            snprintf(lower, sizeof(lower), "%s", path);
        }
        const bool valid = web || (cleanPath(path, clean, 128, false) && cleanPath(path, lower, 128, true));
        // The same file could be asked for as different things.
        snprintf(key, sizeof(key), "%s%s", type == AssetType::Sound ? "sound!" : type == AssetType::Model ? "model!" : type == AssetType::Effect ? "effect!" : "", valid ? lower : "?");
        const u64 hash = hashText(key);
        if (Asset* found = find(key, hash))
        {
            return found;
        }
        Asset* asset = add(key, hash, type);
        if (!valid)
        {
            NX_ERROR("bad asset path: %s", path);
            fail(*asset);
            return asset;
        }
        snprintf(asset->path, sizeof(asset->path), "%s", clean);
        startLoad(*asset);
        return asset;
    }

    void Assets::startLoad(Asset& asset)
    {
        Load* load = create<Load>(MemTag::General);
        if (load == nullptr)
        {
            fail(asset);
            return;
        }
        load->asset = &asset;
        load->platform = m_platform;
        load->type = asset.type;
        snprintf(load->path, sizeof(load->path), "%s", asset.path);
        load->next = m_loading;
        m_loading = load;
        m_pending++;
        m_stats.loads++;
        // A full queue means a great deal is waiting: take some of it in and try again.
        JobQueue* queue = &m_jobs;
        if (isWeb(load->path))
        {
            load->webRead = load->type == AssetType::Texture ? m_webRead : nullptr;
            if (!m_webStarted)
            {
                m_webStarted = m_webJobs[0].start() && m_webJobs[1].start();
            }
            queue = m_webStarted ? &m_webJobs[m_stats.loads % 2] : queue;
        }
        while (!queue->add(loadInBackground, load))
        {
            sleepMilliseconds(1);
        }
    }

    // A file that a model names, in its own folder.
    bool Assets::readBeside(const char* name, std::vector<u8>& out, void* user)
    {
        const Load& load = *static_cast<Load*>(user);
        const char* slash = strrchr(load.path, '/');
        std::string path = std::string(load.path, slash ? static_cast<size_t>(slash - load.path) + 1 : 0) + name;
        // What players bring has been given plain names.
        for (size_t i = 4; strncmp(load.path, "own/", 4) == 0 && i < path.size(); i++)
        {
            path[i] = path[i] == '/' ? '/' : plainLetter(path[i]);
        }
        char clean[192];
        FileData file;
        if (!cleanPath(path.c_str(), clean, sizeof(clean), false) || !load.platform->readFile(clean, file, MemTag::General))
        {
            return false;
        }
        out.assign(file.data, file.data + file.size);
        return true;
    }

    void Assets::loadInBackground(void* user)
    {
        Load& load = *static_cast<Load*>(user);
        const bool web = isWeb(load.path);
        load.fileTime = web ? 1 : load.platform->fileTime(load.path);
        if (web ? load.webRead && load.webRead(load.path, load.file) : load.platform->readFile(load.path, load.file, load.type == AssetType::Sound ? MemTag::Audio : MemTag::Texture))
        {
            if (load.type == AssetType::Sound)
            {
                load.sound = loadSound(load.file.data, load.file.size);
                if (load.sound == nullptr)
                {
                    NX_ERROR("%s is not a sound this engine can read", load.path);
                }
                load.file.clear();
            }
            else if (load.type == AssetType::Effect)
            {
                // Its words are kept for the GPU's thread.
            }
            else if (load.type == AssetType::Model)
            {
                load.model = loadModel(load.file.data, load.file.size, load.path, readBeside, &load);
                load.file.clear();
            }
            else if (isCookedTexture(load.file.data, load.file.size))
            {
                if (!readCookedTexture(load.file.data, load.file.size, load.texture))
                {
                    load.texture = {};
                    NX_ERROR("%s is damaged", load.path);
                }
            }
            else
            {
                u32 width = 0, height = 0;
                if (!imageSize(load.file.data, load.file.size, width, height))
                {
                    NX_ERROR("%s is not an image this engine can read", load.path);
                }
                else if (!gpu::validTextureSize(width, height))
                {
                    NX_ERROR("%s is %ux%u; the largest allowed is %u", load.path, static_cast<unsigned>(width), static_cast<unsigned>(height), static_cast<unsigned>(gpu::kMaxTextureSize));
                }
                else
                {
                    load.decoded = decodeImage(load.file.data, load.file.size, width, height);
                    if (load.decoded)
                    {
                        load.texture = gpu::rgbaTexture(load.decoded, width, height);
                    }
                    else
                    {
                        NX_ERROR("cannot decode %s", load.path);
                    }
                }
                load.file.clear();
            }
        }
        load.done.store(true, std::memory_order_release);
    }

    void Assets::update()
    {
        // A little at a time, so a lot arriving together doesn't hold up one frame.
        takeIn(kTakenInPerUpdate);
    }

    void Assets::takeIn(size_t bytes)
    {
        size_t taken = 0;
        // Oldest first: the list is newest first, so walk to the end.
        while (taken < bytes)
        {
            Load** oldest = nullptr;
            for (Load** link = &m_loading; *link; link = &(*link)->next)
            {
                if ((*link)->done.load(std::memory_order_acquire))
                {
                    oldest = link;
                }
            }
            if (oldest == nullptr)
            {
                return;
            }
            Load* load = *oldest;
            *oldest = load->next;
            m_pending--;
            taken += load->texture.pixels ? gpu::pixelBytes(load->texture.format, load->texture.fullWidth, load->texture.fullHeight) : 0;
            finishLoad(*load);
            destroy(load);
        }
    }

    void Assets::finish()
    {
        takeIn(~static_cast<size_t>(0));
        while (m_pending)
        {
            sleepMilliseconds(1);
            takeIn(~static_cast<size_t>(0));
        }
    }

    void Assets::finishLoad(Load& load)
    {
        Asset& asset = *load.asset;
        asset.fileTime = load.fileTime;
        if (load.type == AssetType::Sound)
        {
            if (load.sound == nullptr)
            {
                fail(asset);
                return;
            }
            if (asset.sound && asset.refs)
            {
                // Someone has taken it since the reload began, and may be playing it: try again later.
                freeSound(load.sound);
                asset.fileTime = 0;
                return;
            }
            makeRoom(AssetType::Sound, load.sound->bytes);
            unloadData(asset);
            asset.sound = load.sound;
            asset.bytes = load.sound->bytes;
        }
        else if (load.type == AssetType::Effect)
        {
            if (load.file.size == 0)
            {
                fail(asset);
                return;
            }
            const std::string source(reinterpret_cast<const char*>(load.file.data), load.file.size);
            const gpu::EffectKind kind = source.find("paint(") != std::string::npos ? gpu::EffectKind::Surface : gpu::EffectKind::Picture;
            char error[512] = "";
            const gpu::Effect made = m_device->createEffect(kind, source.c_str(), error, sizeof(error));
            if (!made && error[0])
            {
                // What worked before stays, while it is being written.
                NX_ERROR("%s: %s", load.path, error);
                if (!asset.effect)
                {
                    fail(asset);
                    return;
                }
            }
            else
            {
                m_device->destroyEffect(asset.effect);
                asset.effect = made;
            }
            asset.bytes = 1024;
        }
        else if (load.type == AssetType::Model)
        {
            if (load.model == nullptr || !uploadModel(*load.model, *m_device))
            {
                freeModel(load.model, *m_device);
                fail(asset);
                return;
            }
            if (asset.model && asset.refs)
            {
                // What shows it may be in the middle of moving: try again when it is let go of.
                freeModel(load.model, *m_device);
                asset.fileTime = 0;
                return;
            }
            makeRoom(AssetType::Model, load.model->bytes);
            unloadData(asset);
            asset.model = load.model;
            asset.bytes = load.model->bytes;
        }
        else
        {
            if (load.texture.pixels == nullptr)
            {
                fail(asset);
                return;
            }
            makeRoom(AssetType::Texture, gpu::pixelBytes(load.texture.format, load.texture.fullWidth, load.texture.fullHeight));
            const gpu::Texture texture = m_device->createTexture(load.texture);
            freeImage(load.decoded);
            if (!texture)
            {
                fail(asset);
                return;
            }
            unloadData(asset);
            asset.texture = texture;
            asset.width = load.texture.width;
            asset.height = load.texture.height;
            asset.bytes = m_device->textureInfo(texture).bytes;
        }
        asset.ready = true;
        asset.failed = false;
        m_bytes[budgetIndex(asset.type)] += asset.bytes;
    }

    // Remembered, so a missing file isn't looked for again every frame.
    void Assets::fail(Asset& asset)
    {
        if (asset.ready && !asset.failed)
        {
            return;  // a reload went wrong: keep what there was
        }
        asset.failed = true;
        asset.ready = true;
        asset.texture = m_missing;
        asset.width = asset.height = 16;
    }

    Asset* Assets::font(const char* path, u32 size)
    {
        char clean[128];
        char lower[128];
        char key[160];
        size = size < 4 ? 4 : (size > 160 ? 160 : size);
        const bool valid = cleanPath(path, clean, sizeof(clean), false) && cleanPath(path, lower, sizeof(lower), true);
        u32 pixels = static_cast<u32>(static_cast<float>(size) * m_fontScale + 0.5f);
        pixels = pixels < 8 ? 8 : (pixels > 190 ? 190 : pixels);
        // Small glyphs come out better at twice the size, halved when drawn.
        pixels = pixels < 20 ? pixels * 2 : pixels;
        snprintf(key, sizeof(key), "%s@%u/%u", valid ? lower : "?", static_cast<unsigned>(size), static_cast<unsigned>(pixels));
        const u64 hash = hashText(key);
        if (Asset* found = find(key, hash))
        {
            return found;
        }
        Asset* asset = add(key, hash, AssetType::Font);
        m_stats.loads++;
        if (!valid || !loadFont(*asset, clean, pixels))
        {
            NX_ERROR("cannot load font %s", path);
            fail(*asset);
            return asset;
        }
        asset->font->scale = static_cast<float>(size) / static_cast<float>(pixels);
        asset->ready = true;
        m_bytes[0] += asset->bytes;
        return asset;
    }

    bool Assets::loadFont(Asset& asset, const char* path, u32 size)
    {
        FileData file;
        if (!m_platform->readFile(path, file, MemTag::Texture))
        {
            return false;
        }
        FontData* font = create<FontData>(MemTag::UI);
        FontImage image;
        if (font == nullptr || !renderFont(file.data, file.size, size, *font, image))
        {
            destroy(font);
            return false;
        }
        makeRoom(AssetType::Font, static_cast<size_t>(image.width) * image.height * 4);
        const gpu::Texture texture = m_device->createTexture(gpu::rgbaTexture(image.pixels, image.width, image.height));
        memFree(image.pixels);
        if (!texture)
        {
            destroy(font);
            return false;
        }
        asset.texture = texture;
        asset.width = image.width;
        asset.height = image.height;
        asset.font = font;
        asset.bytes = m_device->textureInfo(texture).bytes + sizeof(FontData);
        return true;
    }

    void Assets::reloadChanged()
    {
        for (u32 i = 0; i < kCapacity; i++)
        {
            Asset& a = m_assets[i];
            if (a.live() && a.ready && a.path[0] && a.type != AssetType::Font)
            {
                // A sound may be playing from its memory, so only those that nobody holds.
                const u64 time = isWeb(a.path) ? 0 : m_platform->fileTime(a.path);
                if (time && time != a.fileTime && (a.type == AssetType::Texture || a.refs == 0) && a.type != AssetType::Font)
                {
                    NX_INFO("reloading %s", a.path);
                    a.fileTime = time;
                    startLoad(a);
                }
            }
        }
    }

    void Assets::release(Asset* asset)
    {
        NX_ASSERT(asset && asset->live() && asset->refs > 0);
        asset->refs--;
        asset->lastUsed = ++m_clock;
        if (asset->refs == 0)
        {
            makeRoom(asset->type, 0);
        }
    }

    void Assets::makeRoom(AssetType type, size_t bytes)
    {
        const size_t budget = budgetIndex(type);
        while (m_bytes[budget] + bytes > m_budget[budget] && evictOne(budget))
        {
        }
    }

    bool Assets::evictOne(size_t budget)
    {
        Asset* oldest = nullptr;
        for (u32 i = 0; i < kCapacity; i++)
        {
            Asset& a = m_assets[i];
            if (a.live() && a.ready && a.refs == 0 && budgetIndex(a.type) == budget && (oldest == nullptr || a.lastUsed < oldest->lastUsed))
            {
                oldest = &a;
            }
        }
        for (Load* load = m_loading; load && oldest; load = load->next)
        {
            oldest = load->asset == oldest ? nullptr : oldest;  // it is being loaded again
        }
        if (oldest == nullptr)
        {
            return false;
        }
        m_evicted[m_evictedNext] = { oldest->hash, m_platform->seconds() };
        m_evictedNext = (m_evictedNext + 1) % kEvictedMemory;
        m_stats.evictions++;
        unload(*oldest);
        return true;
    }

    bool Assets::evictAny()
    {
        return evictOne(0) || evictOne(1);
    }

    void Assets::unloadData(Asset& asset)
    {
        if (asset.texture != m_missing && asset.texture != m_invisible)
        {
            m_device->destroyTexture(asset.texture);
        }
        asset.texture = m_invisible;
        destroy(asset.font);
        freeSound(asset.sound);
        freeModel(asset.model, *m_device);
        asset.model = nullptr;
        m_device->destroyEffect(asset.effect);
        asset.effect = {};
        asset.font = nullptr;
        asset.sound = nullptr;
        m_bytes[budgetIndex(asset.type)] -= asset.bytes;
        asset.bytes = 0;
    }

    void Assets::unload(Asset& asset)
    {
        unloadData(asset);
        asset = Asset();
    }

    void Assets::trim()
    {
        m_stats.trims++;
        finish();
        for (u32 i = 0; i < kCapacity; i++)
        {
            if (m_assets[i].live() && m_assets[i].refs == 0)
            {
                unload(m_assets[i]);
            }
        }
    }

    void Assets::setBudget(AssetType type, size_t bytes)
    {
        m_budget[budgetIndex(type)] = bytes;
        makeRoom(type, 0);
    }

    AssetStats Assets::stats() const
    {
        AssetStats s = m_stats;
        forEach([&](const Asset& a)
        {
            s.typeBytes[static_cast<size_t>(a.type)] += a.bytes;
            if (a.refs)
            {
                s.inUse++;
                s.inUseBytes += a.bytes;
            }
            else
            {
                s.warm++;
                s.warmBytes += a.bytes;
            }
        });
        return s;
    }
}
