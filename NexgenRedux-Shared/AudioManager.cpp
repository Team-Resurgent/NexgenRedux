#include "AudioManager.h"
#include "ConfigLoader.h"

#include <Gensys/Int.h>
#include <Gensys/FileSystem.h>
#include <Gensys/StringUtility.h>

// #define STB_VORBIS_HEADER_ONLY
// #include <STB/stb_vorbis.c>
#define MINIAUDIO_IMPLEMENTATION
#include <MinAudio/miniaudio.h>
// #undef STB_VORBIS_HEADER_ONLY
// #include <STB/stb_vorbis.c>

#include <string>
#include <vector>
#include <map>

using namespace Gensys;
using namespace NexgenRedux;

namespace
{
    #define SAMPLE_FORMAT   ma_format_f32
    #define CHANNEL_COUNT   2
    #define SAMPLE_RATE     48000

    typedef struct AudioContainer
    {
        bool loop;
        bool finished;
        ma_decoder* decoder;
    } AudioContainer;

    uint32_t m_maxAudioContainerID;
    std::map<uint32_t, AudioContainer> m_audioContainerMap;

    bool m_closeRequested;
    ma_event m_closeEvent; 

    ma_device device;

    ma_uint32 read_and_mix_pcm_frames_f32(ma_decoder* pDecoder, float* pOutputF32, ma_uint32 frameCount)
    {
        /*
        The way mixing works is that we just read into a temporary buffer, then take the contents of that buffer and mix it with the
        contents of the output buffer by simply adding the samples together. You could also clip the samples to -1..+1, but I'm not
        doing that in this example.
        */
        ma_result result;
        float temp[4096];
        ma_uint32 tempCapInFrames = ma_countof(temp) / CHANNEL_COUNT;
        ma_uint32 totalFramesRead = 0;

        while (totalFramesRead < frameCount) {
            ma_uint64 iSample;
            ma_uint64 framesReadThisIteration;
            ma_uint32 totalFramesRemaining = frameCount - totalFramesRead;
            ma_uint32 framesToReadThisIteration = tempCapInFrames;
            if (framesToReadThisIteration > totalFramesRemaining) {
                framesToReadThisIteration = totalFramesRemaining;
            }

            result = ma_decoder_read_pcm_frames(pDecoder, temp, framesToReadThisIteration, &framesReadThisIteration);
            if (result != MA_SUCCESS || framesReadThisIteration == 0) {
                break;
            }

            /* Mix the frames together. */
            for (iSample = 0; iSample < framesReadThisIteration*CHANNEL_COUNT; ++iSample) {
                pOutputF32[totalFramesRead*CHANNEL_COUNT + iSample] += temp[iSample];
            }

            totalFramesRead += (ma_uint32)framesReadThisIteration;

            if (framesReadThisIteration < (ma_uint32)framesToReadThisIteration) {
                break;  /* Reached EOF. */
            }
        }
        
        return totalFramesRead;
    }

    void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
    {
        float* pOutputF32 = (float*)pOutput;

        if (m_closeRequested == false)
        {
            for (std::map<uint32_t, AudioContainer>::iterator it = m_audioContainerMap.begin(); it != m_audioContainerMap.end(); ++it)
            {
                if (it->second.finished == false)
                {
                    ma_uint32 framesRead = read_and_mix_pcm_frames_f32(it->second.decoder, pOutputF32, frameCount);
                    if (framesRead < frameCount) 
                    {    
                        it->second.finished = true;
                    }
                }
                else if (it->second.loop == true)
                {
                    ma_data_source_read_pcm_frames(it->second.decoder, pOutputF32, frameCount, NULL);
                    it->second.finished = false;
                }
            }
        }
        else
        {
            for (std::map<uint32_t, AudioContainer>::iterator it = m_audioContainerMap.begin(); it != m_audioContainerMap.end(); ++it)
            {
                ma_decoder_uninit(it->second.decoder);
                free(it->second.decoder);
                it->second.decoder = NULL;
            }
            ma_event_signal(&m_closeEvent);
        }

        (void)pInput;
    }
}

void AudioManager::Init()
{
    m_closeRequested = false;
    m_maxAudioContainerID = 0;

    ma_event_init(&m_closeEvent);

    ma_device_config deviceConfig;
    deviceConfig = ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.format   = SAMPLE_FORMAT;
    deviceConfig.playback.channels = CHANNEL_COUNT;
    deviceConfig.sampleRate        = SAMPLE_RATE;
    deviceConfig.dataCallback      = data_callback;
    deviceConfig.pUserData         = NULL;

    if (ma_device_init(NULL, &deviceConfig, &device) != MA_SUCCESS) 
    {
        printf("Failed to open playback device.\n");
        return ;
    }

    if (ma_device_start(&device) != MA_SUCCESS) {
        printf("Failed to start playback device.\n");
        return;
    }
}

void AudioManager::Close()
{
    m_closeRequested = true;
    ma_event_wait(&m_closeEvent);
    ma_device_uninit(&device);
}

void AudioManager::Update()
{
    for (std::map<uint32_t, AudioContainer>::iterator it = m_audioContainerMap.begin(); it != m_audioContainerMap.end();)
    {
        if (it->second.loop == false && it->second.finished == true)
        {
            std::map<uint32_t, AudioContainer>::iterator itErase = it;
            ++it;

            ma_decoder_uninit(itErase->second.decoder);
            free(itErase->second.decoder);
            m_audioContainerMap.erase(itErase);
            continue;
        }
        ++it;
    }
}

uint32_t AudioManager::PlayAudio(const std::string path, bool loop)
{
	std::wstring mappedPath;
	if (path.length() == 0 || ConfigLoader::MapPath(StringUtility::ToWideString(path), mappedPath) == false)
	{
		return 0;
	}

    ma_decoder_config decoderConfig = ma_decoder_config_init(SAMPLE_FORMAT, CHANNEL_COUNT, SAMPLE_RATE);

    ma_decoder* decoder = (ma_decoder*)malloc(sizeof(ma_decoder));
    ma_result result = ma_decoder_init_file(StringUtility::ToString(mappedPath).c_str(), &decoderConfig, decoder);
    if (result != MA_SUCCESS) 
    {
        free(decoder);
        return 0;
    }

    ma_data_source_set_looping(decoder, loop ? MA_TRUE : MA_FALSE);

    uint32_t audioContainerID = ++m_maxAudioContainerID;
    AudioContainer audioContainer;
    audioContainer.loop = loop;
    audioContainer.finished = false;
    audioContainer.decoder = decoder;
    m_audioContainerMap.insert(std::pair<uint32_t, AudioContainer>(audioContainerID, audioContainer));
	 return audioContainerID;
}
