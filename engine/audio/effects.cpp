#include "audio/effects.h"
#include "audio/audio.h"
#include "base/memory.h"

#include <math.h>
#include <string.h>

namespace nx
{
    namespace
    {
        constexpr float kPi = 3.14159265f;
        constexpr u32 kCombs = 4, kAllPasses = 2;
        // Freeverb's lengths, made for 44.1 kHz, for 48: each channel's a little apart, so that the room is wide.
        constexpr u32 kCombLength[kCombs] = { 1215, 1293, 1390, 1476 };
        constexpr u32 kAllPassLength[kAllPasses] = { 605, 480 };
        constexpr u32 kSpread = 25;
        constexpr u32 kEchoFrames = static_cast<u32>(kMostEcho * kMixRate);

        float clampFloat(float v, float low, float high)
        {
            return !(v >= low) ? low : v > high ? high : v;
        }

        // Very small numbers that a fading tail leaves are slow to work with on some machines: they are nothing.
        float flush(float v)
        {
            return fabsf(v) < 1e-12f ? 0.0f : v;
        }
    }

    struct ReverbMemory
    {
        float comb[2][kCombs][kCombLength[kCombs - 1] + kSpread];
        float combStore[2][kCombs];
        u32 combAt[2][kCombs];
        float allPass[2][kAllPasses][kAllPassLength[0] + kSpread];
        u32 allPassAt[2][kAllPasses];
    };

    struct EchoMemory
    {
        i16 frames[kEchoFrames * 2];
        u32 at;
    };

    ReverbMemory* createReverbMemory()
    {
        ReverbMemory* m = static_cast<ReverbMemory*>(memAlloc(sizeof(ReverbMemory), MemTag::Audio));
        if (m)
        {
            memset(m, 0, sizeof(*m));
        }
        return m;
    }

    EchoMemory* createEchoMemory()
    {
        EchoMemory* m = static_cast<EchoMemory*>(memAlloc(sizeof(EchoMemory), MemTag::Audio));
        if (m)
        {
            memset(m, 0, sizeof(*m));
        }
        return m;
    }

    void destroyReverbMemory(ReverbMemory* memory) { memFree(memory); }
    void destroyEchoMemory(EchoMemory* memory) { memFree(memory); }

    // The usual biquads, from Robert Bristow-Johnson's cookbook, with Q of a half root two: no peak at the corner.
    void BusEffects::Biquad::lowPass(float hertz)
    {
        const float w = 2 * kPi * clampFloat(hertz, 20, 20000) / kMixRate;
        const float alpha = sinf(w) / (2 * 0.7071f), c = cosf(w), a0 = 1 + alpha;
        b0 = (1 - c) / 2 / a0;
        b1 = (1 - c) / a0;
        b2 = b0;
        a1 = -2 * c / a0;
        a2 = (1 - alpha) / a0;
    }

    void BusEffects::Biquad::highPass(float hertz)
    {
        const float w = 2 * kPi * clampFloat(hertz, 10, 20000) / kMixRate;
        const float alpha = sinf(w) / (2 * 0.7071f), c = cosf(w), a0 = 1 + alpha;
        b0 = (1 + c) / 2 / a0;
        b1 = -(1 + c) / a0;
        b2 = b0;
        a1 = -2 * c / a0;
        a2 = (1 - alpha) / a0;
    }

    float BusEffects::Biquad::run(u32 ch, float x)
    {
        const float y = flush(b0 * x + b1 * x1[ch] + b2 * x2[ch] - a1 * y1[ch] - a2 * y2[ch]);
        x2[ch] = x1[ch];
        x1[ch] = x;
        y2[ch] = y1[ch];
        y1[ch] = y;
        return y;
    }

    void BusEffects::set(const BusEffectSettings& goal, float seconds)
    {
        m_goal = goal;
        m_goal.lowPass = clampFloat(goal.lowPass, 20, 20000);
        m_goal.highPass = clampFloat(goal.highPass, 0, 20000);
        m_goal.reverb = clampFloat(goal.reverb, 0, 1);
        m_goal.room = clampFloat(goal.room, 0, 1);
        m_goal.echo = clampFloat(goal.echo, 0, 1);
        m_goal.echoDelay = clampFloat(goal.echoDelay, 0.02f, kMostEcho);
        m_goal.echoFeedback = clampFloat(goal.echoFeedback, 0, 0.9f);
        m_now.room = m_goal.room;
        m_now.echoDelay = m_goal.echoDelay;
        m_now.echoFeedback = m_goal.echoFeedback;
        m_glide = seconds > 0 ? seconds : 0;
        if (m_glide == 0)
        {
            m_now = m_goal;
        }
    }

