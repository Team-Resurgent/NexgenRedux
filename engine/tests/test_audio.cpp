#include "test.h"

#include "assets/formats.h"
#include "audio/audio.h"
#include "audio/decoder.h"
#include "base/memory.h"
#include "base/thread.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <string>

using namespace nx;

namespace
{
    void put(std::string& out, u32 value, int bytes)
    {
        for (int i = 0; i < bytes; i++)
        {
            out += static_cast<char>(value >> (i * 8));
        }
    }

    // A tone, as a WAV file.
    std::string makeWav(u32 frames, u32 channels, u32 rate, float hz, float volume)
    {
        std::string data;
        for (u32 i = 0; i < frames; i++)
        {
            const float v = sinf(6.2831853f * hz * static_cast<float>(i) / static_cast<float>(rate)) * volume;
            for (u32 c = 0; c < channels; c++)
            {
                put(data, static_cast<u16>(static_cast<i16>(v * 32767)), 2);
            }
        }
        std::string out = "RIFF";
        put(out, 36 + static_cast<u32>(data.size()), 4);
        out += "WAVEfmt ";
        put(out, 16, 4);
        put(out, 1, 2);
        put(out, channels, 2);
        put(out, rate, 4);
        put(out, rate * channels * 2, 4);
        put(out, channels * 2, 2);
        put(out, 16, 2);
        out += "data";
        put(out, static_cast<u32>(data.size()), 4);
        return out + data;
    }

    SoundData* load(const std::string& file)
    {
        return loadSound(reinterpret_cast<const u8*>(file.data()), file.size());
    }

    std::string makeAdpcm(const i16* samples, u32 frames, u32 channels, u32 rate)
    {
        std::string out = "NXA1";
        put(out, rate, 4);
        put(out, frames, 4);
        put(out, channels, 4);
        int steps[2] = {};
        for (u32 at = 0; at < frames; at += kAdpcmFrames)
        {
            u8 block[kAdpcmChannelBytes * 2];
            adpcmEncodeBlock(samples + static_cast<size_t>(at) * channels, frames - at < kAdpcmFrames ? frames - at : kAdpcmFrames, channels, steps, block);
            out.append(reinterpret_cast<const char*>(block), adpcmBlockBytes(channels));
        }
        return out;
    }

    struct Mixer
    {
        Audio audio;
        u32 finished = 0;
        i16 out[4800 * 2];

        Mixer()
        {
            audio.init(createNullAudioOutput(false));
            audio.onFinished([](void*, void* self) { static_cast<Mixer*>(self)->finished++; }, this);
        }

        // The loudest sample on each side.
        void mix(u32 frames, int& left, int& right)
        {
            audio.mix(out, frames);
            audio.update();
            left = right = 0;
            for (u32 i = 0; i < frames; i++)
            {
                left = abs(out[i * 2]) > left ? abs(out[i * 2]) : left;
                right = abs(out[i * 2 + 1]) > right ? abs(out[i * 2 + 1]) : right;
            }
        }
    };
}

NX_TEST(soundsPlayAndEnd)
{
    Mixer m;
    SoundData* tone = load(makeWav(4800, 1, 48000, 440, 0.5f));
    NX_EXPECT(tone && tone->channels == 1 && tone->rate == 48000 && tone->frames == 4800);
    int owner = 0, l = 0, r = 0;
    const Voice v = m.audio.play(*tone, &owner, {});
    NX_EXPECT(v && m.audio.playing(v));
    m.mix(2400, l, r);
    NX_EXPECT(l > 9000 && l < 13000 && r > 9000 && r < 13000);  // half volume, equal loudness
    NX_EXPECT(m.audio.playing(v) && fabsf(m.audio.position(v) - 0.05f) < 0.005f);
    NX_EXPECT(m.audio.level() > 0.2f);
    m.mix(4800, l, r);
    NX_EXPECT(!m.audio.playing(v) && m.finished == 1 && m.audio.voicesInUse() == 0);
    m.mix(4800, l, r);
    NX_EXPECT(l == 0 && r == 0);
    // A name that has ended does nothing, even once its voice is in use again.
    PlayOptions loop;
    loop.loop = true;
    const Voice again = m.audio.play(*tone, &owner, loop);
    m.audio.stop(v, 0);
    for (int i = 0; i < 10; i++)
    {
        m.mix(4800, l, r);
    }
    NX_EXPECT(m.audio.playing(again) && l > 9000);

    m.audio.setPan(again, -1);
    m.mix(4800, l, r);
    m.mix(4800, l, r);
    NX_EXPECT(l > 14000 && r < 100);
    m.audio.setPaused(again, true);
    m.mix(4800, l, r);
    NX_EXPECT(l == 0 && m.audio.playing(again));
    m.audio.setPaused(again, false);
    m.audio.setBusVolume(Bus::Effects, 0);
    m.mix(4800, l, r);
    m.mix(4800, l, r);
    m.mix(4800, l, r);
    NX_EXPECT(l < 10);
    m.audio.setBusVolume(Bus::Effects, 1);
    m.audio.stop(again, 0.05f);
    m.mix(4800, l, r);
    m.mix(4800, l, r);
    NX_EXPECT(!m.audio.playing(again) && l == 0 && m.finished == 2);
    m.audio.shutdown();
    freeSound(tone);
}

