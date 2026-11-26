#include "audio/audio.h"
#include "audio/decoder.h"
#include "base/log.h"
#include "base/memory.h"
#include "base/thread.h"

#include <math.h>
#include <string.h>

namespace nx
{
    namespace
    {
        constexpr u32 kChunk = 256;        // frames mixed at a time
        constexpr u32 kSourceChunk = 1024;  // frames decoded at a time
        constexpr float kPi = 3.14159265f;

        float clampFloat(float v, float low, float high)
        {
            // Not a number counts as low.
            return !(v >= low) ? low : v > high ? high : v;
        }
    }

    enum class Verb : u8 { Play, Stop, Volume, Pan, Pitch, Pause, BusVolume, Master, Effects, GiveReverb, GiveEcho };

    struct Audio::Command
    {
        Verb verb;
        u8 voice;
        u32 generation;
        const SoundData* sound;
        SoundDecoder* decoder;
        PlayOptions options;
        float value;
        float seconds;
        BusEffectSettings effects;
        void* memory;
    };

    struct Audio::Channel
    {
        bool active = false;
        bool paused = false;
        bool stopping = false;
        bool ended = false;       // the source has no more; play out what is buffered
        u32 generation = 0;
        const SoundData* sound = nullptr;
        SoundDecoder* decoder = nullptr;
        PlayOptions options;
        float volume = 0, volumeGoal = 0, volumeStep = 0;
        float left = 0, right = 0;  // as last mixed
        double position = 0;        // frames into samples
        u64 played = 0;             // source frames, for position()
        u32 available = 0;          // frames in samples
        i16 samples[(kSourceChunk + 2) * 2];
    };

    bool Audio::init(AudioOutput* output)
    {
        m_output = output;
        m_channels = static_cast<Channel*>(memAlloc(sizeof(Channel) * kVoices, MemTag::Audio));
        m_commands = static_cast<Command*>(memAlloc(sizeof(Command) * kCommands, MemTag::Audio));
        m_history = static_cast<std::atomic<i16>*>(memAlloc(sizeof(std::atomic<i16>) * kHistory, MemTag::Audio));
        m_busMix = static_cast<float*>(memAlloc(sizeof(float) * kChunk * 2 * static_cast<size_t>(Bus::Count), MemTag::Audio));
        if (m_channels == nullptr || m_commands == nullptr || m_history == nullptr || m_busMix == nullptr)
        {
            return false;
        }
        for (u32 i = 0; i < kVoices; i++)
        {
            new (&m_channels[i]) Channel();
        }
        for (u32 i = 0; i < kHistory; i++)
        {
            new (&m_history[i]) std::atomic<i16>(static_cast<i16>(0));
        }
        if (m_output && !m_output->start([](i16* stereo, u32 frames, void* self) { static_cast<Audio*>(self)->mix(stereo, frames); }, this))
        {
            // No sound is not a reason to stop: everything else still works.
            NX_WARN("no sound output");
            destroy(m_output);
            m_output = createNullAudioOutput(true);
            return m_output && m_output->start([](i16* stereo, u32 frames, void* self) { static_cast<Audio*>(self)->mix(stereo, frames); }, this);
        }
        return true;
    }

    void Audio::shutdown()
    {
        if (m_output)
        {
            m_output->stop();
            destroy(m_output);
            m_output = nullptr;
        }
        if (m_channels)
        {
            // The mixing thread has gone, so this one may finish its work.
            i16 nothing[2];
            mix(nothing, 1);
            for (u32 i = 0; i < kVoices; i++)
            {
                closeDecoder(m_channels[i].decoder);
                m_channels[i].decoder = nullptr;
                m_channels[i].active = false;
                if (m_slots[i].used)
                {
                    m_slots[i].finished.store(m_slots[i].generation);
                }
            }
            update();
        }
        memFree(m_channels);
        memFree(m_commands);
        memFree(m_history);
        memFree(m_busMix);
        m_channels = nullptr;
        m_commands = nullptr;
        m_history = nullptr;
        m_busMix = nullptr;
        for (u32 b = 0; b < static_cast<u32>(Bus::Count); b++)
        {
            m_busEffects[b] = BusEffects();
            m_effectSettings[b] = BusEffectSettings();
            destroyReverbMemory(m_reverbMemory[b]);
            destroyEchoMemory(m_echoMemory[b]);
            m_reverbMemory[b] = nullptr;
            m_echoMemory[b] = nullptr;
        }
    }

