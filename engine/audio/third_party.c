#include "audio/third_party.h"

#include <stdlib.h>
#include <string.h>

extern void* nxAudioRealloc(void* old, size_t size);

/* --- dr_wav --- */

#define DR_WAV_IMPLEMENTATION
#define DR_WAV_NO_STDIO
#include "third_party/dr_libs/dr_wav.h"

/* --- stb_vorbis --- */

#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_NO_PUSHDATA_API
#define STB_VORBIS_MAX_CHANNELS 2
#include "third_party/stb/stb_vorbis.c"

static void* mallocFor(size_t size, void* user)
{
    (void)user;
    return nxAudioRealloc(NULL, size);
}

static void* reallocFor(void* p, size_t size, void* user)
{
    (void)user;
    return nxAudioRealloc(p, size);
}

static void freeFor(void* p, void* user)
{
    (void)user;
    if (p)
    {
        nxAudioRealloc(p, 0);
    }
}

short* nx_wav_load(const unsigned char* data, size_t size, unsigned int* channels, unsigned int* rate, unsigned int* frames)
{
    drwav_allocation_callbacks callbacks = { NULL, mallocFor, reallocFor, freeFor };
    drwav_uint64 count = 0;
    short* samples = drwav_open_memory_and_read_pcm_frames_s16(data, size, channels, rate, &count, &callbacks);
    if (samples && count > 0x7FFFFFFF)
    {
        freeFor(samples, NULL);
        return NULL;
    }
    *frames = (unsigned int)count;
    return samples;
}

void nx_wav_free(short* samples)
{
    freeFor(samples, NULL);
}

/* Given one block at the start, so nothing is allocated while it plays. */
struct NxOgg
{
    stb_vorbis* vorbis;
    char* memory;
};

NxOgg* nx_ogg_open(const unsigned char* data, int size, unsigned int* channels, unsigned int* rate, unsigned int* frames)
{
    int bytes;
    NxOgg* ogg = (NxOgg*)nxAudioRealloc(NULL, sizeof(NxOgg));
    if (ogg == NULL)
    {
        return NULL;
    }
    for (bytes = 160 * 1024; bytes <= 640 * 1024; bytes *= 2)
    {
        int error = 0;
        stb_vorbis_alloc block;
        ogg->memory = (char*)nxAudioRealloc(NULL, (size_t)bytes);
        if (ogg->memory == NULL)
        {
            break;
        }
        block.alloc_buffer = ogg->memory;
        block.alloc_buffer_length_in_bytes = bytes;
        ogg->vorbis = stb_vorbis_open_memory(data, size, &error, &block);
        if (ogg->vorbis)
        {
            stb_vorbis_info info = stb_vorbis_get_info(ogg->vorbis);
            *channels = (unsigned int)info.channels;
            *rate = info.sample_rate;
            *frames = stb_vorbis_stream_length_in_samples(ogg->vorbis);
            return ogg;
        }
        nxAudioRealloc(ogg->memory, 0);
        if (error != VORBIS_outofmem)
        {
            break;
        }
    }
    nxAudioRealloc(ogg, 0);
    return NULL;
}

int nx_ogg_read(NxOgg* ogg, unsigned int channels, short* samples, unsigned int frames)
{
    return stb_vorbis_get_samples_short_interleaved(ogg->vorbis, (int)channels, samples, (int)(frames * channels));
}

void nx_ogg_rewind(NxOgg* ogg)
{
    stb_vorbis_seek_start(ogg->vorbis);
}

void nx_ogg_close(NxOgg* ogg)
{
    if (ogg)
    {
        stb_vorbis_close(ogg->vorbis);
        nxAudioRealloc(ogg->memory, 0);
        nxAudioRealloc(ogg, 0);
    }
}
