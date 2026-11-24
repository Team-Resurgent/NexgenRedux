#pragma once

#include "base/endian.h"
#include "base/types.h"
#include "gpu/gpu.h"

// What the cook tool writes and the engine reads. Little endian, and found by their first bytes, so a
// cooked file keeps the name of the one it was made from.
namespace nx
{
    // "NXT1", width, height, stored width, stored height (u16 each), format (u8), three zeros, pixels.
    constexpr u32 kTextureMagic = 0x3154584E;
    constexpr size_t kTextureHeader = 16;

    inline bool isCookedTexture(const u8* data, size_t size)
    {
        return size >= kTextureHeader && readU32(data) == kTextureMagic;
    }

    // False if anything doesn't add up, so a damaged file can't make anyone read past its end.
    inline bool readCookedTexture(const u8* data, size_t size, gpu::TextureData& out)
    {
        if (!isCookedTexture(data, size) || data[12] > static_cast<u8>(gpu::PixelFormat::Bc3))
        {
            return false;
        }
        out.width = readU16(data + 4);
        out.height = readU16(data + 6);
        out.fullWidth = readU16(data + 8);
        out.fullHeight = readU16(data + 10);
        out.format = static_cast<gpu::PixelFormat>(data[12]);
        out.pixels = data + kTextureHeader;
        return gpu::validTexture(out) && size - kTextureHeader >= gpu::pixelBytes(out.format, out.fullWidth, out.fullHeight);
    }

    // "NXA1", rate (u32), frames (u32), channels (u8), three zeros, blocks. IMA ADPCM: each block has a
    // 4-byte start (sample, step index) per channel, then 4 bits a sample, channels one after the other.
    constexpr u32 kAdpcmMagic = 0x3141584E;
    constexpr size_t kAdpcmHeader = 16;
    constexpr u32 kAdpcmFrames = 505;                                          // in a block
    constexpr u32 kAdpcmChannelBytes = 4 + (kAdpcmFrames - 1) / 2;             // in a block, per channel

    inline size_t adpcmBlockBytes(u32 channels)
    {
        return static_cast<size_t>(kAdpcmChannelBytes) * channels;
    }

    // frames is how many of the block's 505 are real; the rest of the input is taken as silence.
    void adpcmEncodeBlock(const i16* samples, u32 frames, u32 channels, int* stepIndex, u8* out);
    // Writes interleaved samples for a whole block.
    void adpcmDecodeBlock(const u8* block, u32 channels, i16* samples);
}
