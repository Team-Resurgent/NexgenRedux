#include "assets/decode.h"
#include "bindings/script_refs.h"
#include "net/digest.h"
#include "save/save.h"

#include <math.h>
#include <string.h>

// Pixels that scripts read and write, kept as PNGs; bytes squeezed, fingerprinted and written as text; and
// random numbers of their own, from a seed.
namespace nx
{
    struct Pixels
    {
        int refs = 1;
        u32 width = 0, height = 0;
        u8* rgba = nullptr;  // top row first
        Assets* assets = nullptr;
        Asset* texture = nullptr;  // made when it is first drawn
        bool changed = true;

        void addRef() { refs++; }
        void release()
        {
            if (--refs == 0)
            {
                memFree(rgba);
                if (texture)
                {
                    assets->release(texture);
                }
                destroy(this);
            }
        }
    };

    namespace
    {
        constexpr u32 kMostPixels = 2048;

        u8 toByte(float v) { return static_cast<u8>(v <= 0 ? 0 : v >= 1 ? 255 : v * 255.0f + 0.5f); }

        Pixels* adopt(u8* rgba, u32 width, u32 height)
        {
            Pixels* p = create<Pixels>(MemTag::Script);
            if (p == nullptr)
            {
                memFree(rgba);
                return nullptr;
            }
            p->rgba = rgba;
            p->width = width;
            p->height = height;
            p->assets = services().assets;
            return p;
        }

        Pixels* makePixels(u32 width, u32 height, const Color& fill)
        {
            if (width == 0 || height == 0 || width > kMostPixels || height > kMostPixels)
            {
                scriptError("pixels are 1 to 2048 a side");
                return nullptr;
            }
            const size_t count = static_cast<size_t>(width) * height;
            u8* rgba = static_cast<u8*>(memAlloc(count * 4, MemTag::Script));
            if (rgba == nullptr)
            {
                scriptError("there isn't the memory for so many pixels");
                return nullptr;
            }
            const u8 c[4] = { toByte(fill.r), toByte(fill.g), toByte(fill.b), toByte(fill.a) };
            for (size_t i = 0; i < count; i++)
            {
                memcpy(rgba + i * 4, c, 4);
            }
            return adopt(rgba, width, height);
        }
        Pixels* makeClear(u32 width, u32 height) { return makePixels(width, height, Color{ 0, 0, 0, 0 }); }

        // "saved:shot.png" is a PNG kept with the saves, under the running screen's name.
        bool savedName(const Services& s, const std::string& name, std::string& out)
        {
            if (name.size() < 11 || name.compare(0, 6, "saved:") != 0 || name.compare(name.size() - 4, 4, ".png") != 0)
            {
                return false;
            }
            const char* slash = strrchr(s.folder, '/');
            out = std::string(slash ? slash + 1 : s.folder) + ".";
            for (size_t i = 6; i < name.size(); i++)
            {
                const char c = name[i];
                if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.'))
                {
                    return false;
                }
                out += c;
            }
            return out.size() <= 40;
        }

        Pixels* loadPixels(const std::string& name)
        {
            Services& s = services();
            FileData file;
            std::string kept;
            const bool saved = name.compare(0, 6, "saved:") == 0;
            if (saved && !savedName(s, name, kept))
            {
                scriptError("saved pictures have short, plain names that end in .png (" + name + ")");
                return nullptr;
            }
            if (saved ? !s.platform->loadSave(kept.c_str(), file) : !readOwn(s, name.c_str(), file))
            {
                return nullptr;  // not there: the script can tell
            }
            u32 w = 0, h = 0;
            if (!imageSize(file.data, file.size, w, h) || w > kMostPixels || h > kMostPixels)
            {
                scriptError(name + " isn't a picture of up to 2048 pixels a side");
                return nullptr;
            }
            u8* rgba = decodeImage(file.data, file.size, w, h);
            if (rgba == nullptr)
            {
                scriptError(name + " can't be read as a picture");
                return nullptr;
            }
            return adopt(rgba, w, h);
        }

