#pragma once

#include "assets/assets.h"
#include "audio/audio.h"

namespace nx
{
    // Music, one track after another, the next fading in as the last fades out: in order or shuffled, round again or
    // not. Like a film, it belongs to the screen whose sounds it was made among, and moves on only while that screen is
    // on top.
    class Playlist : NonCopyable
    {
    public:
        Playlist(Assets& assets, Audio& audio);
        ~Playlist();

        // Holds the sound.
        void add(Asset* sound);
        void clear();
        // From the track it is at, fading in over so many seconds.
        void play(float fade);
        void next(float fade);
        void previous(float fade);
        void stop(float fade);
        // Straight to a track.
        void jump(u32 index, float fade);
        void update(float dt);

        u32 count() const { return m_count; }
        u32 index() const { return m_index; }
        bool playing() const { return m_playing; }
        bool shuffle = false;
        bool loop = true;
        float crossfade = 2;     // seconds each track overlaps the next
        float volume() const { return m_volume; }
        void setVolume(float volume);
        bool paused() const { return m_paused; }
        void setPaused(bool paused);
        // Of the track playing: seconds in, and long.
        float position() const;
        float seconds() const;

        // Every playlist made while a screen's sounds were set to a group: once a frame, for the screen on top.
        static void updateGroup(u32 group, float dt);

        void addRef() { m_refs++; }
        void release();

    private:
        static constexpr u32 kTracks = 64;

        // Starts one, fading in, and what was playing out.
        void start(u32 index, float fade);
        // Which comes after this one, or kTracks for none.
        u32 after(u32 index);

        Assets& m_assets;
        Audio& m_audio;
        Asset* m_tracks[kTracks] = {};
        u32 m_count = 0, m_index = 0, m_group = 0;
        u32 m_waiting = kTracks;   // to start once it has arrived
        float m_waitingFade = 0;
        Voice m_voice;
        float m_volume = 1;
        bool m_playing = false, m_handedOver = false, m_paused = false;
        u32 m_random = 0x9E3779B9u;
        int m_refs = 1;
        Playlist* m_next = nullptr;
        Playlist* m_previous = nullptr;
    };
}
