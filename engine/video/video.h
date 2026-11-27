#pragma once

#include "assets/assets.h"
#include "audio/audio.h"

// Films: MPEG-1 video with MP2 sound, in an MPEG program stream (.mpg), decoded a frame at a time into a picture that
// is drawn like any other, with its sound played as it comes. Made with, for example,
//   ffmpeg -i in.mp4 -c:v mpeg1video -q:v 4 -c:a mp2 -b:a 160k -format mpeg out.mpg
typedef struct plm_t plm_t;

namespace nx
{
    class Video : NonCopyable
    {
    public:
        Video(Assets& assets, Audio& audio);
        ~Video();

        // Takes the file, from memAlloc. False if it isn't a film that can be played, or is too large to show.
        bool open(u8* bytes, size_t size);

        void play();
        void pause();
        // Back to the start, and paused.
        void stop();
        // Decodes what is due after so many seconds, while it plays.
        void update(float dt);

        bool playing() const { return m_playing; }
        bool done() const { return m_done; }
        bool loop() const { return m_loop; }
        void setLoop(bool loop);
        float volume() const { return m_volume; }
        void setVolume(float volume);
        float time() const;
        float duration() const;
        u32 width() const { return m_width; }
        u32 height() const { return m_height; }
        float framerate() const;
        u32 frames() const { return m_frames; }   // shown so far
        // The picture it is shown in; nullptr until it is open.
        Asset* picture() const { return m_picture; }

        // Every video made while a screen's sounds were set to a group, that plays: once a frame, for the screen on top.
        static void updateGroup(u32 group, float dt);

        void addRef() { m_refs++; }
        void release();

    private:
        struct Sound;

        bool openFile(u8* bytes, size_t size);
        void startSound();
        void endSound(bool stopNow);
        friend struct VideoCallbacks;

        Assets& m_assets;
        Audio& m_audio;
        plm_t* m_plm = nullptr;
        Asset* m_picture = nullptr;
        u8* m_rgba = nullptr;
        u8* m_bytes = nullptr;   // the file
        void* m_newFrame = nullptr;
        Sound* m_sound = nullptr;
        Voice m_voice;
        u32 m_width = 0, m_height = 0, m_frames = 0, m_group = 0;
        float m_volume = 1;
        bool m_playing = false, m_done = false, m_loop = false;
        int m_refs = 1;
        Video* m_next = nullptr;
        Video* m_previous = nullptr;
    };
}