        u32 pixelWidth(const Pixels* p) { return p->width; }
        u32 pixelHeight(const Pixels* p) { return p->height; }
        bool inside(const Pixels* p, u32 x, u32 y) { return x < p->width && y < p->height; }

        Color pixelGet(u32 x, u32 y, const Pixels* p)
        {
            if (!inside(p, x, y))
            {
                return Color{ 0, 0, 0, 0 };
            }
            const u8* c = p->rgba + (static_cast<size_t>(y) * p->width + x) * 4;
            return Color{ c[0] / 255.0f, c[1] / 255.0f, c[2] / 255.0f, c[3] / 255.0f };
        }

        void pixelSet(u32 x, u32 y, const Color& color, Pixels* p)
        {
            if (inside(p, x, y))
            {
                u8* c = p->rgba + (static_cast<size_t>(y) * p->width + x) * 4;
                c[0] = toByte(color.r);
                c[1] = toByte(color.g);
                c[2] = toByte(color.b);
                c[3] = toByte(color.a);
                p->changed = true;
            }
        }

        void pixelFill(const Color& color, Pixels* p)
        {
            const u8 c[4] = { toByte(color.r), toByte(color.g), toByte(color.b), toByte(color.a) };
            const size_t count = static_cast<size_t>(p->width) * p->height;
            for (size_t i = 0; i < count; i++)
            {
                memcpy(p->rgba + i * 4, c, 4);
            }
            p->changed = true;
        }

        void pixelPaste(const Pixels* from, int x, int y, Pixels* p)
        {
            if (from == nullptr)
            {
                return;
            }
            for (u32 row = 0; row < from->height; row++)
            {
                const int ty = y + static_cast<int>(row);
                if (ty < 0 || ty >= static_cast<int>(p->height))
                {
                    continue;
                }
                const int x0 = x < 0 ? -x : 0;
                const int x1 = static_cast<int>(from->width) < static_cast<int>(p->width) - x ? static_cast<int>(from->width) : static_cast<int>(p->width) - x;
                if (x1 > x0)
                {
                    memmove(p->rgba + (static_cast<size_t>(ty) * p->width + x + x0) * 4, from->rgba + (static_cast<size_t>(row) * from->width + x0) * 4, static_cast<size_t>(x1 - x0) * 4);
                }
            }
            p->changed = true;
        }

        Pixels* pixelCopy(const Pixels* p)
        {
            const size_t bytes = static_cast<size_t>(p->width) * p->height * 4;
            u8* rgba = static_cast<u8*>(memAlloc(bytes, MemTag::Script));
            if (rgba == nullptr)
            {
                scriptError("there isn't the memory for so many pixels");
                return nullptr;
            }
            memcpy(rgba, p->rgba, bytes);
            return adopt(rgba, p->width, p->height);
        }

        // A texture of them, sent again to the GPU when they have changed since it was last asked for.
        TextureRef* pixelTexture(Pixels* p)
        {
            Assets& assets = *p->assets;
            if (p->texture == nullptr && (p->texture = assets.dynamicTexture(p->width, p->height)) == nullptr)
            {
                return nullptr;
            }
            gpu::Device& device = assets.device();
            if (p->changed)
            {
                u32 pitch = 0;
                if (u8* memory = device.lockTexture(p->texture->texture, pitch))
                {
                    for (u32 y = 0; y < p->height; y++)
                    {
                        const u8* from = p->rgba + static_cast<size_t>(y) * p->width * 4;
                        u8* to = memory + static_cast<size_t>(y) * pitch;
                        for (u32 x = 0; x < p->width; x++)
                        {
                            to[x * 4 + 0] = from[x * 4 + 2];
                            to[x * 4 + 1] = from[x * 4 + 1];
                            to[x * 4 + 2] = from[x * 4 + 0];
                            to[x * 4 + 3] = from[x * 4 + 3];
                        }
                    }
                    device.unlockTexture(p->texture->texture);
                }
                else
                {
                    device.updateTexture(p->texture->texture, p->rgba);
                }
                p->changed = false;
            }
            assets.addRef(p->texture);
            TextureRef* handle = create<TextureRef>(MemTag::Script);
            if (handle == nullptr)
            {
                assets.release(p->texture);
                return nullptr;
            }
            handle->asset = p->texture;
            handle->owner = &assets;
            handle->refs = 1;
            return handle;
        }

