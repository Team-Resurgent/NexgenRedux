#include "audio/decoder.h"
#include "assets/formats.h"
#include "audio/third_party.h"
#include "base/log.h"
#include "base/memory.h"

#include <string.h>

extern "C" void* nxAudioRealloc(void* old, size_t size)
{
    // Each block remembers its size, because the libraries don't say.
    size_t* block = old ? reinterpret_cast<size_t*>(static_cast<char*>(old) - 16) : nullptr;
    if (size == 0)
    {
        nx::memFree(block);
        return nullptr;
    }
    size_t* fresh = static_cast<size_t*>(nx::memAlloc(size + 16, nx::MemTag::Audio));
    if (fresh == nullptr)
    {
        return nullptr;
    }
    *fresh = size;
    if (block)
    {
        memcpy(reinterpret_cast<char*>(fresh) + 16, old, *block < size ? *block : size);
        nx::memFree(block);
    }
    return reinterpret_cast<char*>(fresh) + 16;
}

namespace nx
{
    namespace
    {
        constexpr u32 kMaxRate = 192000;

        bool sane(u32 channels, u32 rate)
        {
            return (channels == 1 || channels == 2) && rate >= 4000 && rate <= kMaxRate;
        }

        SoundData* keep(SoundKind kind, u32 channels, u32 rate, u32 frames, const u8* data, size_t size)
        {
            SoundData* sound = create<SoundData>(MemTag::Audio);
            u8* copy = sound ? static_cast<u8*>(memAlloc(size, MemTag::Audio)) : nullptr;
            if (copy == nullptr)
            {
                destroy(sound);
                return nullptr;
            }
            memcpy(copy, data, size);
            *sound = { kind, channels, rate, frames, copy, size };
            return sound;
        }
    }

    struct SoundDecoder
    {
        const SoundData* sound = nullptr;
        NxOgg* ogg = nullptr;
        NxMp3* mp3 = nullptr;
        // ADPCM
        u32 frame = 0;
        u32 block = 0xFFFFFFFF;
        i16 samples[kAdpcmFrames * 2];
    };

    SoundData* loadSound(const u8* file, size_t size)
    {
        if (size < 16 || size > 0x7FFFFFFF)
        {
            return nullptr;
        }
        if (readU32(file) == kAdpcmMagic)
        {
            const u32 rate = readU32(file + 4);
            const u32 frames = readU32(file + 8);
            const u32 channels = file[12];
            if (!sane(channels, rate) || frames == 0)
            {
                return nullptr;
            }
            const u64 blocks = (static_cast<u64>(frames) + kAdpcmFrames - 1) / kAdpcmFrames;
            if (blocks * adpcmBlockBytes(channels) > size - kAdpcmHeader)
            {
                return nullptr;
            }
            return keep(SoundKind::Adpcm, channels, rate, frames, file + kAdpcmHeader, static_cast<size_t>(blocks) * adpcmBlockBytes(channels));
        }
        if (memcmp(file, "OggS", 4) == 0)
        {
            unsigned int channels = 0, rate = 0, frames = 0;
            NxOgg* ogg = nx_ogg_open(file, static_cast<int>(size), &channels, &rate, &frames);
            if (ogg == nullptr)
            {
                return nullptr;
            }
            nx_ogg_close(ogg);
            return sane(channels, rate) ? keep(SoundKind::Ogg, channels, rate, frames, file, size) : nullptr;
        }
        if (memcmp(file, "RIFF", 4) == 0)
        {
            unsigned int channels = 0, rate = 0, frames = 0;
            short* samples = nx_wav_load(file, size, &channels, &rate, &frames);
            SoundData* sound = samples && sane(channels, rate) && frames ? keep(SoundKind::Pcm, channels, rate, frames, reinterpret_cast<const u8*>(samples), static_cast<size_t>(frames) * channels * 2) : nullptr;
            nx_wav_free(samples);
            return sound;
        }
        unsigned int channels = 0, rate = 0;
        NxMp3* mp3 = nx_mp3_open(file, size, &channels, &rate);
        if (mp3 == nullptr)
        {
            return nullptr;
        }
        nx_mp3_close(mp3);
        return sane(channels, rate) ? keep(SoundKind::Mp3, channels, rate, 0, file, size) : nullptr;
    }

    void freeSound(SoundData* sound)
    {
        if (sound)
        {
            memFree(sound->data);
            destroy(sound);
        }
    }

