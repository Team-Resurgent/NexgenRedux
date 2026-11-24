#include "audio/audio.h"
#include "base/memory.h"

extern "C"
{
    int nx_device_start(void (*mix)(short* stereo, unsigned int frames, void* user), void* user, unsigned int rate);
    void nx_device_stop(void);
    const char* nx_device_name(void);
}

namespace nx
{
    namespace
    {
        class DesktopAudioOutput final : public AudioOutput
        {
        public:
            ~DesktopAudioOutput() override { stop(); }

            bool start(Mix mix, void* user) override
            {
                m_started = nx_device_start(mix, user, kMixRate) != 0;
                return m_started;
            }

            void stop() override
            {
                if (m_started)
                {
                    m_started = false;
                    nx_device_stop();  // returns once its thread has finished with us
                }
            }

            const char* name() const override { return m_started ? nx_device_name() : "stopped"; }

        private:
            bool m_started = false;
        };
    }

    AudioOutput* createAudioOutput()
    {
        return create<DesktopAudioOutput>(MemTag::Audio);
    }
}
