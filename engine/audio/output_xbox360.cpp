#include "audio/audio.h"
#include "base/log.h"
#include "base/memory.h"
#include "base/thread.h"

#include <xtl.h>
#include <xaudio2.h>

#include <atomic>
#include <string.h>

// The Xbox 360's sound: XAudio2, one voice, fed buffers in turn by a thread of our own.
namespace nx
{
    namespace
    {
        constexpr u32 kBuffers = 4;
        constexpr u32 kFrames = 1024;  // 21 ms

        class Xbox360AudioOutput final : public AudioOutput
        {
        public:
            ~Xbox360AudioOutput() override { stop(); }

            bool start(Mix mix, void* user) override
            {
                m_mix = mix;
                m_user = user;
                if (FAILED(XAudio2Create(&m_audio, 0, XAUDIO2_DEFAULT_PROCESSOR)))
                {
                    NX_ERROR("audio: no XAudio2");
                    return false;
                }
                if (FAILED(m_audio->CreateMasteringVoice(&m_master, XAUDIO2_DEFAULT_CHANNELS, XAUDIO2_DEFAULT_SAMPLERATE, 0, 0, nullptr)))
                {
                    NX_ERROR("audio: no output");
                    return false;
                }
                WAVEFORMATEX format = {};
                format.wFormatTag = WAVE_FORMAT_PCM;
                format.nChannels = 2;
                format.nSamplesPerSec = kMixRate;
                format.wBitsPerSample = 16;
                format.nBlockAlign = 4;
                format.nAvgBytesPerSec = kMixRate * 4;
                if (FAILED(m_audio->CreateSourceVoice(&m_voice, &format, 0, XAUDIO2_DEFAULT_FREQ_RATIO, nullptr, nullptr, nullptr)))
                {
                    NX_ERROR("audio: no voice");
                    return false;
                }
                memset(m_samples, 0, sizeof(m_samples));
                m_voice->Start(0, 0);
                m_running.store(true);
                m_thread = startThread(runThread, this, ThreadPriority::Audio);
                return static_cast<bool>(m_thread);
            }

            void stop() override
            {
                m_running.store(false);
                if (m_thread)
                {
                    joinThread(m_thread);
                    m_thread = {};
                }
                if (m_voice)
                {
                    m_voice->Stop(0, 0);
                    m_voice->DestroyVoice();
                    m_voice = nullptr;
                }
                if (m_master)
                {
                    m_master->DestroyVoice();
                    m_master = nullptr;
                }
                if (m_audio)
                {
                    m_audio->Release();
                    m_audio = nullptr;
                }
            }

            const char* name() const override { return "XAudio2"; }

        private:
            static void runThread(void* self) { static_cast<Xbox360AudioOutput*>(self)->run(); }

            // Each buffer the voice has finished with is mixed again and sent back.
            void run()
            {
                u32 next = 0;
                while (m_running.load())
                {
                    XAUDIO2_VOICE_STATE state;
                    m_voice->GetState(&state);
                    for (u32 queued = state.BuffersQueued; queued < kBuffers; queued++)
                    {
                        i16* samples = m_samples[next];
                        m_mix(samples, kFrames, m_user);
                        XAUDIO2_BUFFER buffer = {};
                        buffer.AudioBytes = kFrames * 4;
                        buffer.pAudioData = reinterpret_cast<const BYTE*>(samples);
                        m_voice->SubmitSourceBuffer(&buffer, nullptr);
                        next = (next + 1) % kBuffers;
                    }
                    sleepMilliseconds(4);
                }
            }

            IXAudio2* m_audio = nullptr;
            IXAudio2MasteringVoice* m_master = nullptr;
            IXAudio2SourceVoice* m_voice = nullptr;
            i16 m_samples[kBuffers][kFrames * 2];
            Mix m_mix = nullptr;
            void* m_user = nullptr;
            ThreadHandle m_thread;
            std::atomic<bool> m_running{ false };
        };
    }

    AudioOutput* createAudioOutput()
    {
        return create<Xbox360AudioOutput>(MemTag::Audio);
    }
}
