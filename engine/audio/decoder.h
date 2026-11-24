#pragma once

#include "audio/audio.h"

namespace nx
{
    // Decodes a compressed sound bit by bit. Opening may allocate; decoding doesn't.
    struct SoundDecoder;

    SoundDecoder* openDecoder(const SoundData& sound);
    void closeDecoder(SoundDecoder* decoder);
    // Interleaved samples, in the sound's own channels. Returns the frames written; 0 at the end.
    u32 decode(SoundDecoder& decoder, i16* samples, u32 frames);
    bool rewindDecoder(SoundDecoder& decoder);

    // All of a sound as samples, for tools. Free with memFree.
    i16* decodeAll(const SoundData& sound, u32& frames);
}
