#pragma once

#include "base/types.h"

// What a bus's sound goes through before it is heard: filters that take away the highs or the lows, a room's reverb,
// and an echo. All of it runs on the mixing thread; the memory reverb and echo need is made on the game thread and
// handed over, so that mixing never allocates.
namespace nx
{
    struct BusEffectSettings
    {
        float lowPass = 20000;     // Hz: what is above it fades; 20000 lets everything through
        float highPass = 0;        // Hz: what is below it fades; 0 lets everything through
        float reverb = 0;          // 0 to 1, how much of the room is heard
        float room = 0.6f;         // 0 to 1, small and dead to large and long
        float echo = 0;            // 0 to 1, how loud the echo is
        float echoDelay = 0.3f;    // seconds, up to kMostEcho
        float echoFeedback = 0.4f; // 0 to 0.9, how much of each echo echoes again
    };

    constexpr float kMostEcho = 0.75f;

    struct ReverbMemory;
    struct EchoMemory;
    ReverbMemory* createReverbMemory();
    EchoMemory* createEchoMemory();
    void destroyReverbMemory(ReverbMemory* memory);
    void destroyEchoMemory(EchoMemory* memory);

    class BusEffects
    {
    public:
        // Filters and amounts glide there over so many seconds; the room, delay and feedback change at once.
        void set(const BusEffectSettings& goal, float seconds);
        void giveReverb(ReverbMemory* memory) { m_reverb = memory; }
        void giveEcho(EchoMemory* memory) { m_echo = memory; }
        // Stereo frames, in place.
        void process(float* stereo, u32 frames);
        bool busy() const;

    private:
        struct Biquad
        {
            float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
            float x1[2] = {}, x2[2] = {}, y1[2] = {}, y2[2] = {};
            void lowPass(float hertz);
            void highPass(float hertz);
            float run(u32 channel, float x);
        };

        BusEffectSettings m_goal;
        BusEffectSettings m_now;
        float m_glide = 0;       // seconds left
        Biquad m_low, m_high;
        float m_lowSet = -1, m_highSet = -1;
        ReverbMemory* m_reverb = nullptr;
        EchoMemory* m_echo = nullptr;
    };
}
