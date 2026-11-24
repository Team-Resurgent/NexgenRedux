/* Sound output on desktop. Only miniaudio's device is used: mixing and decoding are the engine's. */
#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define MA_NO_ENGINE
#define MA_NO_NODE_GRAPH
#define MA_NO_RESOURCE_MANAGER
#define MINIAUDIO_IMPLEMENTATION
#include "third_party/miniaudio/miniaudio.h"

typedef void (*NxMix)(short* stereo, unsigned int frames, void* user);

typedef struct NxDevice
{
    ma_device device;
    NxMix mix;
    void* user;
} NxDevice;

static NxDevice g_device; /* there is one, and its address must not change */

static void fill(ma_device* device, void* output, const void* input, ma_uint32 frames)
{
    NxDevice* self = (NxDevice*)device->pUserData;
    (void)input;
    self->mix((short*)output, frames, self->user);
}

int nx_device_start(NxMix mix, void* user, unsigned int rate)
{
    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_s16;
    config.playback.channels = 2;
    config.sampleRate = rate;
    config.dataCallback = fill;
    config.pUserData = &g_device;
    g_device.mix = mix;
    g_device.user = user;
    if (ma_device_init(NULL, &config, &g_device.device) != MA_SUCCESS)
    {
        return 0;
    }
    if (ma_device_start(&g_device.device) != MA_SUCCESS)
    {
        ma_device_uninit(&g_device.device);
        return 0;
    }
    return 1;
}

void nx_device_stop(void)
{
    ma_device_uninit(&g_device.device);
}

const char* nx_device_name(void)
{
    return ma_get_backend_name(g_device.device.pContext->backend);
}