    void Audio::onFinished(Finished callback, void* user)
    {
        m_finished = callback;
        m_finishedUser = user;
    }

    // --- The game thread ---

    Audio::Slot* Audio::slotOf(Voice voice)
    {
        const u32 index = voice.value & 0xFF;
        return index < kVoices && m_slots[index].used && m_slots[index].generation == voice.value >> 8 ? &m_slots[index] : nullptr;
    }

    const Audio::Slot* Audio::slotOf(Voice voice) const
    {
        return const_cast<Audio*>(this)->slotOf(voice);
    }

    bool Audio::send(const Command& command)
    {
        const u32 written = m_commandsWritten.load(std::memory_order_relaxed);
        if (m_commands == nullptr || written - m_commandsRead.load(std::memory_order_acquire) >= kCommands)
        {
            return false;
        }
        m_commands[written % kCommands] = command;
        m_commandsWritten.store(written + 1, std::memory_order_release);
        return true;
    }

    void Audio::update()
    {
        m_clock++;
        for (Slot& slot : m_slots)
        {
            if (slot.used && slot.finished.load(std::memory_order_acquire) == slot.generation)
            {
                void* owner = slot.owner;
                closeDecoder(slot.retired.exchange(nullptr, std::memory_order_relaxed));
                slot.used = false;
                slot.owner = nullptr;
                slot.generation = slot.generation >= 0xFFFFFF ? 1 : slot.generation + 1;
                if (m_finished && owner)
                {
                    m_finished(owner, m_finishedUser);
                }
            }
        }
        if (m_history && m_analyse)
        {
            m_analyse--;
            analyse();
        }
    }

    float Audio::spectrum(u32 band)
    {
        m_analyse = 60;
        return band < 32 ? m_bands[band] : 0;
    }

    float Audio::wave(u32 index)
    {
        m_analyse = 60;
        return index < 512 ? m_wave[index] : 0;
    }

    Voice Audio::play(const SoundData& sound, void* owner, const PlayOptions& options)
    {
        Slot* chosen = nullptr;
        u32 index = 0;
        for (u32 i = 0; i < kVoices && chosen == nullptr; i++)
        {
            if (!m_slots[i].used)
            {
                chosen = &m_slots[i];
                index = i;
            }
        }
        if (chosen == nullptr)
        {
            // Ask the least important to make way, for next time. Its voice isn't free until the
            // mixing thread says so, which keeps this call from ever waiting.
            Slot* least = nullptr;
            for (u32 i = 0; i < kVoices; i++)
            {
                Slot& s = m_slots[i];
                if (least == nullptr || s.priority < least->priority || (s.priority == least->priority && s.started < least->started))
                {
                    least = &s;
                    index = i;
                }
            }
            if (least->priority <= options.priority)
            {
                stop({ (least->generation << 8) | index }, 0.01f);
            }
            m_refused++;
            return {};
        }

        Command command = {};
        command.verb = Verb::Play;
        command.voice = static_cast<u8>(index);
        command.generation = chosen->generation;
        command.sound = &sound;
        command.options = options;
        command.options.volume = clampFloat(options.volume, 0, 4);
        command.options.pan = clampFloat(options.pan, -1, 1);
        command.options.pitch = clampFloat(options.pitch, 0.05f, 8);
        if (sound.kind != SoundKind::Pcm)
        {
            // Opened here, because it allocates.
            command.decoder = openDecoder(sound);
            if (command.decoder == nullptr)
            {
                return {};
            }
        }
        if (!send(command))
        {
            closeDecoder(command.decoder);
            return {};
        }
        chosen->used = true;
        chosen->priority = options.priority;
        chosen->group = m_group;
        chosen->owner = owner;
        chosen->started = static_cast<double>(m_clock);
        chosen->positionMs.store(0);
        return { (chosen->generation << 8) | index };
    }