NX_TEST(voicesAreLimited)
{
    Mixer m;
    SoundData* tone = load(makeWav(48000, 2, 48000, 220, 0.9f));
    int owner = 0, l = 0, r = 0;
    PlayOptions quiet;
    quiet.priority = 10;
    for (u32 i = 0; i < Audio::kVoices; i++)
    {
        NX_EXPECT(static_cast<bool>(m.audio.play(*tone, &owner, quiet)));
    }
    m.mix(4800, l, r);
    NX_EXPECT(l > 24000);  // loud, but bent rather than wrapped round
    quiet.priority = 5;
    NX_EXPECT(!m.audio.play(*tone, &owner, quiet));
    m.mix(4800, l, r);
    NX_EXPECT(m.audio.voicesInUse() == Audio::kVoices);  // too unimportant to take one
    NX_EXPECT(!m.audio.play(*tone, &owner, {}));
    m.mix(4800, l, r);
    NX_EXPECT(m.audio.voicesInUse() == Audio::kVoices - 1);  // made way
    NX_EXPECT(static_cast<bool>(m.audio.play(*tone, &owner, {})));

    // More commands than fit are refused, not written past the end.
    m.mix(4800, l, r);
    for (int i = 0; i < 2000; i++)
    {
        m.audio.setMasterVolume(static_cast<float>(i % 2));
    }
    m.mix(4800, l, r);

    // Nonsense from a script.
    const float nan = sqrtf(-1.0f);
    const float nonsense[] = { nan, -5.0f, 1e30f };
    for (const float bad : nonsense)
    {
        PlayOptions o;
        o.volume = o.pan = o.pitch = o.fadeIn = bad;
        m.audio.play(*tone, &owner, o);
        m.mix(4800, l, r);
    }

    // Closing a screen ends its voices, paused or not.
    m.audio.pauseGroup(0, true);
    m.audio.stopGroup(0, 0.01f);
    m.mix(4800, l, r);
    m.mix(4800, l, r);
    NX_EXPECT(m.audio.voicesInUse() == 0);
    m.audio.shutdown();
    freeSound(tone);
}

NX_TEST(adpcmIsCloseToTheOriginal)
{
    for (u32 channels = 1; channels <= 2; channels++)
    {
        constexpr u32 frames = 5000;
        i16* samples = static_cast<i16*>(memAlloc(frames * channels * 2, MemTag::Audio));
        for (u32 i = 0; i < frames * channels; i++)
        {
            samples[i] = static_cast<i16>(sinf(static_cast<float>(i / channels) * 0.05f * static_cast<float>(1 + i % channels)) * 12000);
        }
        const std::string file = makeAdpcm(samples, frames, channels, 22050);
        NX_EXPECT(file.size() < frames * channels * 2 / 3);
        SoundData* sound = load(file);
        NX_EXPECT(sound && sound->kind == SoundKind::Adpcm && sound->frames == frames && sound->channels == channels);
        u32 got = 0;
        i16* back = sound ? decodeAll(*sound, got) : nullptr;
        NX_EXPECT(back && got == frames);
        double error = 0;
        for (u32 i = 0; back && i < frames * channels; i++)
        {
            error += fabs(static_cast<double>(back[i]) - samples[i]);
        }
        NX_EXPECT(error / (frames * channels) < 150);
        memFree(back);
        freeSound(sound);
        memFree(samples);
    }
}

