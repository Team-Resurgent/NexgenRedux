// Texture compression, MP3 and FLAC reading, and Ogg Vorbis writing: for the cook tool only. The engine plays WAV,
// Ogg and what this makes, and none of this goes on a console.
#define STB_DXT_IMPLEMENTATION
#include "third_party/stb/stb_dxt.h"

#include "tools/cook/third_party.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#define DR_MP3_IMPLEMENTATION
#define DR_MP3_NO_STDIO
#include "third_party/dr_libs/dr_mp3.h"

#define DR_FLAC_IMPLEMENTATION
#define DR_FLAC_NO_STDIO
#include "third_party/dr_libs/dr_flac.h"

#include "third_party/libvorbis/include/vorbis/vorbisenc.h"

short* cook_mp3(const unsigned char* data, size_t size, unsigned* channels, unsigned* rate, unsigned long long* frames)
{
    drmp3_config config;
    drmp3_uint64 count = 0;
    short* samples = drmp3_open_memory_and_read_pcm_frames_s16(data, size, &config, &count, NULL);
    *channels = config.channels;
    *rate = config.sampleRate;
    *frames = count;
    return samples;
}

short* cook_flac(const unsigned char* data, size_t size, unsigned* channels, unsigned* rate, unsigned long long* frames)
{
    unsigned int ch = 0, hz = 0;
    drflac_uint64 count = 0;
    short* samples = drflac_open_memory_and_read_pcm_frames_s16(data, size, &ch, &hz, &count, NULL);
    *channels = ch;
    *rate = hz;
    *frames = count;
    return samples;
}

void cook_free(void* samples)
{
    free(samples);  /* what dr_mp3 and dr_flac made, with malloc */
}

/* Grows as pages are written. */
typedef struct
{
    unsigned char* data;
    size_t size, room;
} Out;

static int put(Out* out, const unsigned char* bytes, long count)
{
    if (out->size + (size_t)count > out->room)
    {
        size_t room = out->room ? out->room * 2 : 65536;
        while (room < out->size + (size_t)count)
        {
            room *= 2;
        }
        unsigned char* more = (unsigned char*)realloc(out->data, room);
        if (more == NULL)
        {
            return 0;
        }
        out->data = more;
        out->room = room;
    }
    memcpy(out->data + out->size, bytes, (size_t)count);
    out->size += (size_t)count;
    return 1;
}

static int putPage(Out* out, const ogg_page* page)
{
    return put(out, page->header, page->header_len) && put(out, page->body, page->body_len);
}

unsigned char* cook_ogg(const short* samples, unsigned long long frames, unsigned channels, unsigned rate, float quality, size_t* size)
{
    vorbis_info info;
    vorbis_comment comment;
    vorbis_dsp_state dsp;
    vorbis_block block;
    ogg_stream_state stream;
    ogg_page page;
    ogg_packet packet;
    Out out = { NULL, 0, 0 };
    int fine = 1, ended = 0;
    unsigned long long at = 0;
    *size = 0;

    vorbis_info_init(&info);
    if (vorbis_encode_init_vbr(&info, (long)channels, (long)rate, quality) != 0)
    {
        vorbis_info_clear(&info);
        return NULL;
    }
    vorbis_comment_init(&comment);
    vorbis_comment_add_tag(&comment, "ENCODER", "Nexgen cook");
    vorbis_analysis_init(&dsp, &info);
    vorbis_block_init(&dsp, &block);
    ogg_stream_init(&stream, (int)(time(NULL) & 0x7FFFFFFF));
    {
        ogg_packet header, commentHeader, codeHeader;
        vorbis_analysis_headerout(&dsp, &comment, &header, &commentHeader, &codeHeader);
        ogg_stream_packetin(&stream, &header);
        ogg_stream_packetin(&stream, &commentHeader);
        ogg_stream_packetin(&stream, &codeHeader);
        while (fine && ogg_stream_flush(&stream, &page))
        {
            fine = putPage(&out, &page);
        }
    }
    while (fine && !ended)
    {
        const unsigned long long left = frames - at;
        const int count = left > 1024 ? 1024 : (int)left;
        if (count == 0)
        {
            vorbis_analysis_wrote(&dsp, 0);   /* the end */
        }
        else
        {
            float** buffer = vorbis_analysis_buffer(&dsp, count);
            for (int i = 0; i < count; i++)
            {
                for (unsigned c = 0; c < channels; c++)
                {
                    buffer[c][i] = samples[(at + (unsigned long long)i) * channels + c] / 32768.0f;
                }
            }
            vorbis_analysis_wrote(&dsp, count);
            at += (unsigned long long)count;
        }
        while (fine && vorbis_analysis_blockout(&dsp, &block) == 1)
        {
            vorbis_analysis(&block, NULL);
            vorbis_bitrate_addblock(&block);
            while (fine && vorbis_bitrate_flushpacket(&dsp, &packet))
            {
                ogg_stream_packetin(&stream, &packet);
                while (fine && ogg_stream_pageout(&stream, &page))
                {
                    fine = putPage(&out, &page);
                    ended = ended || ogg_page_eos(&page);
                }
            }
        }
        if (count == 0)
        {
            while (fine && ogg_stream_flush(&stream, &page))
            {
                fine = putPage(&out, &page);
            }
            ended = 1;
        }
    }
    ogg_stream_clear(&stream);
    vorbis_block_clear(&block);
    vorbis_dsp_clear(&dsp);
    vorbis_comment_clear(&comment);
    vorbis_info_clear(&info);
    if (!fine)
    {
        free(out.data);
        return NULL;
    }
    *size = out.size;
    return out.data;
}