        std::string pixelPng(const Pixels* p)
        {
            size_t size = 0;
            u8* png = encodePng(p->rgba, p->width, p->height, size);
            std::string out(reinterpret_cast<const char*>(png), png ? size : 0);
            freeImage(png);
            return out;
        }

        bool pixelSave(const std::string& name, const Pixels* p)
        {
            Services& s = services();
            std::string kept;
            if (!savedName(s, name, kept))
            {
                scriptError("pictures are saved as saved:, then a short, plain name that ends in .png (" + name + ")");
                return false;
            }
            if (s.save->readOnly())
            {
                return true;
            }
            const std::string png = pixelPng(p);
            return !png.empty() && s.platform->writeSave(kept.c_str(), png.data(), png.size());
        }

        // --- Bytes ---

        std::string compressText(const std::string& text, int level)
        {
            size_t size = 0;
            u8* out = compressBytes(reinterpret_cast<const u8*>(text.data()), text.size(), size, level);
            std::string result(reinterpret_cast<const char*>(out), out ? size : 0);
            freeImage(out);
            return result;
        }

        std::string uncompressText(const std::string& text)
        {
            size_t size = 0;
            u8* out = uncompressBytes(reinterpret_cast<const u8*>(text.data()), text.size(), size);
            if (out == nullptr)
            {
                scriptError("that isn't what compress() makes");
                return std::string();
            }
            std::string result(reinterpret_cast<const char*>(out), size);
            freeImage(out);
            return result;
        }

        std::string hexOf(const u8* bytes, size_t count)
        {
            static const char kDigits[] = "0123456789abcdef";
            std::string out(count * 2, '0');
            for (size_t i = 0; i < count; i++)
            {
                out[i * 2] = kDigits[bytes[i] >> 4];
                out[i * 2 + 1] = kDigits[bytes[i] & 15];
            }
            return out;
        }

        std::string md5Text(const std::string& text) { u8 d[16]; md5(text.data(), text.size(), d); return hexOf(d, 16); }
        std::string sha1Text(const std::string& text) { u8 d[20]; sha1(text.data(), text.size(), d); return hexOf(d, 20); }
        std::string sha256Text(const std::string& text) { u8 d[32]; sha256(text.data(), text.size(), d); return hexOf(d, 32); }
        std::string hexText(const std::string& text) { return hexOf(reinterpret_cast<const u8*>(text.data()), text.size()); }

        int digit(char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; }

        std::string unhexText(const std::string& text)
        {
            std::string out;
            if (text.size() % 2 == 0)
            {
                out.resize(text.size() / 2);
                for (size_t i = 0; i < out.size(); i++)
                {
                    const int high = digit(text[i * 2]), low = digit(text[i * 2 + 1]);
                    if (high < 0 || low < 0)
                    {
                        out.clear();
                        break;
                    }
                    out[i] = static_cast<char>(high * 16 + low);
                }
                if (!out.empty() || text.empty())
                {
                    return out;
                }
            }
            scriptError("that isn't hex: pairs of 0 to 9 and a to f");
            return std::string();
        }

        const char kBase64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

