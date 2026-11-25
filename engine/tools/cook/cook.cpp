// Prepares media for a platform, on the machine that builds the game: a folder of it, or one file.
//
// Images become compressed textures, sounds become ADPCM and models of every kind become .glb, under the
// same names, so scripts don't change. Whatever wouldn't get smaller is copied. Only files that have changed
// are done again, and files whose originals are gone are removed.
#include "assets/decode.h"
#include "assets/formats.h"
#include "assets/import.h"
#include "assets/model.h"
#include "audio/decoder.h"
#include "base/log.h"
#include "base/memory.h"

#include "third_party/stb/stb_dxt.h"

#include <filesystem>
#include <fstream>
#include <set>
#include <stdio.h>
#include <string.h>
#include <string>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;
using namespace nx;

namespace
{
    // Goes up when the output changes, so everything is done again.
    constexpr u32 kVersion = 2;
    // Compression suits photographs and large art. Small images cost little and show every flaw.
    constexpr u32 kSmallestCompressed = 64 * 64;
    // Longer sounds stay as they are: music is smaller as Ogg or MP3 than as ADPCM.
    constexpr u32 kLongestAdpcmBytes = 512 * 1024;

    using Bytes = std::vector<u8>;

    struct Totals
    {
        u32 done = 0, copied = 0, kept = 0, removed = 0, failed = 0;
        u64 before = 0, after = 0;
    };

