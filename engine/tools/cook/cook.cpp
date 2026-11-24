// Prepares a media folder for a platform, on the machine that builds the game:
//
//   cook --platform xboxog|desktop <media folder> <folder to make>
//
// Images become compressed textures and sounds become ADPCM, under the same names, so scripts don't
// change. Whatever wouldn't get smaller is copied. Only files that have changed are done again, and
// files whose originals are gone are removed.
#include "assets/decode.h"
#include "assets/formats.h"
#include "audio/decoder.h"
#include "base/log.h"
#include "base/memory.h"

#include "third_party/stb/stb_dxt.h"

#include <filesystem>
#include <fstream>
#include <set>
#include <string.h>
#include <string>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;
using namespace nx;

namespace
{
    // Goes up when the output changes, so everything is done again.
    constexpr u32 kVersion = 1;
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

    std::string lower(std::string text)
    {
        for (char& c : text)
        {
            c = c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
        }
        return text;
    }

    int cook(const fs::path& from, const fs::path& to, bool powersOfTwo)
    {
        std::error_code error;
        const fs::path inside = fs::weakly_canonical(to, error).lexically_relative(fs::weakly_canonical(from, error));
        if (!fs::is_directory(from, error) || (!inside.empty() && *inside.begin() != ".."))
        {
            NX_ERROR("%s must be a folder, and the one to make must be outside it", from.string().c_str());
            return 1;
        }
        fs::create_directories(to, error);

        // Never clear out a folder that something else made.
        const fs::path stamp = to / ".cooked";
        const std::string stampText = "nexgen cook " + std::to_string(kVersion) + (powersOfTwo ? " xboxog\n" : " desktop\n");
        bool again = true;
        Bytes old;
        if (readAll(stamp, old))
        {
            again = std::string(old.begin(), old.end()) != stampText;
        }
        else if (!fs::is_empty(to, error) || error)
        {
            NX_ERROR("%s was not made by this tool: choose a new folder", to.string().c_str());
            return 1;
        }
        std::ofstream(stamp, std::ios::binary) << stampText;

        Totals totals;
        std::set<fs::path> made;
        for (auto it = fs::recursive_directory_iterator(from, error); !error && it != fs::recursive_directory_iterator(); it.increment(error))
        {
            if (!it->is_regular_file(error))
            {
                continue;
            }
            const fs::path source = it->path();
            const fs::path name = fs::relative(source, from, error);
            const fs::path target = to / name;
            made.insert(target.lexically_normal());
            totals.before += fs::file_size(source, error);
            const auto sourceTime = fs::last_write_time(source, error);
            std::error_code missing;
            const auto targetTime = fs::last_write_time(target, missing);
            if (!again && !missing && targetTime >= sourceTime)
            {
                totals.kept++;
                totals.after += fs::file_size(target, error);
                continue;
            }
            Bytes in, out;
            const std::string extension = lower(name.extension().string());
            bool ok = readAll(source, in);
            if (ok && isImage(extension))
            {
                ok = cookImage(in, powersOfTwo, out);
            }
            else if (ok && isSound(extension))
            {
                ok = cookSound(in, extension != ".wav", out);
            }
            if (!ok)
            {
                NX_ERROR("cannot read %s", source.string().c_str());
                totals.failed++;
            }
            const Bytes& write = out.empty() ? in : out;
            (out.empty() ? totals.copied : totals.done)++;
            fs::create_directories(target.parent_path(), error);
            std::ofstream file(target, std::ios::binary | std::ios::trunc);
            file.write(reinterpret_cast<const char*>(write.data()), static_cast<std::streamsize>(write.size()));
            if (!file)
            {
                NX_ERROR("cannot write %s", target.string().c_str());
                totals.failed++;
            }
            totals.after += write.size();
        }
        if (error)
        {
            NX_ERROR("%s: %s", from.string().c_str(), error.message().c_str());
            return 1;
        }

        std::vector<fs::path> gone;
        for (const auto& entry : fs::recursive_directory_iterator(to, error))
        {
            if (entry.is_regular_file(error) && entry.path() != stamp && made.count(entry.path().lexically_normal()) == 0)
            {
                gone.push_back(entry.path());
            }
        }
        for (const fs::path& path : gone)
        {
            fs::remove(path, error);
            totals.removed++;
        }

        NX_INFO("%u cooked, %u copied, %u unchanged, %u removed, %u failed; %u KB became %u KB", static_cast<unsigned>(totals.done), static_cast<unsigned>(totals.copied),
            static_cast<unsigned>(totals.kept), static_cast<unsigned>(totals.removed), static_cast<unsigned>(totals.failed),
            static_cast<unsigned>(totals.before / 1024), static_cast<unsigned>(totals.after / 1024));
        return totals.failed ? 1 : 0;
    }
}

int main(int argc, char** argv)
{
    if (argc != 5 || strcmp(argv[1], "--platform") != 0 || (strcmp(argv[2], "xboxog") != 0 && strcmp(argv[2], "desktop") != 0))
    {
        NX_ERROR("usage: cook --platform xboxog|desktop <media folder> <folder to make>");
        return 2;
    }
    return cook(argv[3], argv[4], strcmp(argv[2], "xboxog") == 0);
}
