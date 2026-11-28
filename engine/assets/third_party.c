#include "assets/third_party.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern void* nxDecodeRealloc(void* old, size_t oldSize, size_t size);
extern void nxDecodeFree(void* memory);

/* --- stb_image --- */

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#define STBI_NO_PSD
#define STBI_NO_PIC
#define STBI_NO_PNM
#define STBI_NO_THREAD_LOCALS
#define STBI_MAX_DIMENSIONS 4096
#define STBI_ASSERT(x) ((void)0)
#define STBI_MALLOC(size) nxDecodeRealloc(NULL, 0, size)
#define STBI_REALLOC_SIZED(p, old, size) nxDecodeRealloc(p, old, size)
#define STBI_FREE(p) nxDecodeFree(p)
#include "third_party/stb/stb_image.h"

int nx_stbi_info(const unsigned char* data, int size, int* width, int* height, int* channels)
{
    return stbi_info_from_memory(data, size, width, height, channels);
}

unsigned char* nx_stbi_load(const unsigned char* data, int size, int* width, int* height, int* channels, int wanted)
{
    return stbi_load_from_memory(data, size, width, height, channels, wanted);
}

/* --- stb_image_write: PNGs, and the deflate it makes them with --- */

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_WRITE_NO_STDIO
#define STBIW_ASSERT(x) ((void)0)
#define STBIW_MALLOC(size) nxDecodeRealloc(NULL, 0, size)
#define STBIW_REALLOC_SIZED(p, old, size) nxDecodeRealloc(p, old, size)
#define STBIW_FREE(p) nxDecodeFree(p)
#include "third_party/stb/stb_image_write.h"

unsigned char* nx_png_write(const unsigned char* rgba, int width, int height, int* size)
{
    return stbi_write_png_to_mem(rgba, width * 4, width, height, 4, size);
}

unsigned char* nx_zlib_compress(const unsigned char* data, int size, int* outSize, int level)
{
    return stbi_zlib_compress((unsigned char*)data, size, outSize, level);
}

unsigned char* nx_zlib_uncompress(const unsigned char* data, int size, int* outSize)
{
    return (unsigned char*)stbi_zlib_decode_malloc((const char*)data, size, outSize);
}

unsigned char* nx_inflate_raw(const unsigned char* data, int size, int* outSize)
{
    return (unsigned char*)stbi_zlib_decode_noheader_malloc((const char*)data, size, outSize);
}

/* --- SSFN --- */

/* SSFN resizes blocks without saying how big they were, so each remembers its own size. */
static void* fontRealloc(void* old, size_t size)
{
    size_t* block = old ? (size_t*)((char*)old - 16) : NULL;
    size_t* fresh = (size_t*)nxDecodeRealloc(block, block ? *block + 16 : 0, size + 16);
    if (fresh == NULL)
    {
        return NULL;
    }
    *fresh = size;
    return (char*)fresh + 16;
}

static void fontFree(void* memory)
{
    if (memory)
    {
        nxDecodeFree((char*)memory - 16);
    }
}

#define SSFN_IMPLEMENTATION
#define SSFN_memcmp memcmp
#define SSFN_memset memset
#define SSFN_realloc fontRealloc
#define SSFN_free fontFree
/* Tells it that the standard headers are already here, so it makes no types of its own. */
#ifndef _STDINT_H
#define _STDINT_H
#endif
#ifndef _STRING_H_
#define _STRING_H_
#endif
#define size_t size_t
#include "third_party/ssfn/ssfn.h"

struct NxFont
{
    ssfn_t context;
    unsigned char* data;
};

NxFont* nx_font_open(const unsigned char* data, size_t size, int pixels)
{
    NxFont* font = (NxFont*)fontRealloc(NULL, sizeof(NxFont));
    if (font == NULL)
    {
        return NULL;
    }
    memset(font, 0, sizeof(*font));
    if (size < sizeof(ssfn_font_t) || ssfn_load(&font->context, data) != SSFN_OK ||
        ssfn_select(&font->context, SSFN_FAMILY_ANY, NULL, SSFN_STYLE_REGULAR | SSFN_STYLE_NOKERN | SSFN_STYLE_NOCACHE | SSFN_STYLE_NODEFGLYPH, pixels) != SSFN_OK)
    {
        nx_font_close(font);
        return NULL;
    }
    return font;
}

int nx_font_glyph(NxFont* font, const char* text, unsigned char* image, int side, int x, int y, int* advance)
{
    ssfn_buf_t buffer;
    memset(&buffer, 0, sizeof(buffer));
    buffer.ptr = image;
    buffer.w = side;
    buffer.h = side;
    buffer.p = (uint16_t)(side * 4);
    buffer.x = x;
    buffer.y = y;
    buffer.fg = 0xFFFFFFFF;
    if (ssfn_render(&font->context, &buffer, text) <= 0)
    {
        return 0;
    }
    *advance = buffer.x - x;
    return 1;
}

void nx_font_close(NxFont* font)
{
    ssfn_free(&font->context);
    fontFree(font);
}

#define CGLTF_IMPLEMENTATION
#include "third_party/cgltf/cgltf.h"
