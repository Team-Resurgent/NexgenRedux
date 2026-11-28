#pragma once

// The little of the third-party decoders that the engine uses. They are compiled as C in
// third_party.c, so none of their macros or warnings reach engine code.

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int nx_stbi_info(const unsigned char* data, int size, int* width, int* height, int* channels);
unsigned char* nx_stbi_load(const unsigned char* data, int size, int* width, int* height, int* channels, int wanted);
unsigned char* nx_png_write(const unsigned char* rgba, int width, int height, int* size);
unsigned char* nx_zlib_compress(const unsigned char* data, int size, int* outSize, int level);
unsigned char* nx_zlib_uncompress(const unsigned char* data, int size, int* outSize);
unsigned char* nx_inflate_raw(const unsigned char* data, int size, int* outSize);

typedef struct NxFont NxFont;

NxFont* nx_font_open(const unsigned char* data, size_t size, int pixels);
/* Draws one UTF-8 character with its pen at (x, y) into a square RGBA image. 0 if the font lacks it. */
int nx_font_glyph(NxFont* font, const char* text, unsigned char* image, int side, int x, int y, int* advance);
void nx_font_close(NxFont* font);

#ifdef __cplusplus
}
#endif
