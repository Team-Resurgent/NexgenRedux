#include "AudioManager.h"
#include "ConfigLoader.h"

#include <Gensys/Int.h>
#include <Gensys/FileSystem.h>
#include <Gensys/StringUtility.h>

#if defined NEXGEN_WIN || defined NEXGEN_LINUX || defined NEXGEN_MAC

#define STB_VORBIS_HEADER_ONLY
#include <STB/stb_vorbis.c>
#define MINIAUDIO_IMPLEMENTATION
#include <MinAudio/miniaudio.h>

#include <string>
#include <vector>
#include <map>

using namespace Gensys;
using namespace NexgenRedux;

namespace
{
    AudioManager* m_instance = NULL;

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
    ma_device m_device;

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

AudioManager* AudioManager::GetInstance()
{
	if (m_instance == NULL) 
	{
    	m_instance = new AudioManager();
    }
    return m_instance;
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

    if (ma_device_init(NULL, &deviceConfig, &m_device) != MA_SUCCESS) 
    {
        printf("Failed to open playback device.\n");
        return ;
    }

    if (ma_device_start(&m_device) != MA_SUCCESS) {
        printf("Failed to start playback device.\n");
        return;
    }
}

void AudioManager::Close()
{
    m_closeRequested = true;
    ma_event_wait(&m_closeEvent);
    ma_device_uninit(&m_device);
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

#elif defined NEXGEN_OG

#include <Gensys/Int.h>
#include <Gensys/FileSystem.h>
#include <Gensys/StringUtility.h>

#define DR_WAV_IMPLEMENTATION
#include <MinAudio/dr_wav.h>

#include <dmusici.h>
#include <dsound.h>
#include <map>

using namespace Gensys;
using namespace NexgenRedux;

namespace
{
	AudioManager* m_instance = NULL;

    #define AUDIO_PACKETS 64
    #define AUDIO_FRAME_SAMPLES 2048

    LPDIRECTSOUND8 m_directSoundDevice;
	
	typedef struct AudioContainer {	
		//AudioPlayer::AudioFormat audioFormat;
		bool finished;
		bool loop;
		bool stopRequested;
		float volume;
		uint32_t channels;
		uint32_t sampleRate;
		uint32_t bitsPerSample;

		LPDIRECTSOUNDSTREAM	directSoundStream;	
		DWORD packetStatus[AUDIO_PACKETS];
		DWORD completedSize[AUDIO_PACKETS];
		char *decodeBuffer;

	} AudioContainer;

    uint32_t m_maxAudioContainerID;
    std::map<uint32_t, AudioContainer> m_audioContainerMap;
}

void AudioManager::Init()
{
    m_maxAudioContainerID = 0;

    HRESULT hr = DirectSoundCreate( NULL, &m_directSoundDevice, NULL);
    //return SUCCEEDED(hr);
}

AudioManager* AudioManager::GetInstance()
{
	if (m_instance == NULL) 
	{
    	m_instance = new AudioManager();
    }
    return m_instance;
}

void AudioManager::Close()
{
    for (std::map<uint32_t, AudioContainer>::iterator it = m_audioContainerMap.begin(); it != m_audioContainerMap.end(); ++it)
    {
        // while(it->second.isPlaying)
        // {
        //     it->second.killSound = 1;
        //     Sleep(200);
        // }
        if (it->second.decodeBuffer == NULL) 
        {
            free(it->second.decodeBuffer);
        }
        if (it->second.directSoundStream != NULL) 
        {
            it->second.directSoundStream->Release();
        }
    }
    if (m_directSoundDevice != NULL) 
    {
        m_directSoundDevice->Release();
    }

	delete m_instance;
	m_instance = NULL;
}

void AudioManager::Update()
{

}

uint32_t AudioManager::PlayAudio(const std::string path, bool loop)
{
    std::wstring mappedPath;
	if (path.length() == 0 || ConfigLoader::MapPath(StringUtility::ToWideString(path), mappedPath) == false)
	{
		return 0;
	}

	drwav wav;
    if (drwav_init_file(&wav, StringUtility::ToString(mappedPath).c_str(), NULL) != 1) 
	{
        return 0;
    }

	WAVEFORMATEX waveFormat;
	memset(&waveFormat, 0, sizeof(waveFormat));
	waveFormat.wFormatTag = WAVE_FORMAT_PCM;
	waveFormat.nChannels = (WORD)wav.channels;
	waveFormat.nSamplesPerSec = wav.sampleRate;
	waveFormat.wBitsPerSample = (WORD)wav.bitsPerSample;
	waveFormat.nBlockAlign = (waveFormat.wBitsPerSample >> 3) * waveFormat.nChannels;
	waveFormat.nAvgBytesPerSec = waveFormat.nBlockAlign * waveFormat.nSamplesPerSec;

	DSSTREAMDESC streamDesc;
	memset(&streamDesc, 0, sizeof(streamDesc));
	streamDesc.dwMaxAttachedPackets = AUDIO_PACKETS;
	streamDesc.lpwfxFormat = &waveFormat;

	LPDIRECTSOUNDSTREAM	directSoundStream;
	m_directSoundDevice->CreateSoundStream(&streamDesc, &directSoundStream, NULL); 

	DSMIXBINVOLUMEPAIR mixBinVolumePair[6];
    mixBinVolumePair[0].dwMixBin = DSMIXBIN_FRONT_LEFT;
    mixBinVolumePair[0].lVolume = DSBVOLUME_MAX;
    mixBinVolumePair[1].dwMixBin = DSMIXBIN_FRONT_RIGHT;
    mixBinVolumePair[1].lVolume = DSBVOLUME_MAX;
    mixBinVolumePair[2].dwMixBin = DSMIXBIN_FRONT_CENTER;
    mixBinVolumePair[2].lVolume = DSBVOLUME_MIN;
    mixBinVolumePair[3].dwMixBin = DSMIXBIN_LOW_FREQUENCY;
    mixBinVolumePair[3].lVolume = DSBVOLUME_MIN;
    mixBinVolumePair[4].dwMixBin = DSMIXBIN_BACK_LEFT;
    mixBinVolumePair[4].lVolume = DSBVOLUME_MIN;
    mixBinVolumePair[5].dwMixBin = DSMIXBIN_BACK_RIGHT;
    mixBinVolumePair[5].lVolume = DSBVOLUME_MIN;

	DSMIXBINS mixBins;                
	mixBins.dwMixBinCount = 6;
    mixBins.lpMixBinVolumePairs = mixBinVolumePair;

	directSoundStream->SetVolume(DSBVOLUME_MAX);
	directSoundStream->SetHeadroom(0);
	directSoundStream->SetMixBins(&mixBins);

	AudioContainer audioContainer;
	audioContainer.finished = false;
	audioContainer.loop = false;
	audioContainer.stopRequested = false;
	audioContainer.channels = waveFormat.nChannels;
	audioContainer.sampleRate = waveFormat.nSamplesPerSec;
	audioContainer.bitsPerSample = waveFormat.wBitsPerSample;
	audioContainer.volume = 1.0f;
	audioContainer.directSoundStream = directSoundStream;
	uint64_t frameByteSize = audioContainer.channels * (audioContainer.bitsPerSample >> 3) * AUDIO_FRAME_SAMPLES;
	audioContainer.decodeBuffer = (char *)malloc((size_t)(AUDIO_PACKETS * frameByteSize));
	
	for (uint32_t i = 0; i < AUDIO_PACKETS; i++) 
	{
		audioContainer.packetStatus[i] = XMEDIAPACKET_STATUS_SUCCESS;
	}

    uint32_t audioContainerID = ++m_maxAudioContainerID;
    m_audioContainerMap.insert(std::pair<uint32_t, AudioContainer>(audioContainerID, audioContainer));

	while (audioContainer.stopRequested == false)
	{
		char* decodeBuffer = audioContainer.decodeBuffer;
		for(int i = 0; i < AUDIO_PACKETS; i++)
		{
			if(audioContainer.packetStatus[i] != XMEDIAPACKET_STATUS_PENDING)
			{
				if (audioContainer.finished == true) 
				{
					if (audioContainer.loop == true)
					{
						drwav_bool32 wavResult = drwav_seek_to_pcm_frame(&wav, 0);
						audioContainer.finished = false;
					}
					else
					{
						audioContainer.stopRequested = true;
						break;
					} 
				}
				drwav_uint64 framesRead = drwav_read_pcm_frames_s16(&wav, AUDIO_FRAME_SAMPLES, (drwav_int16*)decodeBuffer);
				if (framesRead < AUDIO_FRAME_SAMPLES)
				{
					uint64_t bytesRead = framesRead * audioContainer.channels * (audioContainer.bitsPerSample >> 3);
					memset(decodeBuffer + bytesRead, 0, (size_t)(frameByteSize - bytesRead));
					audioContainer.finished = true;
				}

				XMEDIAPACKET mediaPacket;
				memset(&mediaPacket, 0, sizeof(mediaPacket));
				mediaPacket.pvBuffer = decodeBuffer;
				mediaPacket.dwMaxSize = (DWORD)frameByteSize;
				mediaPacket.pdwCompletedSize = &audioContainer.completedSize[i];
				mediaPacket.pdwStatus = &audioContainer.packetStatus[i];
 				audioContainer.directSoundStream->Process(&mediaPacket, NULL);
			}
			decodeBuffer += frameByteSize;
		}

		DirectSoundDoWork();
		Sleep(100);

		if (audioContainer.stopRequested == true)
		{
			audioContainer.directSoundStream->FlushEx( 0, DSSTREAMFLUSHEX_ASYNC);
			DirectSoundDoWork();
			Sleep(100);
		}
	}

	audioContainer.directSoundStream->Release();
	free(audioContainer.decodeBuffer);
	drwav_uninit(&wav);
	return audioContainerID;
}

#endif