    bool BusEffects::busy() const
    {
        // Reverb and echo go on being heard after they are turned down, until their tails have gone.
        return m_now.lowPass < 19900 || m_goal.lowPass < 19900 || m_now.highPass > 10 || m_goal.highPass > 10 || m_now.reverb > 0 || m_goal.reverb > 0 ||
               m_now.echo > 0 || m_goal.echo > 0;
    }

    void BusEffects::process(float* stereo, u32 frames)
    {
        if (m_glide > 0)
        {
            const float seconds = static_cast<float>(frames) / kMixRate;
            const float t = seconds >= m_glide ? 1 : seconds / m_glide;
            m_glide = seconds >= m_glide ? 0 : m_glide - seconds;
            // Frequencies glide by octaves, which is how they are heard.
            m_now.lowPass = expf(logf(m_now.lowPass) + (logf(m_goal.lowPass) - logf(m_now.lowPass)) * t);
            const float highFrom = m_now.highPass > 10 ? m_now.highPass : 10, highTo = m_goal.highPass > 10 ? m_goal.highPass : 10;
            m_now.highPass = expf(logf(highFrom) + (logf(highTo) - logf(highFrom)) * t);
            m_now.highPass = m_now.highPass <= 10.5f && m_goal.highPass <= 10 ? 0 : m_now.highPass;
            m_now.reverb += (m_goal.reverb - m_now.reverb) * t;
            m_now.echo += (m_goal.echo - m_now.echo) * t;
        }

        if (m_now.lowPass < 19900)
        {
            if (m_now.lowPass != m_lowSet)
            {
                m_low.lowPass(m_now.lowPass);
                m_lowSet = m_now.lowPass;
            }
            for (u32 i = 0; i < frames; i++)
            {
                stereo[i * 2] = m_low.run(0, stereo[i * 2]);
                stereo[i * 2 + 1] = m_low.run(1, stereo[i * 2 + 1]);
            }
        }
        if (m_now.highPass > 10)
        {
            if (m_now.highPass != m_highSet)
            {
                m_high.highPass(m_now.highPass);
                m_highSet = m_now.highPass;
            }
            for (u32 i = 0; i < frames; i++)
            {
                stereo[i * 2] = m_high.run(0, stereo[i * 2]);
                stereo[i * 2 + 1] = m_high.run(1, stereo[i * 2 + 1]);
            }
        }

        if (m_echo && (m_now.echo > 0 || m_goal.echo > 0))
        {
            EchoMemory& e = *m_echo;
            const u32 delay = static_cast<u32>(m_now.echoDelay * kMixRate);
            for (u32 i = 0; i < frames; i++)
            {
                const u32 from = (e.at + kEchoFrames - delay) % kEchoFrames;
                for (u32 ch = 0; ch < 2; ch++)
                {
                    const float heard = e.frames[from * 2 + ch];
                    const float in = stereo[i * 2 + ch];
                    const float kept = clampFloat(in * m_now.echo + heard * m_now.echoFeedback, -32767, 32767);
                    e.frames[e.at * 2 + ch] = static_cast<i16>(kept);
                    stereo[i * 2 + ch] = in + heard;
                }
                e.at = (e.at + 1) % kEchoFrames;
            }
        }

        if (m_reverb && (m_now.reverb > 0 || m_goal.reverb > 0))
        {
            // Freeverb: the sound into comb filters side by side, which ring as a room does, then all-pass filters
            // one after another, which smear it.
            ReverbMemory& r = *m_reverb;
            const float feedback = 0.7f + m_now.room * 0.28f;
            const float damp = 0.2f + (1 - m_now.room) * 0.2f;
            const float wet = m_now.reverb * 2;
            for (u32 i = 0; i < frames; i++)
            {
                const float in = (stereo[i * 2] + stereo[i * 2 + 1]) * 0.015f;
                for (u32 ch = 0; ch < 2; ch++)
                {
                    float out = 0;
                    for (u32 c = 0; c < kCombs; c++)
                    {
                        const u32 length = kCombLength[c] + (ch ? kSpread : 0);
                        u32& at = r.combAt[ch][c];
                        float& store = r.combStore[ch][c];
                        const float was = r.comb[ch][c][at];
                        out += was;
                        store = flush(was * (1 - damp) + store * damp);
                        r.comb[ch][c][at] = in + store * feedback;
                        at = at + 1 == length ? 0 : at + 1;
                    }
                    for (u32 a = 0; a < kAllPasses; a++)
                    {
                        const u32 length = kAllPassLength[a] + (ch ? kSpread : 0);
                        u32& at = r.allPassAt[ch][a];
                        const float was = r.allPass[ch][a][at];
                        r.allPass[ch][a][at] = flush(out + was * 0.5f);
                        out = was - out;
                        at = at + 1 == length ? 0 : at + 1;
                    }
                    stereo[i * 2 + ch] += out * wet;
                }
            }
        }
    }
}