    void Audio::stop(Voice voice, float fadeOut)
    {
        if (slotOf(voice))
        {
            Command command = {};
            command.verb = Verb::Stop;
            command.voice = static_cast<u8>(voice.value & 0xFF);
            command.generation = voice.value >> 8;
            command.seconds = fadeOut;
            send(command);
        }
    }

    void Audio::setVolume(Voice voice, float volume, float seconds)
    {
        if (slotOf(voice))
        {
            Command command = {};
            command.verb = Verb::Volume;
            command.voice = static_cast<u8>(voice.value & 0xFF);
            command.generation = voice.value >> 8;
            command.value = clampFloat(volume, 0, 4);
            command.seconds = seconds;
            send(command);
        }
    }

    void Audio::setPan(Voice voice, float pan)
    {
        if (slotOf(voice))
        {
            Command command = {};
            command.verb = Verb::Pan;
            command.voice = static_cast<u8>(voice.value & 0xFF);
            command.generation = voice.value >> 8;
            command.value = clampFloat(pan, -1, 1);
            send(command);
        }
    }

    void Audio::setPitch(Voice voice, float pitch)
    {
        if (slotOf(voice))
        {
            Command command = {};
            command.verb = Verb::Pitch;
            command.voice = static_cast<u8>(voice.value & 0xFF);
            command.generation = voice.value >> 8;
            command.value = clampFloat(pitch, 0.05f, 8);
            send(command);
        }
    }

    void Audio::setPaused(Voice voice, bool paused)
    {
        if (slotOf(voice))
        {
            Command command = {};
            command.verb = Verb::Pause;
            command.voice = static_cast<u8>(voice.value & 0xFF);
            command.generation = voice.value >> 8;
            command.value = paused ? 1.0f : 0.0f;
            send(command);
        }
    }

    bool Audio::playing(Voice voice) const
    {
        return slotOf(voice) != nullptr;
    }

    float Audio::position(Voice voice) const
    {
        const Slot* slot = slotOf(voice);
        return slot ? static_cast<float>(slot->positionMs.load(std::memory_order_relaxed)) / 1000.0f : 0;
    }

    void Audio::setEffects(Bus bus, const BusEffectSettings& settings, float seconds)
    {
        const size_t b = static_cast<size_t>(bus);
        if (b >= static_cast<size_t>(Bus::Count) || m_channels == nullptr)
        {
            return;
        }
        // Memory for a room or an echo, the first time a bus wants one: made here, so that mixing never makes any.
        if (settings.reverb > 0 && m_reverbMemory[b] == nullptr && (m_reverbMemory[b] = createReverbMemory()) != nullptr)
        {
            Command give = {};
            give.verb = Verb::GiveReverb;
            give.voice = static_cast<u8>(b);
            give.memory = m_reverbMemory[b];
            send(give);
        }
        if (settings.echo > 0 && m_echoMemory[b] == nullptr && (m_echoMemory[b] = createEchoMemory()) != nullptr)
        {
            Command give = {};
            give.verb = Verb::GiveEcho;
            give.voice = static_cast<u8>(b);
            give.memory = m_echoMemory[b];
            send(give);
        }
        m_effectSettings[b] = settings;
        Command command = {};
        command.verb = Verb::Effects;
        command.voice = static_cast<u8>(b);
        command.effects = settings;
        command.seconds = seconds;
        send(command);
    }