    bool readAll(const fs::path& path, Bytes& out)
    {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file)
        {
            return false;
        }
        out.resize(static_cast<size_t>(file.tellg()));
        file.seekg(0);
        return out.empty() || static_cast<bool>(file.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(out.size())));
    }

    void put16(Bytes& out, u32 value)
    {
        out.push_back(static_cast<u8>(value));
        out.push_back(static_cast<u8>(value >> 8));
    }

    void put32(Bytes& out, u32 value)
    {
        put16(out, value);
        put16(out, value >> 16);
    }

    u32 powerOfTwo(u32 value)
    {
        u32 p = 4;
        while (p < value)
        {
            p *= 2;
        }
        return p;
    }

    bool isImage(const std::string& extension)
    {
        return extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".tga" || extension == ".bmp" || extension == ".gif";
    }

    bool isSound(const std::string& extension)
    {
        return extension == ".wav" || extension == ".ogg" || extension == ".mp3";
    }

    // False if the file is broken. Leaves out empty if it is better copied.
    bool cookImage(const Bytes& in, bool powersOfTwo, Bytes& out)
    {
        u32 width = 0, height = 0;
        u8* pixels = imageSize(in.data(), in.size(), width, height) && gpu::validTextureSize(width, height) ? decodeImage(in.data(), in.size(), width, height) : nullptr;
        if (pixels == nullptr)
        {
            return false;
        }
        bool solid = true;
        for (size_t i = 0; i < static_cast<size_t>(width) * height; i++)
        {
            solid = solid && pixels[i * 4 + 3] == 255;
        }
        const gpu::PixelFormat format = solid ? gpu::PixelFormat::Bc1 : gpu::PixelFormat::Bc3;
        const u32 fullWidth = powersOfTwo ? powerOfTwo(width) : (width + 3) / 4 * 4;
        const u32 fullHeight = powersOfTwo ? powerOfTwo(height) : (height + 3) / 4 * 4;
        const size_t compressed = gpu::pixelBytes(format, fullWidth, fullHeight);
        if (width * height >= kSmallestCompressed && gpu::validTextureSize(fullWidth, fullHeight) && compressed < static_cast<size_t>(width) * height * 4)
        {
            put32(out, kTextureMagic);
            put16(out, width);
            put16(out, height);
            put16(out, fullWidth);
            put16(out, fullHeight);
            put32(out, static_cast<u32>(format));
            for (u32 y = 0; y < fullHeight; y += 4)
            {
                for (u32 x = 0; x < fullWidth; x += 4)
                {
                    u8 block[16 * 4];
                    for (u32 i = 0; i < 16; i++)
                    {
                        // The padding repeats the edge, so filtering doesn't bring in anything else.
                        const u32 px = x + i % 4 < width ? x + i % 4 : width - 1;
                        const u32 py = y + i / 4 < height ? y + i / 4 : height - 1;
                        memcpy(block + static_cast<size_t>(i) * 4, pixels + (static_cast<size_t>(py) * width + px) * 4, 4);
                    }
                    u8 packed[16];
                    stb_compress_dxt_block(packed, block, solid ? 0 : 1, STB_DXT_HIGHQUAL);
                    out.insert(out.end(), packed, packed + (solid ? 8 : 16));
                }
            }
        }
        freeImage(pixels);
        return true;
    }

    bool cookSound(const Bytes& in, bool alreadyCompressed, Bytes& out)
    {
        SoundData* sound = loadSound(in.data(), in.size());
        if (sound == nullptr)
        {
            return false;
        }
        u32 frames = 0;
        i16* samples = decodeAll(*sound, frames);
        const u32 channels = sound->channels;
        const u32 rate = sound->rate;
        freeSound(sound);
        if (samples == nullptr || frames == 0)
        {
            memFree(samples);
            return false;
        }
        const size_t blocks = (frames + kAdpcmFrames - 1) / kAdpcmFrames;
        const size_t size = kAdpcmHeader + blocks * adpcmBlockBytes(channels);
        // ADPCM costs far less to play than Ogg or MP3, so short ones are worth a little more memory.
        if (size < in.size() || (alreadyCompressed && size <= kLongestAdpcmBytes))
        {
            put32(out, kAdpcmMagic);
            put32(out, rate);
            put32(out, frames);
            put32(out, channels);
            int steps[2] = {};
            for (u32 at = 0; at < frames; at += kAdpcmFrames)
            {
                u8 block[kAdpcmChannelBytes * 2];
                adpcmEncodeBlock(samples + static_cast<size_t>(at) * channels, frames - at < kAdpcmFrames ? frames - at : kAdpcmFrames, channels, steps, block);
                out.insert(out.end(), block, block + adpcmBlockBytes(channels));
            }
        }
        memFree(samples);
        return true;
    }

    // What it names is beside it, as it was, not as it is cooked.
    bool readBeside(const char* name, Bytes& out, void* user)
    {
        return readAll(*static_cast<const fs::path*>(user) / fs::path(name), out);
    }

    bool cookModel(const fs::path& source, const Bytes& in, Bytes& out, std::string& why)
    {
        fs::path folder = source.parent_path();
        if (!importModel(source.filename().string().c_str(), in.data(), in.size(), readBeside, &folder, out, why))
        {
            return false;
        }
        // One that the engine would refuse is better known of now.
        ModelData* model = loadModel(out.data(), out.size(), "model");
        if (model == nullptr)
        {
            why = "the engine can't use it: up to 65536 vertices and 128 moving nodes";
            return false;
        }
        destroy(model);
        return true;
    }

    std::string lower(std::string text)
    {
        for (char& c : text)
        {
            c = c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
        }
        return text;
    }

    const char kHelp[] =
        "Prepares media for Nexgen: images, sounds and 3D models.\n"
        "\n"
        "  cook [options] <media folder> <new folder>    a folder, and all in it, under the same names\n"
        "  cook [options] <file> [<file or folder>]      one file; a model becomes a .glb\n"
        "\n"
        "  -p, --platform NAME   desktop, or xboxog for the original Xbox (desktop if not said)\n"
        "  -s, --skip WHAT       leave images, sounds or models as they are; can be given again\n"
        "  -f, --force           do everything again, not only what changed\n"
        "  -n, --dry-run         say what would be done, and write nothing\n"
        "  -q, --quiet           only errors and the last line\n"
        "  -j, --json            a line of JSON for each file, for other programs\n"
        "  -h, --help\n"
        "      --version\n"
        "\n"
        "Models: glTF (.glb, .gltf), FBX, OBJ, STL, PLY. Images: PNG, JPEG, TGA, BMP, GIF. Sounds: WAV, Ogg, MP3.\n"
        "0 if all went well, 1 if something failed, 2 if it was asked wrongly.\n";

    struct Options
    {
        bool xbox = false, force = false, dry = false, quiet = false, json = false;
        bool images = true, sounds = true, models = true;
        std::vector<std::string> paths;
    };

    Options g_options;

    std::string quoted(const std::string& text)
    {
        std::string out = "\"";
        for (const char c : text)
        {
            if (c == '"' || c == '\\')
            {
                out += '\\';
            }
            out += static_cast<unsigned char>(c) < 32 ? ' ' : c;
        }
        return out + "\"";
    }

    std::string utf8(const fs::path& path)
    {
        const auto text = path.generic_u8string();
        return std::string(text.begin(), text.end());
    }

    // What was done with a file: "image", "sound", "model", "copied", "unchanged", "removed" or "failed".
    void tell(const fs::path& name, const char* what, u64 before, u64 after, const std::string& why = "")
    {
        if (g_options.json)
        {
            printf("{\"file\":%s,\"what\":\"%s\",\"before\":%llu,\"after\":%llu%s%s}\n", quoted(utf8(name)).c_str(), what, static_cast<unsigned long long>(before), static_cast<unsigned long long>(after),
                why.empty() ? "" : ",\"why\":", why.empty() ? "" : quoted(why).c_str());
        }
        else if (!why.empty())
        {
            fprintf(stderr, "%s: %s\n", utf8(name).c_str(), why.c_str());
        }
        else if (!g_options.quiet && strcmp(what, "unchanged") != 0)
        {
            printf("%-9s %s  %llu KB -> %llu KB\n", what, utf8(name).c_str(), static_cast<unsigned long long>((before + 1023) / 1024), static_cast<unsigned long long>((after + 1023) / 1024));
        }
        fflush(stdout);
    }

    // What to do with a file, and what comes of it. Nothing in `out`: it stays as it is.
    const char* cookOne(const fs::path& source, const Bytes& in, Bytes& out, std::string& why)
    {
        const std::string extension = lower(utf8(source.extension()));
        if (isImage(extension) && g_options.images)
        {
            if (!cookImage(in, g_options.xbox, out))
            {
                why = "the image can't be read, or is more than 2048 pixels a side";
                return "failed";
            }
            return out.empty() ? "copied" : "image";
        }
        if (isSound(extension) && g_options.sounds)
        {
            if (!cookSound(in, extension != ".wav", out))
            {
                why = "the sound can't be read";
                return "failed";
            }
            return out.empty() ? "copied" : "sound";
        }
        if (modelKindByName(utf8(source).c_str()) > ModelKind::Glb && g_options.models)
        {
            return cookModel(source, in, out, why) ? "model" : "failed";
        }
        return "copied";
    }

    // Written beside the old one and swapped in, so that stopping never leaves half a file.
    bool writeAll(const fs::path& path, const Bytes& bytes)
    {
        std::error_code error;
        fs::create_directories(path.parent_path(), error);
        fs::path fresh = path;
        fresh += ".new";
        {
            std::ofstream file(fresh, std::ios::binary | std::ios::trunc);
            file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
            file.close();
            if (!file)
            {
                return false;
            }
        }
        fs::rename(fresh, path, error);
        return !error;
    }

    int finish(const Totals& t)
    {
        if (g_options.json)
        {
            printf("{\"done\":true,\"cooked\":%u,\"copied\":%u,\"unchanged\":%u,\"removed\":%u,\"failed\":%u,\"before\":%llu,\"after\":%llu}\n", static_cast<unsigned>(t.done), static_cast<unsigned>(t.copied), static_cast<unsigned>(t.kept),
                static_cast<unsigned>(t.removed), static_cast<unsigned>(t.failed), static_cast<unsigned long long>(t.before), static_cast<unsigned long long>(t.after));
        }
        else
        {
            printf("%u cooked, %u copied, %u unchanged, %u removed, %u failed; %u KB became %u KB\n", static_cast<unsigned>(t.done), static_cast<unsigned>(t.copied), static_cast<unsigned>(t.kept),
                static_cast<unsigned>(t.removed), static_cast<unsigned>(t.failed), static_cast<unsigned>(t.before / 1024), static_cast<unsigned>(t.after / 1024));
        }
        return t.failed ? 1 : 0;
    }

    int stop(const std::string& why)
    {
        if (g_options.json)
        {
            printf("{\"error\":%s}\n", quoted(why).c_str());
        }
        fprintf(stderr, "%s\n", why.c_str());
        return 1;
    }

    int cookFile(const fs::path& from, fs::path to)
    {
        const ModelKind kind = modelKindByName(utf8(from).c_str());
        std::error_code error;
        fs::path name = from.filename();
        if (kind != ModelKind::None)
        {
            name.replace_extension(".glb");
        }
        to = to.empty() ? from.parent_path() / name : fs::is_directory(to, error) ? to / name : to;
        if (fs::exists(to, error) && fs::equivalent(from, to, error))
        {
            return stop(utf8(to) + " would be written over itself: say what to make");
        }
        Bytes in, out;
        std::string why;
        Totals t;
        const bool read = readAll(from, in);
        const char* what = read ? cookOne(from, in, out, why) : "failed";
        why = read ? why : "it can't be read";
        const Bytes& write = out.empty() ? in : out;
        if (strcmp(what, "failed") != 0 && !g_options.dry && !writeAll(to, write))
        {
            what = "failed";
            why = utf8(to) + " can't be written";
        }
        const bool failed = strcmp(what, "failed") == 0;
        t.failed = failed;
        t.copied = strcmp(what, "copied") == 0;
        t.done = !failed && !t.copied;
        t.before = in.size();
        t.after = failed ? 0 : write.size();
        tell(failed ? from : to, what, t.before, t.after, why);
        return finish(t);
    }

    int cookFolder(const fs::path& from, const fs::path& to)
    {
        std::error_code error;
        const fs::path inside = fs::weakly_canonical(to, error).lexically_relative(fs::weakly_canonical(from, error));
        if (!inside.empty() && *inside.begin() != "..")
        {
            return stop("the folder to make must be outside " + utf8(from));
        }
        if (!g_options.dry)
        {
            fs::create_directories(to, error);
        }

        // Never clear out a folder that something else made.
        const fs::path stamp = to / ".cooked";
        const std::string skips = std::string(g_options.images ? "" : " -images") + (g_options.sounds ? "" : " -sounds") + (g_options.models ? "" : " -models");
        const std::string stampText = "nexgen cook " + std::to_string(kVersion) + (g_options.xbox ? " xboxog" : " desktop") + skips + "\n";
        bool again = true;
        Bytes old;
        if (readAll(stamp, old))
        {
            again = g_options.force || std::string(old.begin(), old.end()) != stampText;
        }
        else if (fs::exists(to, error) && !fs::is_empty(to, error))
        {
            return stop(utf8(to) + " was not made by this tool: choose a new folder");
        }
        if (!g_options.dry)
        {
            std::ofstream(stamp, std::ios::binary) << stampText;
        }

        std::vector<fs::path> sources;
        for (auto it = fs::recursive_directory_iterator(from, error); !error && it != fs::recursive_directory_iterator(); it.increment(error))
        {
            if (it->is_regular_file(error))
            {
                sources.push_back(it->path());
            }
        }
        if (error)
        {
            return stop(utf8(from) + ": " + error.message());
        }
        if (g_options.json)
        {
            printf("{\"files\":%u}\n", static_cast<unsigned>(sources.size()));
            fflush(stdout);
        }

        Totals totals;
        std::set<fs::path> made;
        for (const fs::path& source : sources)
        {
            const fs::path name = fs::relative(source, from, error);
            const fs::path target = to / name;
            made.insert(target.lexically_normal());
            const u64 size = fs::file_size(source, error);
            totals.before += size;
            const auto sourceTime = fs::last_write_time(source, error);
            std::error_code missing;
            const auto targetTime = fs::last_write_time(target, missing);
            if (!again && !missing && targetTime >= sourceTime)
            {
                totals.kept++;
                const u64 now = fs::file_size(target, error);
                totals.after += now;
                tell(name, "unchanged", size, now);
                continue;
            }
            Bytes in, out;
            std::string why;
            const char* what = "failed";
            if (readAll(source, in))
            {
                what = cookOne(source, in, out, why);
            }
            else
            {
                why = "it can't be read";
            }
            if (strcmp(what, "failed") == 0)
            {
                // An old one would be taken for new.
                if (!g_options.dry)
                {
                    fs::remove(target, error);
                }
                totals.failed++;
                tell(name, what, size, 0, why);
                continue;
            }
            const Bytes& write = out.empty() ? in : out;
            if (!g_options.dry && !writeAll(target, write))
            {
                totals.failed++;
                tell(name, "failed", size, 0, "it can't be written");
                continue;
            }
            (out.empty() ? totals.copied : totals.done)++;
            totals.after += write.size();
            tell(name, what, size, write.size());
        }

        std::vector<fs::path> gone;
        for (auto it = fs::recursive_directory_iterator(to, error); !error && it != fs::recursive_directory_iterator(); it.increment(error))
        {
            if (it->is_regular_file(error) && it->path() != stamp && made.count(it->path().lexically_normal()) == 0)
            {
                gone.push_back(it->path());
            }
        }
        for (const fs::path& path : gone)
        {
            std::error_code ignored;
            const u64 size = fs::file_size(path, ignored);
            if (!g_options.dry)
            {
                fs::remove(path, ignored);
            }
            totals.removed++;
            tell(fs::relative(path, to, ignored), "removed", size, 0);
        }
        return finish(totals);
    }

    int wrong(const std::string& why)
    {
        fprintf(stderr, "%s\ncook --help says what can be asked.\n", why.c_str());
        return 2;
    }
}

