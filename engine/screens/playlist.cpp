#include "screens/playlist.h"

namespace nx
{
    namespace
    {
        Playlist* g_playlists = nullptr;
    }

    Playlist::Playlist(Assets& assets, Audio& audio) : m_assets(assets), m_audio(audio)
    {
        m_group = audio.group();
        m_next = g_playlists;
        if (g_playlists)
        {
            g_playlists->m_previous = this;
        }
        g_playlists = this;
        m_random ^= static_cast<u32>(reinterpret_cast<uintptr_t>(this));
    }

    Playlist::~Playlist()
    {
        (m_previous ? m_previous->m_next : g_playlists) = m_next;
        if (m_next)
        {
            m_next->m_previous = m_previous;
        }
        stop(1);
        clear();
    }

    void Playlist::release()
    {
        if (--m_refs == 0)
        {
            destroy(this);
        }
    }

    void Playlist::add(Asset* sound)
    {
        if (sound && m_count < kTracks)
        {
            m_assets.addRef(sound);
            m_tracks[m_count++] = sound;
        }
    }

    void Playlist::clear()
    {
        stop(0.5f);
        for (u32 i = 0; i < m_count; i++)
        {
            m_assets.release(m_tracks[i]);
            m_tracks[i] = nullptr;
        }
        m_count = m_index = 0;
    }

    void Playlist::setVolume(float volume)
    {
        m_volume = volume < 0 ? 0 : volume;
        if (m_voice && !m_handedOver)
        {
            m_audio.setVolume(m_voice, m_volume, 0.2f);
        }
    }

    void Playlist::setPaused(bool paused)
    {
        m_paused = paused;
        if (m_voice)
        {
            m_audio.setPaused(m_voice, paused);
        }
    }

    float Playlist::position() const
    {
        return m_voice ? m_audio.position(m_voice) : 0;
    }

    float Playlist::seconds() const
    {
        const SoundData* d = m_count ? m_tracks[m_index]->sound : nullptr;
        return d && d->rate ? static_cast<float>(d->frames) / static_cast<float>(d->rate) : 0;
    }

    void Playlist::play(float fade)
    {
        if (m_count && !m_playing)
        {
            start(m_index < m_count ? m_index : 0, fade);
        }
    }

    void Playlist::next(float fade)
    {
        const u32 to = after(m_index);
        if (to < kTracks)
        {
            start(to, fade < 0 ? crossfade : fade);
        }
        else
        {
            stop(fade < 0 ? crossfade : fade);
        }
    }

    void Playlist::previous(float fade)
    {
        if (m_count)
        {
            start((m_index + m_count - 1) % m_count, fade < 0 ? crossfade : fade);
        }
    }

    void Playlist::jump(u32 index, float fade)
    {
        if (index < m_count)
        {
            start(index, fade < 0 ? crossfade : fade);
        }
    }

    void Playlist::stop(float fade)
    {
        if (m_voice)
        {
            m_audio.stop(m_voice, fade > 0 ? fade : 0);
        }
        m_voice = {};
        m_playing = false;
        m_waiting = kTracks;
    }

    u32 Playlist::after(u32 index)
    {
        if (m_count == 0)
        {
            return kTracks;
        }
        if (shuffle && m_count > 1)
        {
            // Any but the one just played.
            m_random = m_random * 1664525u + 1013904223u;
            const u32 pick = (m_random >> 8) % (m_count - 1);
            return pick >= index ? pick + 1 : pick;
        }
        if (index + 1 < m_count)
        {
            return index + 1;
        }
        return loop ? 0 : kTracks;
    }

    void Playlist::start(u32 index, float fade)
    {
        Asset* track = m_tracks[index];
        m_index = index;
        m_playing = true;
        if (track->failed)
        {
            // Past what can't be played.
            m_waiting = kTracks;
            if (m_voice)
            {
                m_audio.stop(m_voice, fade);
                m_voice = {};
            }
            m_handedOver = true;
            return;
        }
        if (track->sound == nullptr)
        {
            m_waiting = index;
            m_waitingFade = fade;
            return;
        }
        m_waiting = kTracks;
        if (m_voice)
        {
            m_audio.stop(m_voice, fade > 0 ? fade : 0.05f);
        }
        PlayOptions o;
        o.volume = m_volume;
        o.fadeIn = fade;
        o.bus = Bus::Music;
        o.priority = 255;
        // The voice holds the sound until it has finished with it.
        m_assets.addRef(track);
        m_voice = m_audio.play(*track->sound, track, o);
        if (!m_voice)
        {
            m_assets.release(track);
        }
        m_handedOver = false;
        m_paused = false;
    }

    void Playlist::update(float)
    {
        if (!m_playing || m_count == 0 || m_paused)
        {
            return;
        }
        if (m_waiting < m_count)
        {
            if (m_tracks[m_waiting]->sound || m_tracks[m_waiting]->failed)
            {
                start(m_waiting, m_waitingFade);
            }
            return;
        }
        const Asset* track = m_tracks[m_index];
        const SoundData* d = track->sound;
        const float length = d && d->rate ? static_cast<float>(d->frames) / static_cast<float>(d->rate) : 0;
        const bool ended = m_handedOver || !m_voice || !m_audio.playing(m_voice);
        // The next comes in before this one ends, if there is time for both to be heard.
        const float overlap = length > crossfade * 3 ? crossfade : 0;
        const bool due = !ended && length > 0 && m_audio.position(m_voice) >= length - overlap - 0.05f;
        if (ended || due)
        {
            const u32 to = after(m_index);
            if (to < kTracks)
            {
                start(to, due ? overlap : 0.3f);
            }
            else if (ended)
            {
                m_playing = false;
                m_voice = {};
            }
        }
    }

    void Playlist::updateGroup(u32 group, float dt)
    {
        for (Playlist* p = g_playlists; p; p = p->m_next)
        {
            if (p->m_group == group)
            {
                p->update(dt);
            }
        }
    }
}