    void Audio::clearEffects(float seconds)
    {
        for (u32 b = 0; b < static_cast<u32>(Bus::Count); b++)
        {
            setEffects(static_cast<Bus>(b), BusEffectSettings(), seconds);
        }
    }

    void Audio::setBusVolume(Bus bus, float volume)
    {
        m_busVolume[static_cast<size_t>(bus)] = clampFloat(volume, 0, 1);
        Command command = {};
        command.verb = Verb::BusVolume;
        command.voice = static_cast<u8>(bus);
        command.value = m_busVolume[static_cast<size_t>(bus)];
        send(command);
    }

    void Audio::setMasterVolume(float volume)
    {
        m_master = clampFloat(volume, 0, 1);
        Command command = {};
        command.verb = Verb::Master;
        command.value = m_master;
        send(command);
    }

    void Audio::stopGroup(u32 group, float fadeOut)
    {
        for (u32 i = 0; i < kVoices; i++)
        {
            if (m_slots[i].used && m_slots[i].group == group)
            {
                stop({ (m_slots[i].generation << 8) | i }, fadeOut);
            }
        }
    }

    void Audio::pauseGroup(u32 group, bool paused)
    {
        for (u32 i = 0; i < kVoices; i++)
        {
            if (m_slots[i].used && m_slots[i].group == group)
            {
                setPaused({ (m_slots[i].generation << 8) | i }, paused);
            }
        }
    }

    u32 Audio::voicesInUse() const
    {
        u32 count = 0;
        for (const Slot& slot : m_slots)
        {
            count += slot.used;
        }
        return count;
    }

    // 1024-point FFT of what was heard last, into 32 bands that get wider as they go up.
    void Audio::analyse()
    {
        constexpr u32 n = 1024;
        float re[n], im[n];
        const u32 at = m_historyAt.load(std::memory_order_acquire);
        for (u32 i = 0; i < n; i++)
        {
            const float sample = static_cast<float>(m_history[(at - n + i) % kHistory].load(std::memory_order_relaxed)) / 32768.0f;
            if (i >= n - 512)
            {
                m_wave[i - (n - 512)] = sample;
            }
            re[i] = sample * (0.5f - 0.5f * cosf(2 * kPi * static_cast<float>(i) / n));
            im[i] = 0;
        }
        for (u32 i = 1, j = 0; i < n; i++)
        {
            u32 bit = n >> 1;
            for (; j & bit; bit >>= 1)
            {
                j ^= bit;
            }
            j ^= bit;
            if (i < j)
            {
                const float t = re[i];
                re[i] = re[j];
                re[j] = t;
            }
        }
        for (u32 length = 2; length <= n; length <<= 1)
        {
            const float angle = -2 * kPi / static_cast<float>(length);
            const float wr = cosf(angle), wi = sinf(angle);
            for (u32 i = 0; i < n; i += length)
            {
                float cr = 1, ci = 0;
                for (u32 k = 0; k < length / 2; k++)
                {
                    const u32 a = i + k, b = i + k + length / 2;
                    const float tr = re[b] * cr - im[b] * ci;
                    const float ti = re[b] * ci + im[b] * cr;
                    re[b] = re[a] - tr;
                    im[b] = im[a] - ti;
                    re[a] += tr;
                    im[a] += ti;
                    const float next = cr * wr - ci * wi;
                    ci = cr * wi + ci * wr;
                    cr = next;
                }
            }
        }
        for (u32 band = 0; band < 32; band++)
        {
            const u32 from = 1 + static_cast<u32>(powf(400.0f, static_cast<float>(band) / 32.0f)) - 1 + band / 4;
            u32 to = 1 + static_cast<u32>(powf(400.0f, static_cast<float>(band + 1) / 32.0f)) - 1 + (band + 1) / 4;
            to = to <= from ? from + 1 : to;
            float most = 0;
            for (u32 i = from; i < to && i < n / 2; i++)
            {
                const float size = sqrtf(re[i] * re[i] + im[i] * im[i]) / (n / 4);
                most = size > most ? size : most;
            }
            const float decibels = 20 * log10f(most + 1e-6f);
            const float now = clampFloat((decibels + 66) / 60, 0, 1);
            // Up at once, down gently.
            m_bands[band] = now > m_bands[band] ? now : m_bands[band] * 0.88f + now * 0.12f;
        }
    }

