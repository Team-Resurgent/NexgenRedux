#include "video/video.h"

#include "base/log.h"
#include "base/memory.h"
#include "gpu/gpu.h"
#include "third_party/pl_mpeg/pl_mpeg.h"

#include <string.h>

namespace nx
{
    namespace
    {
        Video* g_videos = nullptr;  // every one there is, to be updated
        constexpr u32 kMostSide = 1024;

        // 0 to 255 from what the sums below can come to, -288 to 543.
        struct Clamp
        {
            u8 table[832];
            Clamp()
            {
                for (int i = 0; i < 832; i++)
                {
                    table[i] = static_cast<u8>(i < 288 ? 0 : i > 288 + 255 ? 255 : i - 288);
                }
            }
            u32 operator()(int v) const { return table[v + 288]; }
        };

        // As plm_frame_to_bgra, but a whole pixel a write, alpha too, which memory that is written to a line at a time
        // (as a texture's often is) takes much better than a byte at a time.
        void toBgra(const plm_frame_t& frame, u8* dest, u32 pitch)
        {
            static const Clamp clamp;
            const int cols = static_cast<int>(frame.width >> 1), rows = static_cast<int>(frame.height >> 1);
            const int yw = static_cast<int>(frame.y.width), cw = static_cast<int>(frame.cb.width);
            for (int row = 0; row < rows; row++)
            {
                const u8* y0 = frame.y.data + row * 2 * yw;
                const u8* y1 = y0 + yw;
                const u8* cb = frame.cb.data + row * cw;
                const u8* cr = frame.cr.data + row * cw;
                u32* out0 = reinterpret_cast<u32*>(dest + static_cast<size_t>(row) * 2 * pitch);
                u32* out1 = reinterpret_cast<u32*>(reinterpret_cast<u8*>(out0) + pitch);
                for (int col = 0; col < cols; col++)
                {
                    const int u = cb[col] - 128, v = cr[col] - 128;
                    const int r = (v * 104597) >> 16, g = (u * 25674 + v * 53278) >> 16, b = (u * 132201) >> 16;
                    const auto pixel = [&](int luma)
                    {
                        const int y = ((luma - 16) * 76309) >> 16;
                        return 0xFF000000u | clamp(y + r) << 16 | clamp(y - g) << 8 | clamp(y + b);
                    };
                    out0[col * 2] = pixel(y0[col * 2]);
                    out0[col * 2 + 1] = pixel(y0[col * 2 + 1]);
                    out1[col * 2] = pixel(y1[col * 2]);
                    out1[col * 2 + 1] = pixel(y1[col * 2 + 1]);
                }
            }
        }
    }

    // What its sound plays from. The audio frees it, once nothing plays it.
    struct Video::Sound
    {
        SoundStream stream;
        SoundData data;
        i16 converted[PLM_AUDIO_SAMPLES_PER_FRAME * 2];

        static void free(void* sound)
        {
            Sound* s = static_cast<Sound*>(sound);
            memFree(s->stream.samples);
            destroy(s);
        }
    };

    struct VideoCallbacks
    {
        static void frame(plm_t*, plm_frame_t* frame, void* user)
        {
            Video& v = *static_cast<Video*>(user);
            v.m_newFrame = frame;
        }

        static void samples(plm_t*, plm_samples_t* samples, void* user)
        {
            Video& v = *static_cast<Video*>(user);
            if (v.m_sound == nullptr)
            {
                return;
            }
            i16* out = v.m_sound->converted;
            for (u32 i = 0; i < samples->count * 2; i++)
            {
                const float f = samples->interleaved[i];
                out[i] = static_cast<i16>(f >= 1 ? 32767 : f <= -1 ? -32767 : f * 32767);
            }
            v.m_sound->stream.write(out, samples->count);  // what there is no room for is left out
        }
    };

    Video::Video(Assets& assets, Audio& audio) : m_assets(assets), m_audio(audio)
    {
        m_group = audio.group();
        m_next = g_videos;
        if (g_videos)
        {
            g_videos->m_previous = this;
        }
        g_videos = this;
    }

