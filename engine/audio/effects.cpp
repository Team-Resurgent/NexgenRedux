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

    // What was heard in the last 40 ms or so, for chorus and flanger to read a little behind.
    constexpr u32 kWaverFrames = 2048;
    struct WaverMemory
    {
        float chorus[kWaverFrames * 2];
        float flanger[kWaverFrames * 2];
        u32 at;
    };

    WaverMemory* createWaverMemory()
    {
        WaverMemory* m = static_cast<WaverMemory*>(memAlloc(sizeof(WaverMemory), MemTag::Audio));
        if (m)
        {
            memset(m, 0, sizeof(*m));
        }
        return m;
    }

    void destroyWaverMemory(WaverMemory* memory) { memFree(memory); }

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

    // Shelves and a peak from the same cookbook, for the equaliser.
    void BusEffects::Biquad::lowShelf(float hertz, float db)
    {
        const float a = powf(10, db / 40), w = 2 * kPi * hertz / kMixRate, c = cosf(w), s = sinf(w);
        const float alpha = s / 2 * sqrtf(2), root = 2 * sqrtf(a) * alpha;
        const float a0 = (a + 1) + (a - 1) * c + root;
        b0 = a * ((a + 1) - (a - 1) * c + root) / a0;
        b1 = 2 * a * ((a - 1) - (a + 1) * c) / a0;
        b2 = a * ((a + 1) - (a - 1) * c - root) / a0;
        a1 = -2 * ((a - 1) + (a + 1) * c) / a0;
        a2 = ((a + 1) + (a - 1) * c - root) / a0;
    }

    void BusEffects::Biquad::peak(float hertz, float db)
    {
        const float a = powf(10, db / 40), w = 2 * kPi * hertz / kMixRate, alpha = sinf(w) / (2 * 0.7f), c = cosf(w);
        const float a0 = 1 + alpha / a;
        b0 = (1 + alpha * a) / a0;
        b1 = -2 * c / a0;
        b2 = (1 - alpha * a) / a0;
        a1 = b1;
        a2 = (1 - alpha / a) / a0;
    }

    void BusEffects::Biquad::highShelf(float hertz, float db)
    {
        const float a = powf(10, db / 40), w = 2 * kPi * hertz / kMixRate, c = cosf(w), s = sinf(w);
        const float alpha = s / 2 * sqrtf(2), root = 2 * sqrtf(a) * alpha;
        const float a0 = (a + 1) - (a - 1) * c + root;
        b0 = a * ((a + 1) + (a - 1) * c + root) / a0;
        b1 = -2 * a * ((a - 1) + (a + 1) * c) / a0;
        b2 = a * ((a + 1) + (a - 1) * c - root) / a0;
        a1 = 2 * ((a - 1) - (a + 1) * c) / a0;
        a2 = ((a + 1) - (a - 1) * c - root) / a0;
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
        m_goal.distortion = clampFloat(goal.distortion, 0, 1);
        m_goal.chorus = clampFloat(goal.chorus, 0, 1);
        m_goal.chorusRate = clampFloat(goal.chorusRate, 0.05f, 10);
        m_goal.flanger = clampFloat(goal.flanger, 0, 1);
        m_goal.flangerRate = clampFloat(goal.flangerRate, 0.02f, 5);
        m_goal.flangerFeedback = clampFloat(goal.flangerFeedback, 0, 0.9f);
        m_goal.threshold = clampFloat(goal.threshold, -60, 0);
        m_goal.ratio = clampFloat(goal.ratio, 1, 20);
        m_goal.low = clampFloat(goal.low, -24, 24);
        m_goal.mid = clampFloat(goal.mid, -24, 24);
        m_goal.high = clampFloat(goal.high, -24, 24);
        m_now.chorusRate = m_goal.chorusRate;
        m_now.flangerRate = m_goal.flangerRate;
        m_now.flangerFeedback = m_goal.flangerFeedback;
        m_now.threshold = m_goal.threshold;
        m_now.ratio = m_goal.ratio;
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
               m_now.echo > 0 || m_goal.echo > 0 || m_now.distortion > 0 || m_goal.distortion > 0 || m_now.chorus > 0 || m_goal.chorus > 0 ||
               m_now.flanger > 0 || m_goal.flanger > 0 || m_now.threshold < 0 || m_gain < 0.999f || m_now.low != 0 || m_goal.low != 0 ||
               m_now.mid != 0 || m_goal.mid != 0 || m_now.high != 0 || m_goal.high != 0;
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
            m_now.distortion += (m_goal.distortion - m_now.distortion) * t;
            m_now.chorus += (m_goal.chorus - m_now.chorus) * t;
            m_now.flanger += (m_goal.flanger - m_now.flanger) * t;
            m_now.low += (m_goal.low - m_now.low) * t;
            m_now.mid += (m_goal.mid - m_now.mid) * t;
            m_now.high += (m_goal.high - m_now.high) * t;
        }

        // The equaliser: a shelf under the lows, a peak in the middle and a shelf over the highs.
        const float gains[3] = { m_now.low, m_now.mid, m_now.high };
        for (u32 band = 0; band < 3; band++)
        {
            if (fabsf(gains[band]) < 0.01f)
            {
                continue;
            }
            if (gains[band] != m_eqSet[band])
            {
                if (m_eqSet[band] == -100)
                {
                    m_eq[band] = Biquad();
                }
                band == 0 ? m_eq[0].lowShelf(250, gains[0]) : band == 1 ? m_eq[1].peak(1000, gains[1]) : m_eq[2].highShelf(4000, gains[2]);
                m_eqSet[band] = gains[band];
            }
            for (u32 i = 0; i < frames; i++)
            {
                stereo[i * 2] = m_eq[band].run(0, stereo[i * 2]);
                stereo[i * 2 + 1] = m_eq[band].run(1, stereo[i * 2 + 1]);
            }
        }

        if (m_now.distortion > 0)
        {
            // Driven into a curve that flattens what is loud, as an amplifier pushed too far: tanh, near enough, and as
            // loud after as before at the top.
            const float drive = 1 + m_now.distortion * 24;
            const auto curve = [](float x) { x = clampFloat(x, -3, 3); return x * (27 + x * x) / (27 + 9 * x * x); };
            const float level = 1 / curve(drive);
            for (u32 i = 0; i < frames * 2; i++)
            {
                stereo[i] = curve(stereo[i] / 32768.0f * drive) * level * 32768.0f;
            }
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

        if (m_waver && (m_now.chorus > 0 || m_goal.chorus > 0 || m_now.flanger > 0 || m_goal.flanger > 0))
        {
            waver(stereo, frames);
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

        if (m_now.threshold < 0 || m_gain < 0.999f)
        {
            compress(stereo, frames);
        }
    }

    // Two copies read a little behind what is heard, the delay swaying: chorus by 15 to 25 ms, so it sounds like
    // more than one; flanger by 1 to 5 ms, fed back, which sweeps a comb through the sound. Left and right sway apart.
    void BusEffects::waver(float* stereo, u32 frames)
    {
        WaverMemory& w = *m_waver;
        const float step = 2 * kPi * m_now.chorusRate / kMixRate, flangerStep = 2 * kPi * m_now.flangerRate / kMixRate;
        const auto read = [](const float* line, float behind, u32 at, u32 ch) {
            const float back = static_cast<float>(at) + kWaverFrames - behind;
            const u32 whole = static_cast<u32>(back);
            const float part = back - static_cast<float>(whole);
            const float a = line[(whole % kWaverFrames) * 2 + ch], b = line[((whole + 1) % kWaverFrames) * 2 + ch];
            return a + (b - a) * part;
        };
        for (u32 i = 0; i < frames; i++)
        {
            for (u32 ch = 0; ch < 2; ch++)
            {
                const float in = stereo[i * 2 + ch];
                const float sway = ch ? kPi * 0.5f : 0;
                const float chorus = read(w.chorus, (0.020f + 0.005f * sinf(m_phase + sway)) * kMixRate, w.at, ch);
                const float flanger = read(w.flanger, (0.003f + 0.002f * sinf(m_flangerPhase + sway)) * kMixRate, w.at, ch);
                w.chorus[w.at * 2 + ch] = in;
                w.flanger[w.at * 2 + ch] = flush(clampFloat(in + flanger * m_now.flangerFeedback, -65536, 65536));
                stereo[i * 2 + ch] = in + chorus * m_now.chorus + flanger * m_now.flanger;
            }
            w.at = (w.at + 1) % kWaverFrames;
            m_phase += step;
            m_flangerPhase += flangerStep;
        }
        m_phase = fmodf(m_phase, 2 * kPi);
        m_flangerPhase = fmodf(m_flangerPhase, 2 * kPi);
    }

    // What is louder than the threshold is pressed down by the ratio: quick to press, slow to let go. Worked out every
    // 16 frames, and glided between.
    void BusEffects::compress(float* stereo, u32 frames)
    {
        const float attack = 1 - expf(-16.0f / (0.005f * kMixRate)), release = 1 - expf(-16.0f / (0.15f * kMixRate));
        for (u32 start = 0; start < frames; start += 16)
        {
            const u32 count = frames - start < 16 ? frames - start : 16;
            float peak = 0;
            for (u32 i = start; i < start + count; i++)
            {
                peak = fmaxf(peak, fmaxf(fabsf(stereo[i * 2]), fabsf(stereo[i * 2 + 1])));
            }
            peak /= 32768.0f;
            m_envelope += (peak - m_envelope) * (peak > m_envelope ? attack : release);
            float goal = 1;
            if (m_now.threshold < 0 && m_envelope > 1e-5f)
            {
                const float over = 20 * log10f(m_envelope) - m_now.threshold;
                goal = over > 0 ? powf(10, -over * (1 - 1 / m_now.ratio) / 20) : 1;
            }
            const float from = m_gain, by = (goal - from) / static_cast<float>(count);
            for (u32 i = 0; i < count; i++)
            {
                const float g = from + by * static_cast<float>(i + 1);
                stereo[(start + i) * 2] *= g;
                stereo[(start + i) * 2 + 1] *= g;
            }
            m_gain = goal;
        }
    }
}