    // --- The mixing thread ---

    void Audio::run(const Command& c)
    {
        if (c.verb == Verb::BusVolume)
        {
            m_busGoal[c.voice] = c.value;
            return;
        }
        if (c.verb == Verb::Master)
        {
            m_masterGoal = c.value;
            return;
        }
        if (c.verb == Verb::Effects)
        {
            m_busEffects[c.voice].set(c.effects, c.seconds);
            return;
        }
        if (c.verb == Verb::GiveReverb)
        {
            m_busEffects[c.voice].giveReverb(static_cast<ReverbMemory*>(c.memory));
            return;
        }
        if (c.verb == Verb::GiveEcho)
        {
            m_busEffects[c.voice].giveEcho(static_cast<EchoMemory*>(c.memory));
            return;
        }
        Channel& channel = m_channels[c.voice];
        if (c.verb == Verb::Play)
        {
            channel.active = true;
            channel.paused = channel.stopping = channel.ended = false;
            channel.generation = c.generation;
            channel.sound = c.sound;
            channel.decoder = c.decoder;
            channel.options = c.options;
            channel.volume = c.options.fadeIn > 0 ? 0 : c.options.volume;
            channel.volumeGoal = c.options.volume;
            channel.volumeStep = c.options.fadeIn > 0 ? c.options.volume / (c.options.fadeIn * kMixRate) : 0;
            channel.left = channel.right = 0;
            channel.position = 0;
            channel.played = 0;
            channel.available = 0;
            return;
        }
        if (!channel.active || channel.generation != c.generation)
        {
            return;
        }
        switch (c.verb)
        {
        case Verb::Stop:
            channel.stopping = true;
            channel.paused = false;
            channel.volumeGoal = 0;
            channel.volumeStep = (channel.volume + 0.001f) / ((c.seconds > 0.005f ? c.seconds : 0.005f) * kMixRate);
            break;
        case Verb::Volume:
        {
            if (channel.stopping)
            {
                break;
            }
            channel.volumeGoal = c.value;
            const float change = fabsf(c.value - channel.volume);
            channel.volumeStep = change / ((c.seconds > 0.005f ? c.seconds : 0.005f) * kMixRate);
            break;
        }
        case Verb::Pan:
            channel.options.pan = c.value;
            break;
        case Verb::Pitch:
            channel.options.pitch = c.value;
            break;
        case Verb::Pause:
            channel.paused = c.value != 0 && !channel.stopping;
            break;
        default:
            break;
        }
    }