    Video::~Video()
    {
        (m_previous ? m_previous->m_next : g_videos) = m_next;
        if (m_next)
        {
            m_next->m_previous = m_previous;
        }
        endSound(true);
        if (m_plm)
        {
            plm_destroy(m_plm);
        }
        if (m_picture)
        {
            m_assets.release(m_picture);
        }
        memFree(m_rgba);
        memFree(m_bytes);
    }

    void Video::release()
    {
        if (--m_refs == 0)
        {
            destroy(this);
        }
    }

    bool Video::open(u8* bytes, size_t size)
    {
        if (!openFile(bytes, size))
        {
            if (m_plm)
            {
                plm_destroy(m_plm);
                m_plm = nullptr;
            }
            return false;
        }
        return true;
    }

    bool Video::openFile(u8* bytes, size_t size)
    {
        m_bytes = bytes;
        m_plm = bytes ? plm_create_with_memory(bytes, size, 0) : nullptr;
        if (m_plm == nullptr)
        {
            return false;
        }
        if (!plm_probe(m_plm, 5000 * 1024) || plm_get_num_video_streams(m_plm) == 0)
        {
            return false;
        }
        const int w = plm_get_width(m_plm), h = plm_get_height(m_plm);
        if (w <= 0 || h <= 0 || w > static_cast<int>(kMostSide) || h > static_cast<int>(kMostSide))
        {
            NX_ERROR("video: %dx%d is more than can be shown; at most %u a side", w, h, kMostSide);
            return false;
        }
        m_width = static_cast<u32>(w);
        m_height = static_cast<u32>(h);
        m_picture = m_assets.dynamicTexture(m_width, m_height);
        if (m_picture == nullptr)
        {
            return false;
        }
        plm_set_video_decode_callback(m_plm, VideoCallbacks::frame, this);
        plm_set_audio_decode_callback(m_plm, VideoCallbacks::samples, this);
        plm_set_audio_enabled(m_plm, plm_get_num_audio_streams(m_plm) > 0);
        plm_set_audio_lead_time(m_plm, 0.1);
        plm_set_loop(m_plm, m_loop);
        // The first picture straight away.
        if (plm_frame_t* first = plm_decode_video(m_plm))
        {
            m_newFrame = first;
            if (!show())
            {
                return false;
            }
            plm_rewind(m_plm);
        }
        return true;
    }

    // The picture decoded last. Where the device lets it, straight into the texture, as converting it once and copying it
    // again would cost the Original Xbox a frame.
    bool Video::show()
    {
        if (m_newFrame == nullptr)
        {
            return true;
        }
        const plm_frame_t& frame = *static_cast<plm_frame_t*>(m_newFrame);
        m_newFrame = nullptr;
        gpu::Device& device = m_assets.device();
        u32 pitch = 0;
        if (u8* memory = device.lockTexture(m_picture->texture, pitch))
        {
            toBgra(frame, memory, pitch);
            device.unlockTexture(m_picture->texture);
        }
        else
        {
            // Otherwise converted here first.
            const size_t bytes = static_cast<size_t>(m_width) * m_height * 4;
            if (m_rgba == nullptr && (m_rgba = static_cast<u8*>(memAlloc(bytes, MemTag::Texture))) != nullptr)
            {
                for (size_t i = 3; i < bytes; i += 4)
                {
                    m_rgba[i] = 255;  // what it converts leaves this alone
                }
            }
            if (m_rgba == nullptr)
            {
                return false;
            }
            plm_frame_to_rgba(const_cast<plm_frame_t*>(&frame), m_rgba, static_cast<int>(m_width * 4));
            device.updateTexture(m_picture->texture, m_rgba);
        }
        return true;
    }

