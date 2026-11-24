/* Writes what the computer is playing to its output as 16-bit stereo, until its input closes.
   For recording videos on Windows; see record.py. */
#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define MA_NO_ENGINE
#define MA_NO_NODE_GRAPH
#define MA_NO_RESOURCE_MANAGER
#define MINIAUDIO_IMPLEMENTATION
#include "third_party/miniaudio/miniaudio.h"

#include <fcntl.h>
#include <io.h>
#include <stdio.h>

static void heard(ma_device* device, void* output, const void* input, ma_uint32 frames)
{
    (void)device;
    (void)output;
    if (input)
    {
        fwrite(input, 4, frames, stdout);
        fflush(stdout);
    }
}

int main(void)
{
    ma_backend backend = ma_backend_wasapi;
    ma_device_config config = ma_device_config_init(ma_device_type_loopback);
    ma_device device;
    _setmode(_fileno(stdout), _O_BINARY);
    config.capture.format = ma_format_s16;
    config.capture.channels = 2;
    config.sampleRate = 48000;
    config.dataCallback = heard;
    if (ma_device_init_ex(&backend, 1, NULL, &config, &device) != MA_SUCCESS || ma_device_start(&device) != MA_SUCCESS)
    {
        fprintf(stderr, "cannot listen to the sound output\n");
        return 1;
    }
    while (getchar() != EOF)
    {
    }
    ma_device_uninit(&device);
    return 0;
}
