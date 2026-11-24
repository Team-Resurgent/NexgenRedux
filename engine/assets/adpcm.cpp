#include "assets/formats.h"

namespace nx
{
    namespace
    {
        const int kIndexStep[16] = { -1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8 };
        const int kSteps[89] = {
            7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
            130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
            2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
        };

        int clampSample(int v)
        {
            return v < -32768 ? -32768 : v > 32767 ? 32767 : v;
        }

        int clampIndex(int v)
        {
            return v < 0 ? 0 : v > 88 ? 88 : v;
        }

        int apply(int predicted, int& index, int code)
        {
            const int step = kSteps[index];
            int difference = step >> 3;
            difference += code & 1 ? step >> 2 : 0;
            difference += code & 2 ? step >> 1 : 0;
            difference += code & 4 ? step : 0;
            index = clampIndex(index + kIndexStep[code]);
            return clampSample(code & 8 ? predicted - difference : predicted + difference);
        }
    }

    void adpcmEncodeBlock(const i16* samples, u32 frames, u32 channels, int* stepIndex, u8* out)
    {
        for (u32 c = 0; c < channels; c++)
        {
            u8* p = out + static_cast<size_t>(c) * kAdpcmChannelBytes;
            int predicted = frames ? samples[c] : 0;
            int index = clampIndex(stepIndex[c]);
            writeU16(p, static_cast<u16>(predicted));
            p[2] = static_cast<u8>(index);
            p[3] = 0;
            for (u32 i = 1; i < kAdpcmFrames; i++)
            {
                const int sample = i < frames ? samples[static_cast<size_t>(i) * channels + c] : 0;
                int difference = sample - predicted;
                int code = difference < 0 ? 8 : 0;
                difference = difference < 0 ? -difference : difference;
                int step = kSteps[index];
                for (int bit = 4; bit; bit >>= 1)
                {
                    if (difference >= step)
                    {
                        code |= bit;
                        difference -= step;
                    }
                    step >>= 1;
                }
                predicted = apply(predicted, index, code);
                u8& byte = p[4 + (i - 1) / 2];
                byte = (i - 1) % 2 ? static_cast<u8>(byte | (code << 4)) : static_cast<u8>(code);
            }
            stepIndex[c] = index;
        }
    }

    void adpcmDecodeBlock(const u8* block, u32 channels, i16* samples)
    {
        for (u32 c = 0; c < channels; c++)
        {
            const u8* p = block + static_cast<size_t>(c) * kAdpcmChannelBytes;
            int predicted = static_cast<i16>(readU16(p));
            int index = clampIndex(p[2]);
            samples[c] = static_cast<i16>(predicted);
            for (u32 i = 1; i < kAdpcmFrames; i++)
            {
                const u8 byte = p[4 + (i - 1) / 2];
                predicted = apply(predicted, index, (i - 1) % 2 ? byte >> 4 : byte & 15);
                samples[static_cast<size_t>(i) * channels + c] = static_cast<i16>(predicted);
            }
        }
    }
}