    void Audio::mixChannel(u32 index, float* buffer, u32 frames)
    {
        Channel& ch = m_channels[index];
        const SoundData& sound = *ch.sound;
        const u32 channels = sound.channels;
        const double step = static_cast<double>(sound.rate) / kMixRate * ch.options.pitch;

        const float bus = m_busNow[static_cast<size_t>(ch.options.bus)];
        // Equal loudness across the pan.
        const float angle = (ch.options.pan + 1) * kPi / 4;
        const float panLeft = channels == 1 ? cosf(angle) : clampFloat(1 - ch.options.pan, 0, 1);
        const float panRight = channels == 1 ? sinf(angle) : clampFloat(1 + ch.options.pan, 0, 1);

        for (u32 done = 0; done < frames && ch.active;)
        {
            // Linear interpolation needs the frame after as well.
            while (!ch.ended && ch.position + 1 >= ch.available)
            {
                const u32 used = ch.position < ch.available ? static_cast<u32>(ch.position) : ch.available;
                const u32 keep = ch.available - used;
                if (keep)
                {
                    memmove(ch.samples, ch.samples + static_cast<size_t>(used) * channels, static_cast<size_t>(keep) * channels * sizeof(i16));
                }
                ch.position -= used;
                ch.available = keep;
                u32 got = 0;
                const u32 room = kSourceChunk + 1 - keep;
                i16* to = ch.samples + static_cast<size_t>(keep) * channels;
                if (ch.decoder)
                {
                    got = decode(*ch.decoder, to, room);
                    if (got == 0 && ch.options.loop && rewindDecoder(*ch.decoder))
                    {
                        ch.played = 0;
                        got = decode(*ch.decoder, to, room);
                    }
                }
                else
                {
                    if (ch.played >= sound.frames && ch.options.loop)
                    {
                        ch.played = 0;
                    }
                    const u64 left = sound.frames - ch.played;
                    got = left < room ? static_cast<u32>(left) : room;
                    memcpy(to, reinterpret_cast<const i16*>(sound.data) + static_cast<size_t>(ch.played) * channels, static_cast<size_t>(got) * channels * sizeof(i16));
                }
                ch.played += got;
                ch.available += got;
                if (got == 0)
                {
                    ch.ended = true;
                    // One silent frame to come to rest on.
                    memset(ch.samples + static_cast<size_t>(ch.available) * channels, 0, channels * sizeof(i16));
                }
            }
            if (ch.position >= ch.available)
            {
                ch.active = false;
                break;
            }

            const u32 at = static_cast<u32>(ch.position);
            const float part = static_cast<float>(ch.position - at);
            const i16* a = ch.samples + static_cast<size_t>(at) * channels;
            const i16* b = a + channels;
            const float l = static_cast<float>(a[0]) + (static_cast<float>(b[0]) - static_cast<float>(a[0])) * part;
            const float r = channels == 2 ? static_cast<float>(a[1]) + (static_cast<float>(b[1]) - static_cast<float>(a[1])) * part : l;

            if (ch.volume != ch.volumeGoal)
            {
                ch.volume = ch.volume < ch.volumeGoal ? ch.volume + ch.volumeStep : ch.volume - ch.volumeStep;
                if (fabsf(ch.volume - ch.volumeGoal) <= ch.volumeStep)
                {
                    ch.volume = ch.volumeGoal;
                }
            }
            else if (ch.stopping)
            {
                ch.active = false;
                break;
            }
            // Pan and bus changes glide over a few milliseconds.
            const float volume = ch.volume * bus;
            ch.left += (volume * panLeft - ch.left) * 0.02f;
            ch.right += (volume * panRight - ch.right) * 0.02f;
            buffer[done * 2 + 0] += l * ch.left;
            buffer[done * 2 + 1] += r * ch.right;
            ch.position += step;
            done++;
        }

        const u64 heard = ch.played > ch.available ? ch.played - ch.available + static_cast<u64>(ch.position) : static_cast<u64>(ch.position);
        m_slots[index].positionMs.store(static_cast<u32>(heard * 1000 / (sound.rate ? sound.rate : 1)), std::memory_order_relaxed);
    }

