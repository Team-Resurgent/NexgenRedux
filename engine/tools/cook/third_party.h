#pragma once

// What the cook tool uses of the libraries that only it has, compiled as C in third_party.c.

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Every frame, 16-bit and interleaved; nullptr if it can't be read. Free with cook_free.
short* cook_mp3(const unsigned char* data, size_t size, unsigned* channels, unsigned* rate, unsigned long long* frames);
short* cook_flac(const unsigned char* data, size_t size, unsigned* channels, unsigned* rate, unsigned long long* frames);
void cook_free(void* samples);

// An Ogg Vorbis file, quality 0 to 1; nullptr if it can't be made. Free with cook_free.
unsigned char* cook_ogg(const short* samples, unsigned long long frames, unsigned channels, unsigned rate, float quality, size_t* size);

#ifdef __cplusplus
}
#endif