        std::string base64Text(const std::string& text)
        {
            std::string out;
            out.reserve((text.size() + 2) / 3 * 4);
            const u8* b = reinterpret_cast<const u8*>(text.data());
            for (size_t i = 0; i < text.size(); i += 3)
            {
                const u32 n = (static_cast<u32>(b[i]) << 16) | (i + 1 < text.size() ? static_cast<u32>(b[i + 1]) << 8 : 0) | (i + 2 < text.size() ? b[i + 2] : 0);
                out += kBase64[(n >> 18) & 63];
                out += kBase64[(n >> 12) & 63];
                out += i + 1 < text.size() ? kBase64[(n >> 6) & 63] : '=';
                out += i + 2 < text.size() ? kBase64[n & 63] : '=';
            }
            return out;
        }

        std::string unbase64Text(const std::string& text)
        {
            std::string out;
            u32 bits = 0;
            int have = 0;
            bool padded = false;
            for (const char c : text)
            {
                if (c == '\r' || c == '\n' || c == ' ')
                {
                    continue;
                }
                if (c == '=')
                {
                    padded = true;
                    continue;
                }
                const char* at = c ? strchr(kBase64, c) : nullptr;
                // The web's kind, with - and _, too.
                const int value = at ? static_cast<int>(at - kBase64) : c == '-' ? 62 : c == '_' ? 63 : -1;
                if (value < 0 || padded)
                {
                    scriptError("that isn't base64");
                    return std::string();
                }
                bits = (bits << 6) | static_cast<u32>(value);
                have += 6;
                if (have >= 8)
                {
                    have -= 8;
                    out += static_cast<char>((bits >> have) & 255);
                }
            }
            return out;
        }

        // --- Random numbers of a script's own ---

        // xoshiro128**, seeded through splitmix: the same seed gives the same numbers on every machine.
        struct RandomRef
        {
            int refs = 1;
            u32 seed = 0;
            u32 s[4] = {};
            bool hasSpare = false;
            float spare = 0;

            void addRef() { refs++; }
            void release()
            {
                if (--refs == 0)
                {
                    destroy(this);
                }
            }

            void reseed(u32 value)
            {
                seed = value;
                u64 x = value;
                for (u32& part : s)
                {
                    x += 0x9E3779B97F4A7C15ull;
                    u64 z = x;
                    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
                    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
                    part = static_cast<u32>((z ^ (z >> 31)) >> 16);
                }
                hasSpare = false;
            }

            u32 next()
            {
                const u32 result = rotate(s[1] * 5, 7) * 9;
                const u32 t = s[1] << 9;
                s[2] ^= s[0];
                s[3] ^= s[1];
                s[1] ^= s[2];
                s[0] ^= s[3];
                s[2] ^= t;
                s[3] = rotate(s[3], 11);
                return result;
            }
            static u32 rotate(u32 x, int k) { return (x << k) | (x >> (32 - k)); }

            float unit() { return static_cast<float>(next() >> 8) / 16777216.0f; }
        };

        RandomRef* makeRandom(u32 seed)
        {
            RandomRef* r = create<RandomRef>(MemTag::Script);
            if (r)
            {
                r->reseed(seed);
            }
            return r;
        }
        RandomRef* makeRandomAny()
        {
            u32& g = services().random;
            g ^= g << 13;
            g ^= g >> 17;
            g ^= g << 5;
            return makeRandom(g);
        }

        float randomUnit(RandomRef* r) { return r->unit(); }
        float randomBetween(float low, float high, RandomRef* r) { return low + (high - low) * r->unit(); }
        int randomWhole(int low, int high, RandomRef* r)
        {
            if (high <= low)
            {
                return low;
            }
            const u64 span = static_cast<u64>(static_cast<i64>(high) - low) + 1;
            return static_cast<int>(low + static_cast<i64>(r->next() % span));
        }

        // Box and Muller's way: two at a time, the second kept for next time.
        float normalOf(float (*unit)(void*), void* from, bool& hasSpare, float& spare, float mean, float deviation)
        {
            if (hasSpare)
            {
                hasSpare = false;
                return mean + spare * deviation;
            }
            float u = 0;
            while (u <= 1e-7f)
            {
                u = unit(from);
            }
            const float v = unit(from);
            const float radius = sqrtf(-2.0f * logf(u));
            spare = radius * sinf(6.2831853f * v);
            hasSpare = true;
            return mean + radius * cosf(6.2831853f * v) * deviation;
        }

