#include "audio/audio.h"
#include "base/log.h"
#include "base/memory.h"
#include "base/thread.h"

#include <xtl.h>

#include <string.h>

namespace nx
{
    namespace
    {
        constexpr DWORD kChunks = 4;
        constexpr DWORD kChunkFrames = 1024;  // 21 ms
        constexpr DWORD kChunkBytes = kChunkFrames * 4;

        // One looping hardware buffer in four parts. A thread fills the part after the one being played.
        class XboxAudioOutput final : public AudioOutput
        {
        public:
            ~XboxAudioOutput() override { stop(); }

            bool start(Mix mix, void* user) override
            {
                m_mix = mix;
                m_user = user;
                if (FAILED(DirectSoundCreate(nullptr, &m_sound, nullptr)))
                {
                    NX_ERROR("DirectSoundCreate failed");
                    return false;
                }
                WAVEFORMATEX format = {};
                format.wFormatTag = WAVE_FORMAT_PCM;
                format.nChannels = 2;
                format.nSamplesPerSec = kMixRate;
                format.wBitsPerSample = 16;
                format.nBlockAlign = 4;
                format.nAvgBytesPerSec = kMixRate * 4;
                DSBUFFERDESC desc = {};
                desc.dwSize = sizeof(desc);
                desc.dwBufferBytes = kChunkBytes * kChunks;
                desc.lpwfxFormat = &format;
                if (FAILED(m_sound->CreateSoundBuffer(&desc, &m_buffer, nullptr)))
                {
                    NX_ERROR("no sound buffer");
                    return false;
                }
                void* memory = nullptr;
                DWORD bytes = 0;
                if (FAILED(m_buffer->Lock(0, kChunkBytes * kChunks, &memory, &bytes, nullptr, nullptr, 0)))
                {
                    return false;
                }
                memset(memory, 0, bytes);
                m_buffer->Unlock(memory, bytes, nullptr, 0);
                // The hardware's default leaves headroom for sixty-four voices; there is only this one.
                m_buffer->SetHeadroom(0);
                if (FAILED(m_buffer->Play(0, 0, DSBPLAY_LOOPING)))
                {
                    return false;
                }
                m_running.store(true);
                m_thread = startThread([](void* self) { static_cast<XboxAudioOutput*>(self)->run(); }, this, ThreadPriority::Audio);
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
                if (m_buffer)
                {
                    m_buffer->Stop();
                    m_buffer->Release();
                    m_buffer = nullptr;
                }
                if (m_sound)
                {
                    m_sound->Release();
                    m_sound = nullptr;
                }
            }

            const char* name() const override { return "DirectSound"; }

        private:
            void run()
            {
                DWORD filled = 0;
                while (m_running.load())
                {
                    DirectSoundDoWork();
                    DWORD play = 0, write = 0;
                    if (SUCCEEDED(m_buffer->GetCurrentPosition(&play, &write)))
                    {
                        // Everything except the part being played, up to the one before it.
                        const DWORD playing = play / kChunkBytes % kChunks;
                        while (filled != playing)
                        {
                            void* memory = nullptr;
                            DWORD bytes = 0;
                            if (FAILED(m_buffer->Lock(filled * kChunkBytes, kChunkBytes, &memory, &bytes, nullptr, nullptr, 0)))
                            {
                                break;
                            }
                            m_mix(static_cast<i16*>(memory), bytes / 4, m_user);
                            m_buffer->Unlock(memory, bytes, nullptr, 0);
                            filled = (filled + 1) % kChunks;
                        }
                    }
                    sleepMilliseconds(4);
                }
            }

            LPDIRECTSOUND m_sound = nullptr;
            LPDIRECTSOUNDBUFFER m_buffer = nullptr;
            Mix m_mix = nullptr;
            void* m_user = nullptr;
            ThreadHandle m_thread;
            std::atomic<bool> m_running{ false };
        };
    }

    AudioOutput* createAudioOutput()
    {
        return create<XboxAudioOutput>(MemTag::Audio);
    }
}