NX_TEST(compressedSoundsLoopWithoutAllocating)
{
    Mixer m;
    i16 samples[2000];
    for (int i = 0; i < 2000; i++)
    {
        samples[i] = static_cast<i16>(sinf(static_cast<float>(i) * 0.06f) * 15000);
    }
    SoundData* sound = load(makeAdpcm(samples, 2000, 1, 48000));
    int owner = 0, l = 0, r = 0;
    PlayOptions o;
    o.loop = true;
    const Voice v = m.audio.play(*sound, &owner, o);
    const size_t before = memStatsTotal().totalCount;
    for (int i = 0; i < 20; i++)
    {
        m.mix(4800, l, r);
        NX_EXPECT(l > 5000 && m.audio.playing(v) && m.audio.position(v) < 0.05f);
    }
    NX_EXPECT(memStatsTotal().totalCount == before);
    m.audio.shutdown();
    NX_EXPECT(m.finished == 1);
    freeSound(sound);
}

NX_TEST(spectrumFindsTheTone)
{
    Mixer m;
    SoundData* low = load(makeWav(48000, 1, 48000, 120, 0.8f));
    SoundData* high = load(makeWav(48000, 1, 48000, 6000, 0.8f));
    int l = 0, r = 0;
    u32 bands[2] = {};
    SoundData* tones[] = { low, high };
    for (u32 t = 0; t < 2; t++)
    {
        int owner = 0;
        const Voice v = m.audio.play(*tones[t], &owner, {});
        m.audio.spectrum(0);
        for (int i = 0; i < 4; i++)
        {
            m.mix(2400, l, r);
        }
        for (u32 b = 0; b < 32; b++)
        {
            bands[t] = m.audio.spectrum(b) > m.audio.spectrum(bands[t]) ? b : bands[t];
        }
        m.audio.stop(v, 0);
        for (int i = 0; i < 60; i++)
        {
            m.audio.spectrum(0);
            m.mix(2400, l, r);
        }
    }
    NX_EXPECT(bands[0] < 8 && bands[1] > 20);
    NX_EXPECT(m.audio.spectrum(32) == 0 && m.audio.wave(512) == 0);
    m.audio.shutdown();
    freeSound(low);
    freeSound(high);
}

NX_TEST(busEffectsChangeWhatIsHeard)
{
    Mixer m;
    SoundData* low = load(makeWav(48000, 1, 48000, 150, 0.5f));
    SoundData* high = load(makeWav(48000, 1, 48000, 6000, 0.5f));
    SoundData* click = load(makeWav(2400, 1, 48000, 1000, 0.5f));   // a twentieth of a second
    int l = 0, r = 0;
    int owner = 0;
    // How loud a tone is once it has settled, with whatever is on the bus.
    const auto loudness = [&](const SoundData& tone)
    {
        const Voice v = m.audio.play(tone, &owner, {});
        m.mix(4800, l, r);
        m.mix(2400, l, r);
        const int heard = l;
        m.audio.stop(v, 0);
        for (int i = 0; i < 10; i++)
        {
            m.mix(4800, l, r);
        }
        return heard;
    };
    const int plainHigh = loudness(*high), plainLow = loudness(*low);
    NX_EXPECT(plainHigh > 8000 && plainLow > 8000);

    BusEffectSettings muffled;
    muffled.lowPass = 500;
    m.audio.setEffects(Bus::Effects, muffled, 0);
    NX_EXPECT(loudness(*high) < plainHigh / 20);           // gone
    NX_EXPECT(loudness(*low) > plainLow * 8 / 10);         // still there

    BusEffectSettings thin;
    thin.highPass = 2000;
    m.audio.setEffects(Bus::Effects, thin, 0);
    NX_EXPECT(loudness(*low) < plainLow / 20);
    NX_EXPECT(loudness(*high) > plainHigh * 8 / 10);

    // Only the bus that has them: music goes through as it was.
    PlayOptions music;
    music.bus = Bus::Music;
    const Voice tune = m.audio.play(*low, &owner, music);
    m.mix(4800, l, r);
    m.mix(2400, l, r);
    NX_EXPECT(l > plainLow * 8 / 10);
    m.audio.stop(tune, 0);
    m.mix(4800, l, r);

    // An echo: a short sound, heard again a quarter of a second later, when it has stopped.
    BusEffectSettings echo;
    echo.echo = 0.6f;
    echo.echoDelay = 0.25f;
    echo.echoFeedback = 0;
    m.audio.setEffects(Bus::Effects, echo, 0);
    m.audio.play(*click, &owner, {});
    m.mix(4800, l, r);                     // the click, and silence after it
    int quiet = 0;
    m.mix(4800, quiet, r);
    m.mix(4800, l, r);                     // 0.2 to 0.3 seconds: the echo
    NX_EXPECT(quiet < 100 && l > 3000);

    // A room: what is heard goes on after the sound has stopped, and with none, it doesn't.
    const auto tail = [&]()
    {
        m.audio.play(*click, &owner, {});
        m.mix(4800, l, r);
        m.mix(4800, l, r);
        const int after = l;
        for (int i = 0; i < 40; i++)
        {
            m.mix(4800, l, r);
        }
        return after;
    };
    BusEffectSettings hall;
    hall.reverb = 1;
    hall.room = 0.9f;
    m.audio.setEffects(Bus::Effects, hall, 0);
    NX_EXPECT(tail() > 300);
    m.audio.clearEffects(0);
    NX_EXPECT(tail() == 0);

    m.audio.shutdown();
    freeSound(low);
    freeSound(high);
    freeSound(click);
}