    void Video::startSound()
    {
        if (m_sound || m_plm == nullptr || !plm_get_audio_enabled(m_plm))
        {
            return;
        }
        Sound* s = create<Sound>(MemTag::Audio);
        const u32 rate = static_cast<u32>(plm_get_samplerate(m_plm));
        if (s == nullptr || rate < 8000 || rate > 48000)
        {
            destroy(s);
            return;
        }
        // Half a second of it.
        s->stream.capacity = rate / 2;
        s->stream.samples = static_cast<i16*>(memAlloc(static_cast<size_t>(s->stream.capacity) * 2 * sizeof(i16), MemTag::Audio));
        if (s->stream.samples == nullptr)
        {
            destroy(s);
            return;
        }
        s->data.kind = SoundKind::Stream;
        s->data.channels = 2;
        s->data.rate = rate;
        s->data.data = reinterpret_cast<u8*>(&s->stream);
        PlayOptions options;
        options.volume = m_volume;
        options.priority = 200;
        m_voice = m_audio.playOwn(s->data, s, Sound::free, options);
        if (!m_voice)
        {
            Sound::free(s);
            return;
        }
        m_sound = s;
    }

    // Its sound is let go: played to the end of what it has, or stopped now. The audio frees it after.
    void Video::endSound(bool stopNow)
    {
        if (m_sound == nullptr)
        {
            return;
        }
        m_sound->stream.over.store(true, std::memory_order_release);
        if (stopNow)
        {
            m_audio.stop(m_voice, 0.05f);
        }
        m_sound = nullptr;
        m_voice = {};
    }

    void Video::play()
    {
        if (m_plm == nullptr)
        {
            return;
        }
        if (m_done)
        {
            plm_rewind(m_plm);
            m_done = false;
        }
        m_playing = true;
        startSound();
        m_audio.setPaused(m_voice, false);
    }

    void Video::pause()
    {
        if (m_playing && m_newFrame)
        {
            show();
            m_frames++;
        }
        m_playing = false;
        if (m_voice)
        {
            m_audio.setPaused(m_voice, true);
        }
    }

    void Video::stop()
    {
        pause();
        if (m_plm)
        {
            plm_rewind(m_plm);
        }
        if (m_sound)
        {
            m_sound->stream.flush();
        }
        m_done = false;
    }

    void Video::setLoop(bool loop)
    {
        m_loop = loop;
        if (m_plm)
        {
            plm_set_loop(m_plm, loop);
        }
    }

    void Video::setVolume(float volume)
    {
        m_volume = volume < 0 ? 0 : volume > 4 ? 4 : volume;
        if (m_voice)
        {
            m_audio.setVolume(m_voice, m_volume, 0.05f);
        }
    }

    float Video::time() const { return m_plm ? static_cast<float>(plm_get_time(m_plm)) : 0; }
    float Video::duration() const { return m_plm ? static_cast<float>(plm_get_duration(m_plm)) : 0; }
    float Video::framerate() const { return m_plm ? static_cast<float>(plm_get_framerate(m_plm)) : 0; }

    void Video::update(float dt)
    {
        if (!m_playing || m_plm == nullptr)
        {
            return;
        }
        // What was decoded the frame before is shown now, so that decoding and showing (each about half a frame on
        // the Original Xbox) don't fall in the same one. In a film of 30 a second or fewer, the next is due two frames on at the
        // soonest.
        if (m_newFrame)
        {
            show();
            m_frames++;
        }
        // A long frame isn't caught up all at once. If more than one picture comes, only the last is shown.
        plm_decode(m_plm, dt < 0 ? 0 : dt > 0.1f ? 0.1 : static_cast<double>(dt));
        if (plm_has_ended(m_plm))
        {
            if (m_newFrame)
            {
                show();  // the last picture
                m_frames++;
            }
            m_playing = false;
            m_done = true;
            endSound(false);  // what is left of it plays out
        }
    }

    void Video::updateGroup(u32 group, float dt)
    {
        for (Video* v = g_videos; v; v = v->m_next)
        {
            if (v->m_group == group)
            {
                v->update(dt);
            }
        }
    }
}