    SoundDecoder* openDecoder(const SoundData& sound)
    {
        SoundDecoder* d = create<SoundDecoder>(MemTag::Audio);
        if (d == nullptr)
        {
            return nullptr;
        }
        d->sound = &sound;
        unsigned int channels, rate, frames;
        if (sound.kind == SoundKind::Ogg)
        {
            d->ogg = nx_ogg_open(sound.data, static_cast<int>(sound.bytes), &channels, &rate, &frames);
        }
        else if (sound.kind == SoundKind::Mp3)
        {
            d->mp3 = nx_mp3_open(sound.data, sound.bytes, &channels, &rate);
        }
        if ((sound.kind == SoundKind::Ogg && d->ogg == nullptr) || (sound.kind == SoundKind::Mp3 && d->mp3 == nullptr))
        {
            destroy(d);
            return nullptr;
        }
        return d;
    }

    void closeDecoder(SoundDecoder* d)
    {
        if (d)
        {
            nx_ogg_close(d->ogg);
            nx_mp3_close(d->mp3);
            destroy(d);
        }
    }

    u32 decode(SoundDecoder& d, i16* samples, u32 frames)
    {
        const SoundData& sound = *d.sound;
        switch (sound.kind)
        {
        case SoundKind::Ogg:
        {
            u32 got = 0;
            while (got < frames)
            {
                const int n = nx_ogg_read(d.ogg, sound.channels, samples + static_cast<size_t>(got) * sound.channels, frames - got);
                if (n <= 0)
                {
                    break;
                }
                got += static_cast<u32>(n);
            }
            return got;
        }
        case SoundKind::Mp3:
            return nx_mp3_read(d.mp3, samples, frames);
        case SoundKind::Adpcm:
        {
            u32 got = 0;
            while (got < frames && d.frame < sound.frames)
            {
                const u32 block = d.frame / kAdpcmFrames;
                const u32 inside = d.frame % kAdpcmFrames;
                if (block != d.block)
                {
                    adpcmDecodeBlock(sound.data + block * adpcmBlockBytes(sound.channels), sound.channels, d.samples);
                    d.block = block;
                }
                u32 count = kAdpcmFrames - inside;
                count = count < frames - got ? count : frames - got;
                count = count < sound.frames - d.frame ? count : sound.frames - d.frame;
                memcpy(samples + static_cast<size_t>(got) * sound.channels, d.samples + static_cast<size_t>(inside) * sound.channels, static_cast<size_t>(count) * sound.channels * sizeof(i16));
                got += count;
                d.frame += count;
            }
            return got;
        }
        case SoundKind::Pcm:
        {
            const u32 left = sound.frames - d.frame;
            const u32 count = left < frames ? left : frames;
            memcpy(samples, reinterpret_cast<const i16*>(sound.data) + static_cast<size_t>(d.frame) * sound.channels, static_cast<size_t>(count) * sound.channels * sizeof(i16));
            d.frame += count;
            return count;
        }
        }
        return 0;
    }

    bool rewindDecoder(SoundDecoder& d)
    {
        d.frame = 0;
        d.block = 0xFFFFFFFF;
        if (d.ogg)
        {
            nx_ogg_rewind(d.ogg);
        }
        return d.mp3 ? nx_mp3_rewind(d.mp3) != 0 : true;
    }

    i16* decodeAll(const SoundData& sound, u32& frames)
    {
        SoundDecoder* d = openDecoder(sound);
        frames = 0;
        if (d == nullptr)
        {
            return nullptr;
        }
        size_t room = sound.rate;
        i16* samples = static_cast<i16*>(memAlloc(room * sound.channels * sizeof(i16), MemTag::Audio));
        while (samples)
        {
            if (frames + 4096 > room)
            {
                i16* bigger = static_cast<i16*>(memAlloc(room * 2 * sound.channels * sizeof(i16), MemTag::Audio));
                if (bigger)
                {
                    memcpy(bigger, samples, static_cast<size_t>(frames) * sound.channels * sizeof(i16));
                }
                memFree(samples);
                samples = bigger;
                room *= 2;
                continue;
            }
            const u32 got = decode(*d, samples + static_cast<size_t>(frames) * sound.channels, 4096);
            if (got == 0)
            {
                break;
            }
            frames += got;
        }
        closeDecoder(d);
        return samples;
    }
}