NX_TEST(damagedSoundsDontCrash)
{
    i16 samples[1200] = {};
    const std::string good[] = { makeWav(1000, 2, 22050, 300, 0.5f), makeAdpcm(samples, 1200, 1, 22050) };
    u32 random = 12345;
    const auto next = [&] { random ^= random << 13; random ^= random >> 17; random ^= random << 5; return random; };
    for (const std::string& file : good)
    {
        for (u32 i = 0; i < 400; i++)
        {
            std::string bad = file;
            if (i % 2)
            {
                bad.resize(next() % bad.size());
            }
            for (u32 n = 0; n < 1 + i % 8 && !bad.empty(); n++)
            {
                bad[next() % (i % 3 && bad.size() > 48 ? 48 : bad.size())] = static_cast<char>(next());
            }
            SoundData* sound = load(bad);
            if (sound)
            {
                NX_EXPECT(sound->channels == 1 || sound->channels == 2);
                Mixer m;
                int l = 0, r = 0, owner = 0;
                m.audio.play(*sound, &owner, {});
                m.mix(1000, l, r);
                m.audio.shutdown();
                freeSound(sound);
            }
        }
    }
    NX_EXPECT(load("") == nullptr && load("OggS") == nullptr && load("ID3") == nullptr);
}

NX_TEST(jobsRunInOrder)
{
    struct State
    {
        std::atomic<int> done{ 0 };
        int order[100];
    } state;
    struct Job
    {
        State* state;
        int number;
    } jobs[100];
    JobQueue queue;
    NX_EXPECT(queue.start());
    const ThreadFunction run = [](void* job)
    {
        Job& j = *static_cast<Job*>(job);
        j.state->order[j.state->done.load()] = j.number;
        j.state->done++;
    };
    for (int i = 0; i < 100; i++)
    {
        jobs[i] = { &state, i };
        while (!queue.add(run, &jobs[i]))
        {
            sleepMilliseconds(1);
        }
    }
    queue.stop();
    NX_EXPECT(state.done.load() == 100);
    for (int i = 0; i < 100; i++)
    {
        NX_EXPECT(state.order[i] == i);
    }
}

NX_TEST(mixingRunsOnItsOwnThread)
{
    i16 samples[3000] = {};
    SoundData* sounds[] = { load(makeWav(4000, 1, 22050, 300, 0.5f)), load(makeAdpcm(samples, 3000, 1, 44100)) };
    Audio audio;
    u32 finished = 0, started = 0;
    NX_EXPECT(audio.init(createNullAudioOutput(true)));
    audio.onFinished([](void*, void* count) { ++*static_cast<u32*>(count); }, &finished);
    u32 random = 99;
    Voice voices[8];
    for (u32 i = 0; i < 300; i++)
    {
        random = random * 1664525 + 1013904223;
        Voice& v = voices[(random >> 8) % 8];
        PlayOptions o;
        o.loop = (random >> 12) % 4 == 0;
        audio.setGroup((random >> 16) % 3);
        switch ((random >> 20) % 7)
        {
        case 0: audio.stop(v, 0.01f); break;
        case 1: audio.setVolume(v, 0.3f, 0.02f); break;
        case 2: audio.setPitch(v, 1.5f); break;
        case 3: audio.pauseGroup(1, (random >> 24) % 2 != 0); break;
        case 4: audio.stopGroup(2, 0.02f); break;
        default:
            v = audio.play(*sounds[(random >> 10) % 2], &finished, o);
            started += v ? 1 : 0;
            break;
        }
        audio.spectrum(0);
        audio.update();
        sleepMilliseconds(1);
    }
    audio.shutdown();
    NX_EXPECT(started > 50 && finished == started);
    freeSound(sounds[0]);
    freeSound(sounds[1]);
}