        float randomNormal(float mean, float deviation, RandomRef* r)
        {
            return normalOf([](void* p) { return static_cast<RandomRef*>(p)->unit(); }, r, r->hasSpare, r->spare, mean, deviation);
        }
        u32 randomSeed(const RandomRef* r) { return r->seed; }
        void randomSetSeed(u32 seed, RandomRef* r) { r->reseed(seed); }

        std::string randomState(const RandomRef* r) { return hexOf(reinterpret_cast<const u8*>(r->s), sizeof(r->s)); }
        void randomSetState(const std::string& text, RandomRef* r)
        {
            const std::string bytes = text.size() == sizeof(r->s) * 2 ? unhexText(text) : std::string();
            if (bytes.size() != sizeof(r->s))
            {
                scriptError("that isn't the state of a Random");
                return;
            }
            memcpy(r->s, bytes.data(), sizeof(r->s));
            r->hasSpare = false;
        }

        // The shared one's.
        bool g_hasSpare = false;
        float g_spare = 0;
        float sharedUnit(void*)
        {
            u32& g = services().random;
            g ^= g << 13;
            g ^= g >> 17;
            g ^= g << 5;
            return static_cast<float>(g >> 8) / 16777216.0f;
        }
        float sharedNormal(float mean, float deviation) { return normalOf(sharedUnit, nullptr, g_hasSpare, g_spare, mean, deviation); }
    }

    Pixels* takeScreenshot()
    {
        gpu::Device& device = *services().device;
        u32 w = 0, h = 0;
        device.size(w, h);
        const size_t bytes = static_cast<size_t>(w) * h * 4;
        u8* rgba = bytes ? static_cast<u8*>(memAlloc(bytes, MemTag::Script)) : nullptr;
        if (rgba == nullptr || !device.capture(rgba, bytes))
        {
            memFree(rgba);
            return nullptr;
        }
        return adopt(rgba, w, h);
    }

