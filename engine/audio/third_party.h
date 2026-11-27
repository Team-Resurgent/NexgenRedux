#pragma once

// The little of the third-party sound libraries that the engine uses. They are compiled as C in
// third_party.c, so none of their macros or warnings reach engine code.

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

short* nx_wav_load(const unsigned char* data, size_t size, unsigned int* channels, unsigned int* rate, unsigned int* frames);
void nx_wav_free(short* samples);

typedef struct NxOgg NxOgg;
NxOgg* nx_ogg_open(const unsigned char* data, int size, unsigned int* channels, unsigned int* rate, unsigned int* frames);
int nx_ogg_read(NxOgg* ogg, unsigned int channels, short* samples, unsigned int frames);
void nx_ogg_rewind(NxOgg* ogg);
void nx_ogg_close(NxOgg* ogg);

#ifdef __cplusplus
}
#endif
