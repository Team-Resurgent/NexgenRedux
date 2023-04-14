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
#define DR_MP3_IMPLEMENTATION
#include <MinAudio/dr_mp3.h>
#define DR_FLAC_IMPLEMENTATION
#include <MinAudio/dr_flac.h>
#define STB_VORBIS_HEADER_ONLY
#include <STB/stb_vorbis.c>

#include <dmusici.h>
#include <dsound.h>
#include <map>

using namespace Gensys;
using namespace NexgenRedux;

namespace
{
	AudioManager* m_instance = NULL;

    #define AUDIO_PACKETS 16
    #define AUDIO_FRAME_SAMPLES 2048

    LPDIRECTSOUND8 m_directSoundDevice;

	typedef enum AudioDecoderType
	{
		AudioDecoderTypeWav,
		AudioDecoderTypeMp3,
		AudioDecoderTypeFlac,
		AudioDecoderTypeOgg
	} AudioDecoderType;
	
	typedef struct AudioContainer {	
		
		bool finished;
		bool loop;
		bool stopRequested;
		bool canDelete;
		float volume;
		uint32_t channels;
		uint32_t sampleRate;
		uint32_t bitsPerSample;
		uint32_t frameByteSize;

		LPDIRECTSOUNDSTREAM	directSoundStream;	
		DWORD packetStatus[AUDIO_PACKETS];
		DWORD completedSize[AUDIO_PACKETS];

		AudioDecoderType decoderType;
		void* decoder;
		char* decodeBuffer;

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
    for (std::map<uint32_t, AudioContainer>::iterator it = m_audioContainerMap.begin(); it != m_audioContainerMap.end();)
    {
        if (it->second.canDelete == true)
        {
            std::map<uint32_t, AudioContainer>::iterator itErase = it;
            ++it;

            m_audioContainerMap.erase(itErase);
            continue;
        }
        ++it;
    }
}

void PlayAudioContainer(uint32_t audioContainerID)
{
	std::map<uint32_t, AudioContainer>::iterator it = m_audioContainerMap.find(audioContainerID);
	if (it == m_audioContainerMap.end()) {
		return;
	}

	AudioContainer* audioContainer = &it->second;

	while (audioContainer->stopRequested == false)
	{
		char* decodeBuffer = audioContainer->decodeBuffer;
		for(int i = 0; i < AUDIO_PACKETS; i++)
		{
			if(audioContainer->packetStatus[i] != XMEDIAPACKET_STATUS_PENDING)
			{
				if (audioContainer->finished == true) 
				{
					if (audioContainer->loop == true)
					{
						if (audioContainer->decoderType == AudioDecoderTypeWav)
						{
							drwav_seek_to_pcm_frame((drwav*)audioContainer->decoder, 0);
						}
						else if (audioContainer->decoderType == AudioDecoderTypeMp3)
						{
							drmp3_seek_to_pcm_frame((drmp3*)audioContainer->decoder, 0);
						}
						else if (audioContainer->decoderType == AudioDecoderTypeFlac)
						{
							drflac_seek_to_pcm_frame((drflac*)audioContainer->decoder, 0);
						}
						else if (audioContainer->decoderType == AudioDecoderTypeOgg)
						{
							stb_vorbis_seek((stb_vorbis*)audioContainer->decoder, 0);
						}
						audioContainer->finished = false;
					}
					else
					{
						audioContainer->stopRequested = true;
						break;
					} 
				}
				uint64_t framesRead = 0;

				if (audioContainer->decoderType == AudioDecoderTypeWav)
				{
					framesRead = (uint64_t)drwav_read_pcm_frames_s16((drwav*)audioContainer->decoder, AUDIO_FRAME_SAMPLES, (drwav_int16*)decodeBuffer);
				}
				else if (audioContainer->decoderType == AudioDecoderTypeMp3)
				{
					framesRead = (uint64_t)drmp3_read_pcm_frames_s16((drmp3*)audioContainer->decoder, AUDIO_FRAME_SAMPLES, (drmp3_int16*)decodeBuffer);
				}
				else if (audioContainer->decoderType == AudioDecoderTypeFlac)
				{
					framesRead = (uint64_t)drflac_read_pcm_frames_s16((drflac*)audioContainer->decoder, AUDIO_FRAME_SAMPLES, (drwav_int16*)decodeBuffer);
				}
				else if (audioContainer->decoderType == AudioDecoderTypeOgg)
				{
					framesRead = (uint64_t)stb_vorbis_get_samples_short_interleaved((stb_vorbis*)audioContainer->decoder, audioContainer->channels, (short*)decodeBuffer, audioContainer->channels * AUDIO_FRAME_SAMPLES);
				}

				if (framesRead < AUDIO_FRAME_SAMPLES)
				{
					uint64_t bytesRead = framesRead * audioContainer->channels * (audioContainer->bitsPerSample >> 3);
					memset(decodeBuffer + bytesRead, 0, (size_t)(audioContainer->frameByteSize - bytesRead));
					audioContainer->finished = true;
				}

				XMEDIAPACKET mediaPacket;
				memset(&mediaPacket, 0, sizeof(mediaPacket));
				mediaPacket.pvBuffer = decodeBuffer;
				mediaPacket.dwMaxSize = (DWORD)audioContainer->frameByteSize;
				mediaPacket.pdwCompletedSize = &audioContainer->completedSize[i];
				mediaPacket.pdwStatus = &audioContainer->packetStatus[i];
 				audioContainer->directSoundStream->Process(&mediaPacket, NULL);
			}
			decodeBuffer += audioContainer->frameByteSize;
		}

		DirectSoundDoWork();
		Sleep(10);

		if (audioContainer->stopRequested == true)
		{
			bool hasPending = true;
			while (hasPending == true)
			{
				DirectSoundDoWork();
				Sleep(10);

				hasPending = false;
				for(int i = 0; i < AUDIO_PACKETS; i++)
				{
					if(audioContainer->packetStatus[i] == XMEDIAPACKET_STATUS_PENDING)
					{
						hasPending = true;
						break;
					}
				}
			}
			audioContainer->directSoundStream->FlushEx(0, DSSTREAMFLUSHEX_ASYNC);
			DirectSoundDoWork();
			Sleep(10);
		}
	}

	if (audioContainer->decoderType == AudioDecoderTypeWav)
	{
		drwav_uninit((drwav*)audioContainer->decoder);
		free(audioContainer->decoder);
	}
	else if (audioContainer->decoderType == AudioDecoderTypeMp3)
	{
		drmp3_uninit((drmp3*)audioContainer->decoder);
		free(audioContainer->decoder);
	}
	else if (audioContainer->decoderType == AudioDecoderTypeMp3)
	{
		drflac_close((drflac*)audioContainer->decoder);
	}
	else if (audioContainer->decoderType == AudioDecoderTypeOgg)
	{
		stb_vorbis_close((stb_vorbis*)audioContainer->decoder);
	}

	audioContainer->directSoundStream->Release();
	free(audioContainer->decodeBuffer);
	audioContainer->canDelete = true;
}

uint32_t AudioManager::PlayAudio(const std::string path, bool loop)
{
    std::wstring mappedPath;
	if (path.length() == 0 || ConfigLoader::MapPath(StringUtility::ToWideString(path), mappedPath) == false)
	{
		return 0;
	}

	uint32_t channels;
	uint32_t sampleRate;
	uint32_t bitsPerSample;
	void* decoder;
	AudioDecoderType decoderType;

	std::wstring extension = StringUtility::UpperCase(FileSystem::GetExtension(mappedPath));
	if (extension == L".WAV")
	{
		drwav* wav = (drwav*)malloc(sizeof(drwav));
		if (drwav_init_file(wav, StringUtility::ToString(mappedPath).c_str(), NULL) != 1) 
		{
			return 0;
		}
		channels = wav->channels;
		sampleRate = wav->sampleRate;
		bitsPerSample = 16;
		decoderType = AudioDecoderTypeWav;
		decoder = wav;
	}
	else if (extension == L".MP3")
	{
		drmp3* mp3 = (drmp3*)malloc(sizeof(drmp3));
		if (drmp3_init_file(mp3, StringUtility::ToString(mappedPath).c_str(), NULL) != 1) 
		{
			return 0;
		}
		channels = mp3->channels;
		sampleRate = mp3->sampleRate;
		bitsPerSample = 16;
		decoderType = AudioDecoderTypeMp3;
		decoder = mp3;
	}
	else if (extension == L".FLAC")
	{
		drflac* flac = drflac_open_file(StringUtility::ToString(mappedPath).c_str(), NULL);
		if (flac == NULL) 
		{
			return 0;
		}
		channels = flac->channels;
		sampleRate = flac->sampleRate;
		bitsPerSample = 16;
		decoderType = AudioDecoderTypeFlac;
		decoder = flac;
	}
	else if (extension == L".OGG")
	{
		stb_vorbis* ogg = stb_vorbis_open_filename(StringUtility::ToString(mappedPath).c_str(), NULL, NULL);
		if (ogg == NULL)
		{
			return 0;
		}
		stb_vorbis_info oggInfo = stb_vorbis_get_info(ogg);
		channels = oggInfo.channels;
		sampleRate = oggInfo.sample_rate;
		bitsPerSample = 16;
		decoderType = AudioDecoderTypeOgg;
		decoder = ogg;
	}
	else
	{
		return 0;
	}

	WAVEFORMATEX waveFormat;
	memset(&waveFormat, 0, sizeof(waveFormat));
	waveFormat.wFormatTag = WAVE_FORMAT_PCM;
	waveFormat.nChannels = (WORD)channels;
	waveFormat.nSamplesPerSec = sampleRate;
	waveFormat.wBitsPerSample = (WORD)bitsPerSample;
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
	audioContainer.loop = loop;
	audioContainer.stopRequested = false;
	audioContainer.canDelete = false;
	audioContainer.channels = channels;
	audioContainer.sampleRate = sampleRate;
	audioContainer.bitsPerSample = bitsPerSample;
	audioContainer.frameByteSize = channels * (bitsPerSample >> 3) * AUDIO_FRAME_SAMPLES;
	audioContainer.volume = 1.0f;
	audioContainer.directSoundStream = directSoundStream;
	audioContainer.decoderType = decoderType;
	audioContainer.decoder = decoder;
	audioContainer.decodeBuffer = (char *)malloc((size_t)(AUDIO_PACKETS * audioContainer.frameByteSize));
	
	for (uint32_t i = 0; i < AUDIO_PACKETS; i++) 
	{
		audioContainer.packetStatus[i] = XMEDIAPACKET_STATUS_SUCCESS;
	}

    uint32_t audioContainerID = ++m_maxAudioContainerID;
    m_audioContainerMap.insert(std::pair<uint32_t, AudioContainer>(audioContainerID, audioContainer));
	CreateThread(NULL, 128, (LPTHREAD_START_ROUTINE)PlayAudioContainer, (void*)audioContainerID, 0, NULL);
	return audioContainerID;
}

#endif