int main(int argc, char** argv)
{
    Options& o = g_options;
    for (int i = 1; i < argc; i++)
    {
        const std::string a = argv[i];
        const auto is = [&](const char* small, const char* full) { return a == small || a == full; };
        if (is("-h", "--help"))
        {
            fputs(kHelp, stdout);
            return 0;
        }
        if (a == "--version")
        {
            printf("Nexgen cook %s, format %u\n", NX_VERSION, static_cast<unsigned>(kVersion));
            return 0;
        }
        if (is("-p", "--platform") || is("-s", "--skip"))
        {
            const std::string value = i + 1 < argc ? argv[++i] : "";
            if (is("-p", "--platform"))
            {
                if (value != "xboxog" && value != "desktop")
                {
                    return wrong("the platform is desktop or xboxog, not \"" + value + "\"");
                }
                o.xbox = value == "xboxog";
                continue;
            }
            bool* which = value == "images" ? &o.images : value == "sounds" ? &o.sounds : value == "models" ? &o.models : nullptr;
            if (which == nullptr)
            {
                return wrong("what can be skipped is images, sounds or models, not \"" + value + "\"");
            }
            *which = false;
        }
        else if (is("-f", "--force")) o.force = true;
        else if (is("-n", "--dry-run")) o.dry = true;
        else if (is("-q", "--quiet")) o.quiet = true;
        else if (is("-j", "--json")) o.json = true;
        else if (a.size() > 1 && a[0] == '-')
        {
            return wrong(a + " isn't an option");
        }
        else
        {
            o.paths.push_back(a);
        }
    }
    if (o.paths.empty() || o.paths.size() > 2)
    {
        return wrong("a folder and a folder to make, or a file, must be given");
    }
    setLogSink([](LogLevel, const char*) {});   // what went wrong is told with the file it was in
    std::error_code error;
    const auto path = [](const std::string& text) { return fs::path(std::u8string(text.begin(), text.end())); };
    const fs::path from = path(o.paths[0]);
    if (fs::is_directory(from, error))
    {
        return o.paths.size() == 2 ? cookFolder(from, path(o.paths[1])) : wrong("the folder to make must be given too");
    }
    if (!fs::is_regular_file(from, error))
    {
        return stop(o.paths[0] + " isn't there");
    }
    return cookFile(from, o.paths.size() == 2 ? path(o.paths[1]) : fs::path());
}