    void bindData(Binder& b)
    {
        b.section("Pixels", "Pictures as colours that a script reads and writes, one pixel at a time: made, loaded, drawn and saved.");
        b.refType("Pixels", 0, "A picture held as its pixels, from the top left, up to 2048 a side.");
        b.behaviour(asBEHAVE_FACTORY, "Pixels@ f(uint width, uint height)", NX_FN(makeClear));
        b.behaviour(asBEHAVE_FACTORY, "Pixels@ f(uint width, uint height, const Color &in fill)", NX_FN(makePixels));
        b.behaviour(asBEHAVE_ADDREF, "void f()", NX_METHOD(Pixels, addRef));
        b.behaviour(asBEHAVE_RELEASE, "void f()", NX_METHOD(Pixels, release));
        b.method("uint get_width() const property", NX_OBJ_LAST(pixelWidth), "Across.");
        b.method("uint get_height() const property", NX_OBJ_LAST(pixelHeight), "Down.");
        b.method("Color get(uint x, uint y) const", NX_OBJ_LAST(pixelGet), "One pixel's colour; see-through black outside.");
        b.method("void set(uint x, uint y, const Color &in color)", NX_OBJ_LAST(pixelSet), "Changes one; nothing outside.");
        b.method("void fill(const Color &in color)", NX_OBJ_LAST(pixelFill), "Every one the same.");
        b.method("void paste(const Pixels@+ from, int x, int y)", NX_OBJ_LAST(pixelPaste), "Another's pixels over these, its top left at x, y, as they are: what is see-through in it is see-through here.");
        b.method("Pixels@ copy() const", NX_OBJ_LAST(pixelCopy), "Another the same, of its own.");
        b.method("Texture@ get_texture() property", NX_OBJ_LAST(pixelTexture), "To draw them: the same texture each time, sent to the GPU again if a pixel has changed since it was last asked for. Change many, then draw. Null where the GPU can't.");
        b.method("string png() const", NX_OBJ_LAST(pixelPng), "As a PNG file, in a string: to send, or to keep in a save.");
        b.method("bool save(const string &in name)", NX_OBJ_LAST(pixelSave), "Keeps them as a PNG with the saves: `saved:` and a short, plain name that ends in .png. Assets::pixels reads it back. On a computer it is a file in the saves' folder. False if it couldn't be.");
        b.names("Assets");
        b.function("Pixels@ pixels(const string &in name)", NX_FN(loadPixels), "A picture's pixels, read now: `asset:`, `global:` or `own:`, or `saved:` for one that Pixels.save kept. Null if there is none.");
        b.names("");

        b.section("Data", "Bytes in strings: squeezed, fingerprinted, and written as text. A string can hold any bytes.");
        b.names("Data");
        b.function("string compress(const string &in bytes, int level = 6)", NX_FN(compressText), "Smaller, in zlib's format, which most languages can read: level 1 is quickest, 9 smallest.");
        b.function("string uncompress(const string &in bytes)", NX_FN(uncompressText), "As it was before compress(), or what zlib made.");
        b.function("string md5(const string &in bytes)", NX_FN(md5Text), "A fingerprint, as 32 hex digits: the same bytes give the same one. For telling whether a file has changed, not for secrets.");
        b.function("string sha1(const string &in bytes)", NX_FN(sha1Text), "The same, 40 digits.");
        b.function("string sha256(const string &in bytes)", NX_FN(sha256Text), "The same, 64 digits, and hard to forge.");
        b.function("string base64(const string &in bytes)", NX_FN(base64Text), "Any bytes as letters, digits, + and /: for JSON, the web, or copying by hand.");
        b.function("string unbase64(const string &in text)", NX_FN(unbase64Text), "The bytes again; the web's kind, with - and _, too.");
        b.function("string hex(const string &in bytes)", NX_FN(hexText), "Each byte as two hex digits.");
        b.function("string unhex(const string &in text)", NX_FN(unhexText), "The bytes again.");
        b.names("");

        b.section("Random", "Random numbers of a script's own, from a seed: the same seed gives the same numbers, in the same order, on every machine. For worlds made from a number, replays, and dealing the same cards twice.");
        b.refType("Random", 0, "A generator of its own.");
        b.behaviour(asBEHAVE_FACTORY, "Random@ f()", NX_FN(makeRandomAny));
        b.behaviour(asBEHAVE_FACTORY, "Random@ f(uint seed)", NX_FN(makeRandom));
        b.behaviour(asBEHAVE_ADDREF, "void f()", NX_METHOD(RandomRef, addRef));
        b.behaviour(asBEHAVE_RELEASE, "void f()", NX_METHOD(RandomRef, release));
        b.method("float random()", NX_OBJ_LAST(randomUnit), "From 0 up to 1.");
        b.method("float random(float low, float high)", NX_OBJ_LAST(randomBetween), "Between two numbers.");
        b.method("int randomInt(int low, int high)", NX_OBJ_LAST(randomWhole), "A whole number from low to high, including both.");
        b.method("float normal(float mean = 0, float deviation = 1)", NX_OBJ_LAST(randomNormal), "Mostly near the mean and seldom far from it, in a bell curve: about two in three within one deviation. For heights, aim, damage that varies.");
        b.method("uint get_seed() const property", NX_OBJ_LAST(randomSeed), "What it started from.");
        b.method("void set_seed(uint) property", NX_OBJ_LAST(randomSetSeed), "Starts again from a seed.");
        b.method("string get_state() const property", NX_OBJ_LAST(randomState), "Where it has got to, as text, to keep in a save.");
        b.method("void set_state(const string &in) property", NX_OBJ_LAST(randomSetState), "Goes on from where it had got to.");
        b.function("float randomNormal(float mean = 0, float deviation = 1)", NX_FN(sharedNormal), "From the shared generator, as Random.normal.");
    }
}