    void Audio::mix(i16* stereo, u32 frames)
    {
        const u32 written = m_commandsWritten.load(std::memory_order_acquire);
        u32 read = m_commandsRead.load(std::memory_order_relaxed);
        for (; read != written; read++)
        {
            run(m_commands[read % kCommands]);
        }
        m_commandsRead.store(read, std::memory_order_release);

        float loudest = 0;
        while (frames)
        {
            const u32 count = frames < kChunk ? frames : kChunk;
            float buffer[kChunk * 2] = {};
            for (u32 b = 0; b < static_cast<u32>(Bus::Count); b++)
            {
                m_busNow[b] += (m_busGoal[b] - m_busNow[b]) * 0.3f;
            }
            m_masterNow += (m_masterGoal - m_masterNow) * 0.3f;

            memset(m_busMix, 0, sizeof(float) * kChunk * 2 * static_cast<size_t>(Bus::Count));
            for (u32 i = 0; i < kVoices; i++)
            {
                Channel& ch = m_channels[i];
                if (ch.active && !ch.paused)
                {
                    mixChannel(i, m_busMix + static_cast<size_t>(ch.options.bus) * kChunk * 2, count);
                }
                if (!ch.active && ch.sound)
                {
                    m_slots[i].retired.store(ch.decoder, std::memory_order_relaxed);
                    ch.decoder = nullptr;
                    ch.sound = nullptr;
                    m_slots[i].finished.store(ch.generation, std::memory_order_release);
                }
            }

            // Each bus through its own effects, then all together.
            for (u32 b = 0; b < static_cast<u32>(Bus::Count); b++)
            {
                float* mixed = m_busMix + static_cast<size_t>(b) * kChunk * 2;
                if (m_busEffects[b].busy())
                {
                    m_busEffects[b].process(mixed, count);
                }
                for (u32 i = 0; i < count * 2; i++)
                {
                    buffer[i] += mixed[i];
                }
            }

            u32 at = m_historyAt.load(std::memory_order_relaxed);
            for (u32 i = 0; i < count; i++)
            {
                for (u32 c = 0; c < 2; c++)
                {
                    float v = buffer[i * 2 + c] * m_masterNow / 32768.0f;
                    // Straight up to three quarters, then bending towards full, so loud mixes don't crackle.
                    const float size = fabsf(v);
                    if (size > 0.75f)
                    {
                        const float bent = 0.75f + 0.25f * (1 - 1 / (1 + (size - 0.75f) * 4));
                        v = v < 0 ? -bent : bent;
                    }
                    loudest = size > loudest ? size : loudest;
                    stereo[i * 2 + c] = static_cast<i16>(v * 32767.0f);
                }
                m_history[at % kHistory].store(static_cast<i16>((stereo[i * 2] + stereo[i * 2 + 1]) / 2), std::memory_order_relaxed);
                at++;
            }
            m_historyAt.store(at, std::memory_order_release);
            stereo += static_cast<size_t>(count) * 2;
            frames -= count;
        }
        const float before = m_level.load(std::memory_order_relaxed);
        m_level.store(loudest > before ? (loudest > 1 ? 1 : loudest) : before * 0.95f, std::memory_order_relaxed);
    }

    // --- The output that plays nothing ---

    namespace
    {
        class NullAudioOutput final : public AudioOutput
        {
        public:
            explicit NullAudioOutput(bool timed) : m_timed(timed) {}

            bool start(Mix mix, void* user) override
            {
                if (m_timed)
                {
                    m_mix = mix;
                    m_user = user;
                    m_thread = startThread([](void* self) { static_cast<NullAudioOutput*>(self)->run(); }, this, ThreadPriority::Normal);
                }
                return true;
            }

            void stop() override
            {
                m_stopping.store(true);
                if (m_thread)
                {
                    joinThread(m_thread);
                    m_thread = {};
                }
            }

            const char* name() const override { return "no sound"; }

        private:
            void run()
            {
                i16 unheard[480 * 2];
                while (!m_stopping.load())
                {
                    m_mix(unheard, 480, m_user);
                    sleepMilliseconds(10);
                }
            }

            bool m_timed;
            Mix m_mix = nullptr;
            void* m_user = nullptr;
            ThreadHandle m_thread;
            std::atomic<bool> m_stopping{ false };
        };
    }

    AudioOutput* createNullAudioOutput(bool timed)
    {
        return create<NullAudioOutput>(MemTag::Audio, timed);
    }
}
