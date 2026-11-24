#pragma once

#include "base/types.h"

#include <atomic>

namespace nx
{
    constexpr u32 kMixRate = 48000;

    enum class SoundKind : u8
    {
        Pcm,    // samples in memory: short effects
        Adpcm,  // cooked: a quarter of the size, decoded while playing
        Ogg,    // compressed in memory, decoded while playing
        Mp3
    };

    // A loaded sound. Shared by every voice that plays it, and never changed after loading.
    struct SoundData
    {
        SoundKind kind = SoundKind::Pcm;
        u32 channels = 0;  // 1 or 2
        u32 rate = 0;
        u32 frames = 0;    // 0 if not known without decoding it all
        u8* data = nullptr;  // from memAlloc
        size_t bytes = 0;
    };

    // Takes a whole file: WAV, Ogg Vorbis, MP3 or cooked. WAV becomes samples; the others stay as they
    // are. nullptr if it can't be read. Free with freeSound.
    SoundData* loadSound(const u8* file, size_t size);
    void freeSound(SoundData* sound);

    enum class Bus : u8 { Effects, Music, Interface, Count };

    struct SoundDecoder;

    struct Voice
    {
        u32 value = 0;
        explicit operator bool() const { return value != 0; }
    };

    struct PlayOptions
    {
        float volume = 1;
        float pan = 0;    // -1 left to 1 right
        float pitch = 1;
        bool loop = false;
        Bus bus = Bus::Effects;
        u8 priority = 128;  // a higher one may take the voice of a lower one
        float fadeIn = 0;   // seconds
    };

    class AudioOutput
    {
    public:
        using Mix = void (*)(i16* stereo, u32 frames, void* user);

        virtual ~AudioOutput() = default;
        // Starts calling mix from its own thread.
        virtual bool start(Mix mix, void* user) = 0;
        virtual void stop() = 0;
        virtual const char* name() const = 0;
    };

    // The platform's sound, or one that plays nothing. Release with nx::destroy.
    AudioOutput* createAudioOutput();
    // Runs its own clock when timed; otherwise only when someone calls Audio::mix.
    AudioOutput* createNullAudioOutput(bool timed);

    // Plays sounds on a fixed number of voices. The game thread sends commands; the mixing thread owns
    // everything that plays. Neither ever waits for the other, and nothing is allocated while mixing.
    class Audio : NonCopyable
    {
    public:
        static constexpr u32 kVoices = 32;

        ~Audio() { shutdown(); }

        // Takes ownership of the output.
        bool init(AudioOutput* output);
        void shutdown();

        // Called when a voice is done with a sound it was given, on the game thread, from update().
        using Finished = void (*)(void* owner, void* user);
        void onFinished(Finished callback, void* user);

        // Once a frame.
        void update();

        // The sound must stay loaded until the finished callback gives back its owner. No voice if
        // all are busy with more important things.
        Voice play(const SoundData& sound, void* owner, const PlayOptions& options);
        void stop(Voice voice, float fadeOut);
        void setVolume(Voice voice, float volume, float seconds);
        void setPan(Voice voice, float pan);
        void setPitch(Voice voice, float pitch);
        void setPaused(Voice voice, bool paused);
        bool playing(Voice voice) const;
        // Seconds into the sound.
        float position(Voice voice) const;

        void setBusVolume(Bus bus, float volume);
        float busVolume(Bus bus) const { return m_busVolume[static_cast<size_t>(bus)]; }
        void setMasterVolume(float volume);
        float masterVolume() const { return m_master; }

        // Stops voices that a screen started, when it closes. Pauses them while it is covered.
        void setGroup(u32 group) { m_group = group; }
        void stopGroup(u32 group, float fadeOut);
        void pauseGroup(u32 group, bool paused);

        u32 voicesInUse() const;
        u32 voicesRefused() const { return m_refused; }
        // How loud the output has been lately, 0 to 1.
        float level() const { return m_level.load(std::memory_order_relaxed); }
        // Strength of one of 32 frequency bands, low to high, 0 to 1.
        float spectrum(u32 band);
        // One of the last 512 output samples, -1 to 1, oldest first.
        float wave(u32 index);

        const char* outputName() const { return m_output ? m_output->name() : "none"; }

        // Mixes the next frames. Called by the output's thread; public for tests.
        void mix(i16* stereo, u32 frames);

    private:
        struct Channel;  // the mixing thread's side of a voice
        struct Command;

        // The game thread's side of a voice.
        struct Slot
        {
            u32 generation = 1;
            bool used = false;
            u8 priority = 0;
            u32 group = 0;
            float startVolume = 0;
            double started = 0;
            void* owner = nullptr;
            std::atomic<u32> finished{ 0 };   // the generation that has finished
            std::atomic<u32> positionMs{ 0 };
            std::atomic<SoundDecoder*> retired{ nullptr };  // closed by the game thread, as it frees memory
        };

        static constexpr u32 kCommands = 256;
        static constexpr u32 kHistory = 2048;

        Slot* slotOf(Voice voice);
        const Slot* slotOf(Voice voice) const;
        bool send(const Command& command);
        void run(const Command& command);
        void mixChannel(u32 index, float* buffer, u32 frames);
        void analyse();

        AudioOutput* m_output = nullptr;
        Slot m_slots[kVoices];
        Channel* m_channels = nullptr;
        Command* m_commands = nullptr;
        std::atomic<u32> m_commandsWritten{ 0 };
        std::atomic<u32> m_commandsRead{ 0 };

        float m_busVolume[static_cast<size_t>(Bus::Count)] = { 1, 1, 1 };
        float m_master = 1;
        // The mixing thread's copies, which glide so a change doesn't click.
        float m_busNow[static_cast<size_t>(Bus::Count)] = { 1, 1, 1 };
        float m_busGoal[static_cast<size_t>(Bus::Count)] = { 1, 1, 1 };
        float m_masterNow = 1, m_masterGoal = 1;

        std::atomic<i16>* m_history = nullptr;  // mono, for analysis only
        std::atomic<u32> m_historyAt{ 0 };
        std::atomic<float> m_level{ 0 };
        float m_bands[32] = {};
        float m_wave[512] = {};
        u32 m_analyse = 0;  // frames left to, since someone last asked

        Finished m_finished = nullptr;
        void* m_finishedUser = nullptr;
        u32 m_group = 0;
        u32 m_refused = 0;
        u64 m_clock = 0;
    };
}
