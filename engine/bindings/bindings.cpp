#include "bindings/bindings.h"
#include "script/debugger.h"
#include "bindings/script_refs.h"
#include "screens/screens.h"
#include "net/net.h"
#include "video/video.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <type_traits>

namespace nx
{
    bool resolveAsset(const Services& services, const char* name, char* out, size_t size)
    {
        int length;
        if (strncmp(name, "global:", 7) == 0)
        {
            length = snprintf(out, size, "global/%s", name + 7);
        }
        else if (strncmp(name, "asset:", 6) == 0)
        {
            length = snprintf(out, size, "%s/asset/%s", services.folder, name + 6);
        }
        else if (strncmp(name, "own:", 4) == 0 && strstr(name, "..") == nullptr)
        {
            length = snprintf(out, size, "own/%s", name + 4);
        }
        else if (strncmp(name, "https://", 8) == 0 || strncmp(name, "http://", 7) == 0)
        {
            length = snprintf(out, size, "%s", name);
        }
        else
        {
            return false;
        }
        return length > 0 && static_cast<size_t>(length) < size;
    }

    namespace
    {
        template <typename T>
        T* wrap(Asset* asset, Assets& owner)
        {
            T* handle = create<T>(MemTag::Script);
            handle->asset = asset;
            handle->owner = &owner;
            handle->refs = 1;
            return handle;
        }

        TextureRef* loadTexture(const std::string& name)
        {
            Services& s = services();
            char path[192];
            if (!resolveAsset(s, name.c_str(), path, sizeof(path)))
            {
                scriptError("asset names start with asset: or global: (" + name + ")");
                return nullptr;
            }
            return wrap<TextureRef>(s.assets->texture(path), *s.assets);
        }

        Video* loadVideo(const std::string& name)
        {
            Services& s = services();
            FileData file;
            if (!readOwn(s, name.c_str(), file))
            {
                scriptError("there is no video " + name);
                return nullptr;
            }
            Video* v = create<Video>(MemTag::Script, *s.assets, *s.audio);
            if (v == nullptr)
            {
                return nullptr;
            }
            u8* bytes = file.data;
            const size_t size = file.size;
            file.data = nullptr;
            file.size = 0;
            if (!v->open(bytes, size))
            {
                v->release();
                scriptError(name + " isn't a video that can be played: MPEG-1, with MP2 sound if any, in a .mpg");
                return nullptr;
            }
            return v;
        }

        TextureRef* videoPicture(const Video* v)
        {
            Asset* a = v->picture();
            if (a == nullptr)
            {
                return nullptr;
            }
            services().assets->addRef(a);
            return wrap<TextureRef>(a, *services().assets);
        }

        TextureRef* makePicture(u32 width, u32 height)
        {
            Services& s = services();
            Asset* a = width && height && width <= 2048 && height <= 2048 ? s.assets->target(width, height) : nullptr;
            return a ? wrap<TextureRef>(a, *s.assets) : nullptr;
        }

        ModelRef* loadModelAsset(const std::string& name)
        {
            Services& s = services();
            char path[192];
            if (!resolveAsset(s, name.c_str(), path, sizeof(path)))
            {
                scriptError("asset names start with asset: or global: (" + name + ")");
                return nullptr;
            }
            return wrap<ModelRef>(s.assets->model(path), *s.assets);
        }

        EffectUse* loadEffectAsset(const std::string& name)
        {
            Services& s = services();
            char path[192];
            if (!resolveAsset(s, name.c_str(), path, sizeof(path)))
            {
                scriptError("asset names start with asset: or global: (" + name + ")");
                return nullptr;
            }
            EffectUse* use = create<EffectUse>(MemTag::Script);
            *use = {};
            use->asset = s.assets->effect(path);
            use->owner = s.assets;
            use->refs = 1;
            return use;
        }

        bool canvasEffect(const EffectUse* effect, Canvas* c) { return effect && c->effect(*effect->asset, effect->values, static_cast<float>(services().time)); }

        // A script's look: its settings are fields, so that they can be changed as numbers are.
        struct LookRef
        {
            ColorLook::Settings settings;
            ColorLook* look;
            int refs;

            void addRef() { refs++; }
            void release()
            {
                if (--refs == 0)
                {
                    destroy(look);
                    destroy(this);
                }
            }
        };
        static_assert(std::is_standard_layout_v<LookRef>);

        LookRef* makeLook()
        {
            LookRef* l = create<LookRef>(MemTag::Script);
            l->look = create<ColorLook>(MemTag::Script);
            l->refs = 1;
            return l;
        }
        LookRef* makeLookNamed(const std::string& name)
        {
            LookRef* l = makeLook();
            if (!l->look->preset(name.c_str()))
            {
                l->release();
                scriptError("no look is called " + name);
                return nullptr;
            }
            l->settings = l->look->settings;
            return l;
        }
        bool lookPreset(const std::string& name, LookRef* l)
        {
            l->look->settings = l->settings;
            const bool known = l->look->preset(name.c_str());
            l->settings = l->look->settings;
            return known;
        }
        bool lookLoad(const std::string& name, LookRef* l)
        {
            Services& s = services();
            char path[192];
            FileData file;
            if (!resolveAsset(s, name.c_str(), path, sizeof(path)) || strncmp(path, "http", 4) == 0)
            {
                scriptError("asset names start with asset: or global: (" + name + ")");
                return false;
            }
            return s.platform->readFile(path, file, MemTag::General) && l->look->readCube(reinterpret_cast<const char*>(file.data), file.size);
        }
        void lookForget(LookRef* l) { l->look->forgetCube(); }
        bool lookLoaded(const LookRef* l) { return l->look->hasCube(); }
        Color lookApply(const Color& c, LookRef* l)
        {
            l->look->settings = l->settings;
            float rgb[3] = { c.r, c.g, c.b };
            l->look->apply(rgb);
            return { rgb[0], rgb[1], rgb[2], c.a };
        }
        bool canvasGrade(LookRef* l, float strength, Canvas* c)
        {
            if (l == nullptr)
            {
                return false;
            }
            l->look->settings = l->settings;
            return c->grade(*l->look, strength);
        }

        FontRef* loadFont(const std::string& name, u32 size)
        {
            Services& s = services();
            char path[192];
            if (!resolveAsset(s, name.c_str(), path, sizeof(path)))
            {
                scriptError("asset names start with asset: or global: (" + name + ")");
                return nullptr;
            }
            return wrap<FontRef>(s.assets->font(path, size), *s.assets);
        }

        SoundRef* loadSoundAsset(const std::string& name)
        {
            Services& s = services();
            char path[192];
            if (!resolveAsset(s, name.c_str(), path, sizeof(path)))
            {
                scriptError("asset names start with asset: or global: (" + name + ")");
                return nullptr;
            }
            return wrap<SoundRef>(s.assets->sound(path), *s.assets);
        }

        u32 cachePending() { return services().assets->pending(); }
        u32 cacheTextureKilobytes() { return static_cast<u32>((services().assets->stats().typeBytes[0] + services().assets->stats().typeBytes[1]) / 1024); }
        u32 cacheSoundKilobytes() { return static_cast<u32>(services().assets->stats().typeBytes[2] / 1024); }
        u32 cacheSoundBudgetKilobytes() { return static_cast<u32>(services().assets->budget(AssetType::Sound) / 1024); }
        u32 cacheInUse() { return services().assets->stats().inUse; }
        u32 cacheWarm() { return services().assets->stats().warm; }
        u32 cacheHits() { return services().assets->stats().hits; }
        u32 cacheLoads() { return services().assets->stats().loads; }
        u32 cacheEvictions() { return services().assets->stats().evictions; }
        u32 cacheKilobytes()
        {
            const AssetStats a = services().assets->stats();
            return static_cast<u32>((a.inUseBytes + a.warmBytes) / 1024);
        }
        u32 cacheBudgetKilobytes() { return static_cast<u32>(services().assets->budget(AssetType::Texture) / 1024); }
        void cacheTrim() { services().assets->trim(); }

        // --- Canvas: scripts pass handles and references, the canvas takes the assets behind them ---

        const Asset* assetOf(const AssetHandle* handle)
        {
            if (handle == nullptr)
            {
                scriptError("null texture or font");
                return nullptr;
            }
            return handle->asset;
        }

        bool beginPicture(const TextureRef* t, const Color& clear, Canvas* c) { return t && t->asset && c->beginTarget(*t->asset, clear); }
        void endPicture(Canvas* c) { c->endTarget(); }
        void drawImage(const TextureRef* t, float x, float y, const Color& tint, Canvas* c)
        {
            if (const Asset* a = assetOf(t)) c->image(*a, x, y, t->width(), t->height(), tint);
        }
        void drawImageSized(const TextureRef* t, float x, float y, float w, float h, const Color& tint, Canvas* c)
        {
            if (const Asset* a = assetOf(t)) c->image(*a, x, y, w, h, tint);
        }
        void drawImagePart(const TextureRef* t, float x, float y, float w, float h, float sx, float sy, float sw, float sh, const Color& tint, Canvas* c)
        {
            if (const Asset* a = assetOf(t)) c->imagePart(*a, x, y, w, h, sx, sy, sw, sh, tint);
        }
        void drawSprite(const TextureRef* t, u32 columns, u32 rows, u32 frame, float x, float y, float w, float h, const Color& tint, Canvas* c)
        {
            if (const Asset* a = assetOf(t)) c->sprite(*a, columns, rows, frame, x, y, w, h, tint);
        }
        void drawNineSlice(const TextureRef* t, float x, float y, float w, float h, float corner, const Color& tint, Canvas* c)
        {
            if (const Asset* a = assetOf(t)) c->nineSlice(*a, x, y, w, h, corner, tint);
        }
        void drawTile(const TextureRef* t, float x, float y, float w, float h, float tw, float th, float ox, float oy, const Color& tint, Canvas* c)
        {
            if (const Asset* a = assetOf(t)) c->tile(*a, x, y, w, h, tw, th, ox, oy, tint);
        }
        float drawText(const FontRef* f, const std::string& text, float x, float y, const Color& color, Align align, Canvas* c)
        {
            const Asset* a = assetOf(f);
            return a ? c->text(*a, text.c_str(), x, y, color, align) : 0;
        }
        float drawParagraph(const FontRef* f, const std::string& text, float x, float y, float width, const Color& color, Canvas* c)
        {
            const Asset* a = assetOf(f);
            return a ? c->paragraph(*a, text.c_str(), x, y, width, color) : 0;
        }
        float drawRich(const FontRef* f, const std::string& text, float x, float y, float width, const Color& color, Align align, Canvas* c)
        {
            const Asset* a = assetOf(f);
            return a ? c->rich(*a, text.c_str(), x, y, width, color, align) : 0;
        }
        float richHeight(const FontRef* f, const std::string& text, float width, Canvas* c)
        {
            const Asset* a = assetOf(f);
            return a ? c->richHeight(*a, text.c_str(), width) : 0;
        }
        Vec2 fontMeasure(const std::string& text, const FontRef* f) { return services().canvas->measure(*f->asset, text.c_str()); }

        // --- Sound ---

        // A script's name for something playing. Safe to keep after it has ended.
        struct PlayingSound
        {
            u32 value;  // no initial value: only plain types are returned the same way by every compiler
            Voice voice() const { return { value }; }
            bool playing() const { return services().audio->playing(voice()); }
            float position() const { return services().audio->position(voice()); }
            void stop(float fade) { services().audio->stop(voice(), fade); }
            void setVolume(float volume, float seconds) { services().audio->setVolume(voice(), volume, seconds); }
            void setPan(float pan) { services().audio->setPan(voice(), pan); }
            void setPitch(float pitch) { services().audio->setPitch(voice(), pitch); }
            void setPaused(bool paused) { services().audio->setPaused(voice(), paused); }
        };

        static_assert(std::is_trivial_v<PlayingSound>);

        void makePlaying(void* memory) { new (memory) PlayingSound{ 0 }; }

        PlayingSound playSound(const SoundRef& sound, const PlayOptions& options)
        {
            Services& s = services();
            PlayingSound playing = { 0 };
            if (sound.asset->sound)
            {
                // The voice holds on to the sound until it has finished with it.
                s.assets->addRef(sound.asset);
                playing.value = s.audio->play(*sound.asset->sound, sound.asset, options).value;
                if (playing.value == 0)
                {
                    s.assets->release(sound.asset);
                }
            }
            return playing;
        }

        PlayingSound soundPlay(float volume, float pan, float pitch, bool loop, SoundRef* sound)
        {
            PlayOptions o;
            o.volume = volume;
            o.pan = pan;
            o.pitch = pitch;
            o.loop = loop;
            return playSound(*sound, o);
        }

        PlayingSound soundPlayMusic(float volume, float fadeIn, bool loop, SoundRef* sound)
        {
            PlayOptions o;
            o.volume = volume;
            o.fadeIn = fadeIn;
            o.loop = loop;
            o.bus = Bus::Music;
            o.priority = 255;
            return playSound(*sound, o);
        }

        // From a place in space, or from an entity, until either ends.
        PlayingSound playFrom(SoundRef* sound, u32 entity, const Vec3& place, float volume, float range, float pitch, bool loop)
        {
            Space& space = services().world->space();
            PlayOptions o;
            space.hear(place, volume, range, o.volume, o.pan);
            o.pitch = pitch;
            o.loop = loop;
            o.priority = loop ? 140 : 120;
            const PlayingSound playing = playSound(*sound, o);
            space.follow(Voice{ playing.value }, entity, place, volume, range);
            return playing;
        }
        PlayingSound soundPlayAt(const Vec3& place, float volume, float range, float pitch, SoundRef* sound)
        {
            return playFrom(sound, 0, place, volume, range, pitch, false);
        }
        PlayingSound entitySound(SoundRef* sound, float volume, float range, bool loop, EntityRef* e)
        {
            if (sound == nullptr)
            {
                scriptError("null sound");
            }
            else if (e->world && e->world->alive(e->id))
            {
                return playFrom(sound, e->id, e->world->space().worldPlace(e->id), volume, range, 1, loop);
            }
            return { 0 };
        }

        float soundSeconds(const SoundRef* sound) { const SoundData* d = sound->asset->sound; return d && d->rate ? static_cast<float>(d->frames) / static_cast<float>(d->rate) : 0; }
        bool soundKnown(Bus bus) { return bus < Bus::Count; }
        void setBus(Bus bus, float volume) { if (soundKnown(bus)) services().audio->setBusVolume(bus, volume); }
        float getBus(Bus bus) { return soundKnown(bus) ? services().audio->busVolume(bus) : 0; }
        // Each changes one part of what a bus goes through, and leaves the rest.
        template <typename Change> void changeEffects(Bus bus, float seconds, Change change)
        {
            if (soundKnown(bus))
            {
                BusEffectSettings s = services().audio->effects(bus);
                change(s);
                services().audio->setEffects(bus, s, seconds);
            }
        }
        void audioLowPass(Bus bus, float hertz, float seconds) { changeEffects(bus, seconds, [&](BusEffectSettings& s) { s.lowPass = hertz; }); }
        void audioHighPass(Bus bus, float hertz, float seconds) { changeEffects(bus, seconds, [&](BusEffectSettings& s) { s.highPass = hertz; }); }
        void audioReverb(Bus bus, float amount, float room, float seconds) { changeEffects(bus, seconds, [&](BusEffectSettings& s) { s.reverb = amount; s.room = room; }); }
        void audioEcho(Bus bus, float amount, float delay, float feedback, float seconds)
        {
            changeEffects(bus, seconds, [&](BusEffectSettings& s) { s.echo = amount; s.echoDelay = delay; s.echoFeedback = feedback; });
        }
        void audioClearEffects(float seconds) { services().audio->clearEffects(seconds); }
        void setMaster(float volume) { services().audio->setMasterVolume(volume); }
        float getMaster() { return services().audio->masterVolume(); }
        float audioLevel() { return services().audio->level(); }
        float audioSpectrum(u32 band) { return services().audio->spectrum(band); }
        float audioWave(u32 index) { return services().audio->wave(index); }
        u32 audioVoices() { return services().audio->voicesInUse(); }
        u32 audioVoiceLimit() { return Audio::kVoices; }
        std::string audioOutput() { return services().audio->outputName(); }

        // --- Coroutines ---

        void coStart(asIScriptFunction* function)
        {
            if (!services().coroutines->start(function))
            {
                scriptError("cannot start another coroutine");
            }
        }
        void coWait(float seconds)
        {
            if (!services().coroutines->wait(seconds))
            {
                scriptError("wait() works only inside a coroutine");
            }
        }
        void coNext() { coWait(0); }
        u32 coCount() { return services().coroutines->count(); }

        // --- Saving ---

        void saveSet(const std::string& key, const std::string& value)
        {
            if (!services().save->set(key.c_str(), value.c_str()))
            {
                scriptError("cannot save " + key);
            }
        }
        std::string saveGet(const std::string& key, const std::string& otherwise) { return services().save->get(key.c_str(), otherwise.c_str()); }
        void saveRemove(const std::string& key) { services().save->remove(key.c_str()); }

        // --- Input ---

        bool action(const std::string& name, Action& out)
        {
            if (!Input::parse(name.c_str(), out))
            {
                scriptError("no action called " + name);
                return false;
            }
            return true;
        }

        bool inputHeld(const std::string& name) { Action a; return action(name, a) && services().input->held(a); }
        bool inputPressed(const std::string& name) { Action a; return action(name, a) && services().input->pressed(a); }
        bool inputReleased(const std::string& name) { Action a; return action(name, a) && services().input->released(a); }
        bool inputRepeated(const std::string& name) { Action a; return action(name, a) && services().input->repeated(a); }
        Vec2 inputMove() { return services().input->move(); }
        bool inputUsingPad() { return services().input->usingPad(); }
        bool inputHasMouse() { return services().input->raw().hasMouse; }
        bool inputStickPointer() { return services().input->stickPointer(); }
        void inputRumble(float strength, float seconds, int pad) { services().input->rumble(pad, strength, strength, seconds); }
        void inputRumbleMotors(float heavy, float light, float seconds, int pad) { services().input->rumble(pad, heavy, light, seconds); }
        void inputStopRumble() { services().input->stopRumble(); }
        bool inputRumbleOn() { return services().input->rumbleOn(); }
        void inputSetRumbleOn(bool on) { services().input->setRumbleOn(on); }
        void inputSetStickPointer(bool on) { services().input->setStickPointer(on); }
        bool inputWantsPointer() { return services().input->wantsPointer(); }
        void inputSetWantsPointer(bool on) { services().input->setWantsPointer(on); }
        Vec2 inputMouse() { const InputState& i = services().input->raw(); return services().canvas->toDesign(i.mouseX, i.mouseY); }
        bool inputMouseDown(u32 button) { return button < 3 && services().input->raw().mouseButtons[button]; }
        float inputWheel() { return services().input->wheel(); }
        // In design units, as the pointer is.
        Vec2 inputTurn()
        {
            const Vec2 a = services().canvas->toDesign(0, 0), b = services().canvas->toDesign(services().input->turnX(), services().input->turnY());
            return { b.x - a.x, b.y - a.y };
        }
        void inputLock(bool on) { services().platform->lockMouse(on); }
        bool inputKey(const std::string& name)
        {
            const Input& input = *services().input;
            for (size_t k = 1; k < static_cast<size_t>(Key::Count); k++)
            {
                if (name == Input::keyName(static_cast<Key>(k)))
                {
                    return !input.typing() && input.raw().keys[k];
                }
            }
            scriptError("no key called " + name);
            return false;
        }
        const Gamepad* pad(u32 index) { return index < kMaxPads ? &services().input->raw().pads[index] : nullptr; }
        bool padConnected(u32 index) { const Gamepad* p = pad(index); return p && p->connected; }
        Vec2 padLeft(u32 index) { const Gamepad* p = pad(index); return p ? Vec2(p->leftX, p->leftY) : Vec2(); }
        Vec2 padRight(u32 index) { const Gamepad* p = pad(index); return p ? Vec2(p->rightX, p->rightY) : Vec2(); }
        Vec2 padTriggers(u32 index) { const Gamepad* p = pad(index); return p ? Vec2(p->leftTrigger, p->rightTrigger) : Vec2(); }
        bool padButton(u32 index, u32 button) { const Gamepad* p = pad(index); return p && button < static_cast<u32>(Button::Count) && p->down(static_cast<Button>(button)); }

        void inputListen(const std::string& name) { Action a; if (action(name, a)) services().input->listen(a); }
        bool inputListening() { return services().input->listening(); }
        std::string inputKeys(const std::string& name) { Action a; return action(name, a) ? services().input->describeKeys(a) : ""; }
        std::string inputButtons(const std::string& name) { Action a; return action(name, a) ? services().input->describeButtons(a) : ""; }
        void inputReset() { services().input->resetBindings(); }

        // --- Screens ---

        void screensPush(const std::string& id) { services().screens->push(id.c_str()); }
        void screensPop() { services().screens->pop(); }
        void screensReplace(const std::string& id) { services().screens->replace(id.c_str()); }
        void screensHome() { services().screens->home(); }
        u32 screensCount() { return services().screens->count(); }
        const ScreenInfo* screen(u32 index)
        {
            const ScreenInfo* info = services().screens->info(index);
            if (info == nullptr)
            {
                scriptError("no such screen");
            }
            return info;
        }
        std::string screensId(u32 i) { const ScreenInfo* s = screen(i); return s ? s->id : ""; }
        std::string screensTitle(u32 i) { const ScreenInfo* s = screen(i); return s ? s->title : ""; }
        std::string screensDescription(u32 i) { const ScreenInfo* s = screen(i); return s ? s->description : ""; }
        std::string screensCategory(u32 i) { const ScreenInfo* s = screen(i); return s ? s->category : ""; }

        // --- App and maths ---

        float appTime() { return static_cast<float>(services().time); }
        double appNow() { return services().platform->seconds(); }
        float appFps() { return services().fps; }
        bool appDevelopment() { return services().development; }
        void appSetDevelopment(bool on) { services().development = on; }
        bool appDebugger(u32 port, bool otherMachines)
        {
            ScriptDebugger* d = services().debugger;
            return d && (d->listening() || (port > 0 && port < 65536 && d->start(*services().host, static_cast<u16>(port), !otherMachines)));
        }
        bool appDebugging() { return services().debugger && services().debugger->connected(); }
        u32 appReloads() { return services().reloads; }
        u32 appDraws() { return services().draws; }
        std::string appPlatform() { return services().platform->name(); }
        std::string appGpu() { return services().device->name(); }
        u32 appMemoryKilobytes() { return static_cast<u32>(memStatsTotal().bytes / 1024); }
        u32 appScriptKilobytes() { return static_cast<u32>(memStats(MemTag::Script).bytes / 1024); }
        u32 appFreeKilobytes() { return static_cast<u32>(services().platform->freeMemory() / 1024); }

        float randomUnit()
        {
            u32& r = services().random;  // xorshift
            r ^= r << 13;
            r ^= r >> 17;
            r ^= r << 5;
            return static_cast<float>(r >> 8) / 16777216.0f;
        }
        float randomRange(float low, float high) { return low + (high - low) * randomUnit(); }
        int randomInt(int low, int high)
        {
            return high <= low ? low : low + static_cast<int>(randomUnit() * static_cast<float>(high - low + 1));
        }
        float lerp(float a, float b, float t) { return a + (b - a) * t; }
        float clampFloat(float v, float low, float high) { return v < low ? low : v > high ? high : v; }
        float leastOf(float a, float b) { return a < b ? a : b; }
        float mostOf(float a, float b) { return a > b ? a : b; }
        float smooth(float t) { t = clampFloat(t, 0, 1); return t * t * (3 - 2 * t); }
        float bounce(float t)
        {
            t = clampFloat(t, 0, 1);
            if (t < 1 / 2.75f)
            {
                return 7.5625f * t * t;
            }
            if (t < 2 / 2.75f)
            {
                t -= 1.5f / 2.75f;
                return 7.5625f * t * t + 0.75f;
            }
            if (t < 2.5f / 2.75f)
            {
                t -= 2.25f / 2.75f;
                return 7.5625f * t * t + 0.9375f;
            }
            t -= 2.625f / 2.75f;
            return 7.5625f * t * t + 0.984375f;
        }
        enum class Ease : int { Linear, Smooth, In, Out, Bounce, Overshoot, Elastic };

        float ease(Ease kind, float t)
        {
            t = clampFloat(t, 0, 1);
            switch (kind)
            {
            case Ease::Smooth: return smooth(t);
            case Ease::In: return t * t * t;
            case Ease::Out: return 1 - (1 - t) * (1 - t) * (1 - t);
            case Ease::Bounce: return bounce(t);
            case Ease::Overshoot:
            {
                const float s = t - 1;
                return s * s * (2.70158f * s + 1.70158f) + 1;
            }
            case Ease::Elastic:
                return t <= 0 || t >= 1 ? t : 1 + powf(2, -10 * t) * sinf((t * 10 - 0.75f) * 2.0943951f);
            default: return t;
            }
        }

        // Keeps its own time, so there is nothing to update and nothing to clean up.
        struct Tween
        {
            float from = 0, to = 0, start = 0, length = 0;
            Ease kind = Ease::Smooth;

            float part() const
            {
                const float gone = static_cast<float>(services().time) - start;
                return length <= 0 ? 1 : clampFloat(gone / length, 0, 1);
            }
            float value() const { return from + (to - from) * ease(kind, part()); }
            bool done() const { return part() >= 1; }
            // From wherever it is now, so changing course never jumps.
            void go(float target, float seconds, Ease k)
            {
                from = value();
                to = target;
                length = seconds;
                kind = k;
                start = static_cast<float>(services().time);
            }
        };

        void makeTween(void* memory) { new (memory) Tween(); }
        void makeTweenAt(float value, void* memory)
        {
            Tween* t = new (memory) Tween();
            t->from = t->to = value;
        }
        void makeTweenFull(float from, float to, float seconds, Ease kind, void* memory)
        {
            Tween* t = new (memory) Tween();
            t->from = from;
            t->to = to;
            t->length = seconds;
            t->kind = kind;
            t->start = static_cast<float>(services().time);
        }

        Color hsv(float hue, float saturation, float value)
        {
            hue -= static_cast<float>(static_cast<int>(hue));
            hue = (hue < 0 ? hue + 1 : hue) * 6;
            const int sector = static_cast<int>(hue) % 6;
            const float f = hue - static_cast<float>(static_cast<int>(hue));
            const float p = value * (1 - saturation);
            const float q = value * (1 - saturation * f);
            const float t = value * (1 - saturation * (1 - f));
            switch (sector)
            {
            case 0: return { value, t, p, 1 };
            case 1: return { q, value, p, 1 };
            case 2: return { p, value, t, 1 };
            case 3: return { p, q, value, 1 };
            case 4: return { t, p, value, 1 };
            default: return { value, p, q, 1 };
            }
        }
        Color lerpColor(const Color& a, const Color& b, float t) { return { a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t }; }
        Color fade(const Color& c, float alpha) { return { c.r, c.g, c.b, c.a * alpha }; }

        void makeColor(void* memory) { new (memory) Color(); }
        void makeColorRgba(float r, float g, float b, float a, void* memory) { new (memory) Color{ r, g, b, a }; }

        const Color kWhite = { 1, 1, 1, 1 };
        const Color kBlack = { 0, 0, 0, 1 };
        const Color kRed = { 0.93f, 0.26f, 0.26f, 1 };
        const Color kOrange = { 0.98f, 0.58f, 0.18f, 1 };
        const Color kYellow = { 0.98f, 0.85f, 0.25f, 1 };
        const Color kGreen = { 0.3f, 0.82f, 0.42f, 1 };
        const Color kBlue = { 0.27f, 0.55f, 0.96f, 1 };
        const Color kPurple = { 0.66f, 0.4f, 0.93f, 1 };
        const Color kGray = { 0.55f, 0.58f, 0.63f, 1 };

        void bindColor(Binder& b)
        {
            b.valueType("Color", sizeof(Color), asOBJ_POD | asOBJ_APP_CLASS_ALLFLOATS | asGetTypeTraits<Color>(), "Red, green, blue and alpha, each 0 to 1.");
            b.constructor("void f()", NX_OBJ_LAST(makeColor), "Opaque black.");
            b.constructor("void f(float r, float g, float b, float a = 1)", NX_OBJ_LAST(makeColorRgba), "From its parts.");
            b.property("float r", offsetof(Color, r), "Red.");
            b.property("float g", offsetof(Color, g), "Green.");
            b.property("float b", offsetof(Color, b), "Blue.");
            b.property("float a", offsetof(Color, a), "Alpha: 0 is invisible.");
            const struct { const char* name; const Color* value; } all[] = {
                { "const Color White", &kWhite }, { "const Color Black", &kBlack }, { "const Color Red", &kRed }, { "const Color Orange", &kOrange }, { "const Color Yellow", &kYellow },
                { "const Color Green", &kGreen }, { "const Color Blue", &kBlue }, { "const Color Purple", &kPurple }, { "const Color Gray", &kGray },
            };
            for (const auto& c : all)
            {
                b.constant(c.name, const_cast<Color*>(c.value), "A ready-made colour.");
            }
            b.function("Color hsv(float hue, float saturation, float value)", NX_FN(hsv), "A colour from its hue (0 to 1 around the wheel), saturation and brightness.");
            b.function("Color lerp(const Color &in a, const Color &in b, float t)", NX_FN(lerpColor), "From a to b as t goes from 0 to 1.");
            b.function("Color fade(const Color &in color, float alpha)", NX_FN(fade), "The same colour, more transparent.");
        }

        void bindAssets(Binder& b)
        {
            b.refType("Texture", 0, "An image. Loaded once and shared by everyone who asks for it.");
            b.behaviour(asBEHAVE_ADDREF, "void f()", NX_METHOD(TextureRef, addRef));
            b.behaviour(asBEHAVE_RELEASE, "void f()", NX_METHOD(TextureRef, release));
            b.method("float get_width() const property", NX_METHOD(TextureRef, width), "Width in its own pixels.");
            b.method("float get_height() const property", NX_METHOD(TextureRef, height), "Height in its own pixels.");
            b.method("bool get_failed() const property", NX_METHOD(TextureRef, failed), "Whether the file could not be loaded. It then draws as a checkerboard.");
            b.method("bool get_ready() const property", NX_METHOD(TextureRef, ready), "Whether it has arrived. Until then it draws as nothing.");

            bindSpaceTypes(b);

            b.refType("Font", 0, "A scalable SSFN font at one size.");
            b.behaviour(asBEHAVE_ADDREF, "void f()", NX_METHOD(FontRef, addRef));
            b.behaviour(asBEHAVE_RELEASE, "void f()", NX_METHOD(FontRef, release));
            b.method("Vec2 measure(const string &in text) const", NX_OBJ_LAST(fontMeasure), "How much room some text takes.");
            b.method("bool get_failed() const property", NX_METHOD(FontRef, failed), "Whether the file could not be loaded.");

            b.refType("Sound", 0, "Something to play: WAV, Ogg Vorbis, MP3 or cooked. Short effects are kept ready; long music is decoded while it plays.");
            b.behaviour(asBEHAVE_ADDREF, "void f()", NX_METHOD(SoundRef, addRef));
            b.behaviour(asBEHAVE_RELEASE, "void f()", NX_METHOD(SoundRef, release));

            b.refType("Video", 0, "A film: MPEG-1 with MP2 sound, in a .mpg. Its picture is drawn like any other image, its sound comes with it, and it plays by itself while its screen is on top.");
            b.behaviour(asBEHAVE_ADDREF, "void f()", NX_METHOD(Video, addRef));
            b.behaviour(asBEHAVE_RELEASE, "void f()", NX_METHOD(Video, release));
            b.method("void play()", NX_METHOD(Video, play), "Plays it, from where it was, or from the start once it is done.");
            b.method("void pause()", NX_METHOD(Video, pause), "Stops it where it is.");
            b.method("void stop()", NX_METHOD(Video, stop), "Back to the start, and paused.");
            b.method("bool get_playing() const property", NX_METHOD(Video, playing), "Whether it is playing.");
            b.method("bool get_done() const property", NX_METHOD(Video, done), "Whether it has come to the end.");
            b.method("bool get_loop() const property", NX_METHOD(Video, loop), "Whether it starts again at the end.");
            b.method("void set_loop(bool) property", NX_METHOD(Video, setLoop), "Starts it again at the end, or not.");
            b.method("float get_volume() const property", NX_METHOD(Video, volume), "How loud its sound is: 1 as it is.");
            b.method("void set_volume(float) property", NX_METHOD(Video, setVolume), "Makes its sound louder or softer.");
            b.method("float get_time() const property", NX_METHOD(Video, time), "Seconds into it.");
            b.method("float get_duration() const property", NX_METHOD(Video, duration), "Seconds long.");
            b.method("float get_framerate() const property", NX_METHOD(Video, framerate), "Pictures a second.");
            b.method("uint get_width() const property", NX_METHOD(Video, width), "Its width in pixels.");
            b.method("uint get_height() const property", NX_METHOD(Video, height), "Its height in pixels.");
            b.method("uint get_frames() const property", NX_METHOD(Video, frames), "Pictures shown so far.");
            b.method("Texture@ get_picture() const property", NX_OBJ_LAST(videoPicture), "What it shows now: draw it with Canvas.image, or put it on an entity or a model like any other image.");

            b.section("Assets", "Loading. Names start with `asset:` for the screen's own files, `global:` for shared ones or `own:` for those that the player brought.");
            b.names("Assets");
            b.function("Texture@ picture(uint width, uint height)", NX_FN(makePicture), "A blank picture of its own to draw into, with Canvas.beginPicture, and then to draw like any other: a minimap, a screen in a room, a mirror, something drawn once and shown many times. Up to 2048 pixels a side; null where the machine can't.");
            b.function("Texture@ texture(const string &in name)", NX_FN(loadTexture), "An image: PNG, JPEG, TGA, BMP or GIF, up to 2048 pixels a side; from the web too, by its https:// address. What a screen asks for as it is made is ready by onEnter(); later ones arrive in the background.");
            b.function("Sound@ sound(const string &in name)", NX_FN(loadSoundAsset), "A sound. Arrives like an image.");
            b.function("Video@ video(const string &in name)", NX_FN(loadVideo), "A film, read at once: MPEG-1 with MP2 sound in a .mpg, up to 1024 pixels a side; `ffmpeg -i in.mp4 -c:v mpeg1video -q:v 4 -c:a mp2 -format mpeg out.mpg` makes one. Paused at its first picture until it is played.");
            b.function("Effect@ effect(const string &in name)", NX_FN(loadEffectAsset), "A shader of your own: a .fx file. Each time gives one whose values are its own.");
            b.function("Model@ model(const string &in name)", NX_FN(loadModelAsset), "A 3D model: glTF (.glb or .gltf), FBX, OBJ, STL or PLY. Arrives like an image. On the original Xbox an FBX has to be cooked.");
            b.function("uint get_pending() property", NX_FN(cachePending), "Files still on their way.");
            b.function("uint get_textureKilobytes() property", NX_FN(cacheTextureKilobytes), "Memory that images and fonts take.");
            b.function("uint get_soundKilobytes() property", NX_FN(cacheSoundKilobytes), "Memory that sounds take.");
            b.function("uint get_soundBudgetKilobytes() property", NX_FN(cacheSoundBudgetKilobytes), "Memory that sounds may take.");
            b.function("Font@ font(const string &in name, uint size)", NX_FN(loadFont), "A font at a size in design units.");
            b.function("uint get_inUse() property", NX_FN(cacheInUse), "Assets that something is holding.");
            b.function("uint get_warm() property", NX_FN(cacheWarm), "Assets kept ready though nothing holds them.");
            b.function("uint get_hits() property", NX_FN(cacheHits), "Requests served without reading a file.");
            b.function("uint get_loads() property", NX_FN(cacheLoads), "Files read.");
            b.function("uint get_evictions() property", NX_FN(cacheEvictions), "Assets unloaded to make room.");
            b.function("uint get_kilobytes() property", NX_FN(cacheKilobytes), "Memory the loaded assets take.");
            b.function("uint get_budgetKilobytes() property", NX_FN(cacheBudgetKilobytes), "Memory that images and fonts may take.");
            b.function("void trim()", NX_FN(cacheTrim), "Unloads everything that isn't held. Rarely needed.");
            b.names("");
        }

        void bindCanvas(Binder& b)
        {
            b.enumType("Blend", "How drawing combines with what is there.");
            b.enumValue("BlendNone", static_cast<int>(gpu::Blend::None), "Replaces it.");
            b.enumValue("BlendAlpha", static_cast<int>(gpu::Blend::Alpha), "Covers it, by alpha.");
            b.enumValue("BlendAdd", static_cast<int>(gpu::Blend::Additive), "Adds light to it.");
            b.enumValue("BlendMultiply", static_cast<int>(gpu::Blend::Multiply), "Darkens it by the colour: white leaves it as it was. For shading, and maps of light.");
            b.enumType("Filter", "How images are scaled.");
            b.enumValue("FilterLinear", static_cast<int>(gpu::Filter::Linear), "Smoothly.");
            b.enumValue("FilterNearest", static_cast<int>(gpu::Filter::Nearest), "As sharp pixels.");
            b.enumType("Align", "Which part of some text sits at its position.");
            b.enumValue("AlignLeft", static_cast<int>(Align::Left), "Its left edge.");
            b.enumValue("AlignCenter", static_cast<int>(Align::Center), "Its middle.");
            b.enumValue("AlignRight", static_cast<int>(Align::Right), "Its right edge.");

            b.refType("Look", 0, "What each colour of a picture becomes: warmer, colder, faded, harder. Settings, a preset by name, or a .cube table from a grading program, or all of them: the table first, then the settings. Given to `Canvas.grade`.");
            b.behaviour(asBEHAVE_FACTORY, "Look@ f()", NX_FN(makeLook));
            b.behaviour(asBEHAVE_FACTORY, "Look@ f(const string &in preset)", NX_FN(makeLookNamed));
            b.behaviour(asBEHAVE_ADDREF, "void f()", NX_METHOD(LookRef, addRef));
            b.behaviour(asBEHAVE_RELEASE, "void f()", NX_METHOD(LookRef, release));
            const size_t settings = offsetof(LookRef, settings);
            b.property("float exposure", settings + offsetof(ColorLook::Settings, exposure), "Stops brighter: 1 is twice as bright, -1 half. 0 to begin with.");
            b.property("float warmth", settings + offsetof(ColorLook::Settings, warmth), "-1 bluer to 1 more orange.");
            b.property("float tint", settings + offsetof(ColorLook::Settings, tint), "-1 greener to 1 more magenta.");
            b.property("float contrast", settings + offsetof(ColorLook::Settings, contrast), "How far colours are from the middle: 1 as they are, more is harder, less is flatter.");
            b.property("float saturation", settings + offsetof(ColorLook::Settings, saturation), "How colourful: 0 is grey, 1 as it is, more is stronger.");
            b.property("float fade", settings + offsetof(ColorLook::Settings, fade), "0 to 1: blacks lifted and whites dimmed, as old film.");
            b.property("float gamma", settings + offsetof(ColorLook::Settings, gamma), "More than 1 lifts the middle, less sinks it.");
            b.property("Color shadows", settings + offsetof(ColorLook::Settings, shadows), "A colour for what is dark, keeping how bright it is; its alpha is how much. None to begin with.");
            b.property("Color highlights", settings + offsetof(ColorLook::Settings, highlights), "A colour for what is bright, the same way.");
            b.method("bool preset(const string &in name)", NX_OBJ_LAST(lookPreset), "Sets everything to one ready-made: `none`, `warm`, `cold`, `faded`, `noir`, `sepia`, `dusk`, `vivid`, `bleach` or `night`. False if there is none of that name. A table that was loaded stays.");
            b.method("bool load(const string &in name)", NX_OBJ_LAST(lookLoad), "A .cube 3D table from a grading program (Resolve, Photoshop and others write them), up to 64 a side, read now. Colours go through it before the settings. False if it can't be read.");
            b.method("void unload()", NX_OBJ_LAST(lookForget), "Forgets the table.");
            b.method("bool get_loaded() const property", NX_OBJ_LAST(lookLoaded), "Whether it has a table.");
            b.method("Color apply(const Color &in color)", NX_OBJ_LAST(lookApply), "What it makes of one colour, worked out here rather than on the GPU.");

            b.refType("Canvas", asOBJ_NOCOUNT, "2D drawing, passed to a screen's draw(). Positions are in design units from the top left, y down.");
            b.method("void design(float width, float height)", NX_METHOD(Canvas, design), "Draws on a screen of this size, fitted inside the real one. Call it first.");
            b.method("void designHeight(float height)", NX_METHOD(Canvas, designHeight), "The same, but as wide as the real screen's shape allows. Lay out by width.");
            b.method("float get_width() const property", NX_METHOD(Canvas, width), "Design width.");
            b.method("float get_height() const property", NX_METHOD(Canvas, height), "Design height.");
            b.method("void push()", NX_METHOD(Canvas, push), "Saves position, rotation, scale, alpha, blend, filter and clip.");
            b.method("void pop()", NX_METHOD(Canvas, pop), "Goes back to the last push().");
            b.method("void translate(float x, float y)", NX_METHOD(Canvas, translate), "Moves what is drawn next.");
            b.method("void rotate(float degrees)", NX_METHOD(Canvas, rotate), "Turns it clockwise.");
            b.method("void scale(float x, float y)", NX_METHOD(Canvas, scale), "Resizes it.");
            b.method("float get_alpha() const property", NX_METHOD(Canvas, alpha), "How opaque what is drawn next will be.");
            b.method("void set_alpha(float) property", NX_METHOD(Canvas, setAlpha), "Fades everything drawn next.");
            b.method("Blend get_blend() const property", NX_METHOD(Canvas, blend), "How what is drawn next combines with what is behind.");
            b.method("void set_blend(Blend) property", NX_METHOD(Canvas, setBlend), "Chooses how it combines.");
            b.method("Filter get_filter() const property", NX_METHOD(Canvas, filter), "How images that are drawn larger will look.");
            b.method("void set_filter(Filter) property", NX_METHOD(Canvas, setFilter), "Chooses how images are scaled.");
            b.method("void clip(float x, float y, float w, float h)", NX_METHOD(Canvas, clip), "Draws nothing outside an area. Clips within clips overlap.");
            b.method("void noClip()", NX_METHOD(Canvas, noClip), "Removes all clips.");
            b.method("void rect(float x, float y, float w, float h, const Color &in color)", NX_METHOD(Canvas, rect), "A filled rectangle.");
            b.method("void rectLine(float x, float y, float w, float h, float thickness, const Color &in color)", NX_METHOD(Canvas, rectLine), "A rectangle's outline.");
            b.method("void gradient(float x, float y, float w, float h, const Color &in top, const Color &in bottom)", NX_METHOD(Canvas, gradient), "A rectangle fading from one colour to another.");
            b.method("void roundRect(float x, float y, float w, float h, float radius, const Color &in color)", NX_METHOD(Canvas, roundRect), "A rectangle with round corners.");
            b.method("void line(float x1, float y1, float x2, float y2, float thickness, const Color &in color)", NX_METHOD(Canvas, line), "A straight line.");
            b.method("void circle(float x, float y, float radius, const Color &in color)", NX_METHOD(Canvas, circle), "A filled circle.");
            b.method("void ring(float x, float y, float radius, float thickness, const Color &in color)", NX_METHOD(Canvas, ring), "A circle's outline.");
            b.method("void arc(float x, float y, float radius, float thickness, float from, float to, const Color &in color)", NX_METHOD(Canvas, arc), "Part of a ring, or a slice of a circle, between two angles in degrees.");
            b.method("void glow(float x, float y, float radius, const Color &in color)", NX_METHOD(Canvas, glow), "A round glow, fading to nothing at its edge. With BlendAdd it is light.");
            b.method("void triangle(const Vec2 &in a, const Vec2 &in b, const Vec2 &in c, const Color &in ca, const Color &in cb, const Color &in cc)", NX_METHOD(Canvas, triangle), "A triangle with a colour at each corner.");
            b.method("bool effect(Effect@+ effect)", NX_OBJ_LAST(canvasEffect), "Puts all that has been drawn so far through a shader. False where there are none.");
            b.method("bool bloom(float strength = 0.8, float threshold = 0.7, float spread = 1)", NX_METHOD(Canvas, bloom), "Lets light spill from what is bright, of all that has been drawn so far. False where it can't be done.");
            b.method("bool grade(Look@+ look, float strength = 1)", NX_OBJ_LAST(canvasGrade), "Turns each colour of all that has been drawn so far into what a look makes of it, mixed with what it was by strength (0 to 1). Only inside the clip, if there is one, so a part can be shown before and after. One pass over the screen, on every GPU; the look is worked out again only when it changes. Usually last, before text that should stay as it is. False where it can't be done.");
            b.method("bool beginPicture(Texture@+ picture, const Color &in clear = Color(0, 0, 0, 0))", NX_OBJ_LAST(beginPicture), "Draws into a picture from Assets::picture instead of the screen, 2D and 3D alike, until endPicture: cleared to a colour first, and with a design of its own pixels until it is changed. One at a time; effects, bloom and grading are for the screen and do nothing in it. False if it can't be, and drawing goes on to the screen.");
            b.method("void endPicture()", NX_OBJ_LAST(endPicture), "Back to the screen, as it was.");
            b.method("void image(const Texture@+ texture, float x, float y, const Color &in tint = White)", NX_OBJ_LAST(drawImage), "An image at its own size.");
            b.method("void image(const Texture@+ texture, float x, float y, float w, float h, const Color &in tint = White)", NX_OBJ_LAST(drawImageSized), "An image at any size.");
            b.method("void imagePart(const Texture@+ texture, float x, float y, float w, float h, float sx, float sy, float sw, float sh, const Color &in tint = White)", NX_OBJ_LAST(drawImagePart), "Part of an image, given in its own pixels.");
            b.method("void sprite(const Texture@+ texture, uint columns, uint rows, uint frame, float x, float y, float w, float h, const Color &in tint = White)", NX_OBJ_LAST(drawSprite), "One frame from a sheet of equal frames.");
            b.method("void nineSlice(const Texture@+ texture, float x, float y, float w, float h, float corner, const Color &in tint = White)", NX_OBJ_LAST(drawNineSlice), "A stretchy panel whose corners keep their size.");
            b.method("void tile(const Texture@+ texture, float x, float y, float w, float h, float tileW, float tileH, float offsetX, float offsetY, const Color &in tint = White)", NX_OBJ_LAST(drawTile), "An image repeated over an area.");
            b.method("float text(const Font@+ font, const string &in text, float x, float y, const Color &in color = White, Align align = AlignLeft)", NX_OBJ_LAST(drawText), "Text from its top. Returns its width.");
            b.method("float paragraph(const Font@+ font, const string &in text, float x, float y, float width, const Color &in color = White)", NX_OBJ_LAST(drawParagraph), "Text broken into lines to fit a width. Returns its height.");
            b.method("float rich(const Font@+ font, const string &in text, float x, float y, float width, const Color &in color = White, Align align = AlignLeft)", NX_OBJ_LAST(drawRich), "A paragraph with parts of it marked: `[color=#ff8000]` or `[color=orange]` ... `[/color]`, `[b]` bold `[/b]`, `[u]` underlined `[/u]`, `[s]` struck through `[/s]`; `[[` is a [ itself. The colours are white, black, red, green, blue, yellow, orange, pink, purple, cyan and gray, or #rrggbb and #rrggbbaa. Returns its height.");
            b.method("float richHeight(const Font@+ font, const string &in text, float width)", NX_OBJ_LAST(richHeight), "How tall that would be, with nothing drawn.");
        }

        void bindInput(Binder& b)
        {
            b.section("Input", "Actions are `up`, `down`, `left`, `right`, `accept`, `cancel`, `fire`, `alternate` and `menu`, from a keyboard, mouse or gamepad.");
            b.names("Input");
            b.function("bool held(const string &in action)", NX_FN(inputHeld), "Whether it is down now.");
            b.function("bool pressed(const string &in action)", NX_FN(inputPressed), "Whether it has just gone down.");
            b.function("bool released(const string &in action)", NX_FN(inputReleased), "Whether it has just been let go.");
            b.function("bool repeated(const string &in action)", NX_FN(inputRepeated), "pressed(), then again at intervals while held.");
            b.function("Vec2 get_move() property", NX_FN(inputMove), "Direction from keys, d-pad and left stick: x right, y down, never longer than 1.");
            b.function("bool get_usingPad() property", NX_FN(inputUsingPad), "Whether a gamepad was used last.");
            b.function("bool get_hasMouse() property", NX_FN(inputHasMouse), "Whether a mouse has moved, or the stick moves a pointer.");
            b.function("void rumble(float strength, float seconds = 0.2f, int pad = -1)", NX_FN(inputRumble), "Shakes a gamepad: strength 0 to 1, for so long. Pad -1 is every pad. Another rumble for the same pad replaces this one; the engine's menu, and leaving the screen, stop it.");
            b.function("void rumbleMotors(float heavy, float light, float seconds, int pad = -1)", NX_FN(inputRumbleMotors), "The same, with each motor its own strength: the heavy one is a slow thud, the light one a quick buzz.");
            b.function("void stopRumble()", NX_FN(inputStopRumble), "Stops every pad.");
            b.function("bool get_rumbleOn() property", NX_FN(inputRumbleOn), "The player's choice: whether pads rumble at all. Kept with the settings.");
            b.function("void set_rumbleOn(bool) property", NX_FN(inputSetRumbleOn), "The player's choice: whether pads rumble at all. Kept with the settings.");
            b.function("bool get_stickPointer() property", NX_FN(inputStickPointer), "The player's choice: whether, on screens that want a pointer, the left stick moves one and A clicks with it, as a mouse would. Kept with the settings.");
            b.function("void set_stickPointer(bool) property", NX_FN(inputSetStickPointer), "The player's choice: whether, on screens that want a pointer, the left stick moves one and A clicks with it, as a mouse would. The stick and A then do nothing else, and a mouse that moves takes over. Kept with the settings.");
            b.function("bool get_wantsPointer() property", NX_FN(inputWantsPointer), "Whether this screen has a use for a pointer.");
            b.function("void set_wantsPointer(bool) property", NX_FN(inputSetWantsPointer), "Says this screen has a use for a pointer, such as clicking in a 3D view: then, if the player chose it, the stick moves one. Where nothing else shows a pointer, it is drawn. Off again when the screen closes.");
            b.function("Vec2 get_mouse() property", NX_FN(inputMouse), "Where the mouse is, in design units.");
            b.function("bool mouseDown(uint button)", NX_FN(inputMouseDown), "Left is 0, right 1, middle 2.");
            b.function("Vec2 get_mouseMove() property", NX_FN(inputTurn), "How far the mouse has just moved, in design units, even while it is locked.");
            b.function("void lockMouse(bool locked)", NX_FN(inputLock), "Hides the pointer and keeps it in the window, for looking about. It is let go when the window is left, and when the screen closes.");
            b.function("float get_wheel() property", NX_FN(inputWheel), "How far the mouse's wheel has just turned: up is more than 0.");
            b.function("bool key(const string &in name)", NX_FN(inputKey), "Whether a key is down, by its name: `A`, `1`, `Up`, `Space`, `Shift`, `Control`. Not while someone is typing. Actions are better for playing with, as they can be changed.");
            b.function("bool padConnected(uint pad)", NX_FN(padConnected), "Whether a gamepad (0 to 3) is plugged in.");
            b.function("Vec2 padLeft(uint pad)", NX_FN(padLeft), "Left stick, -1 to 1, y up.");
            b.function("Vec2 padRight(uint pad)", NX_FN(padRight), "Right stick, -1 to 1, y up.");
            b.function("Vec2 padTriggers(uint pad)", NX_FN(padTriggers), "Left and right triggers, 0 to 1.");
            b.function("bool padButton(uint pad, uint button)", NX_FN(padButton), "In order: up, down, left, right, Start, Back, left stick, right stick, A, B, X, Y, left and right shoulder.");
            b.function("void listen(const string &in action)", NX_FN(inputListen), "The next key or button pressed will do this. Escape or Start calls it off. Not for `menu`.");
            b.function("bool get_listening() property", NX_FN(inputListening), "Whether it is still waiting for one.");
            b.function("string keys(const string &in action)", NX_FN(inputKeys), "The keys for an action, as text.");
            b.function("string buttons(const string &in action)", NX_FN(inputButtons), "The gamepad buttons for an action, as text.");
            b.function("void resetBindings()", NX_FN(inputReset), "Puts them all back.");
            b.names("");
        }

        void bindAudio(Binder& b)
        {
            b.enumType("Bus", "Groups of sounds with a volume each.");
            b.enumValue("BusEffects", static_cast<int>(Bus::Effects), "Sounds in the game.");
            b.enumValue("BusMusic", static_cast<int>(Bus::Music), "Music.");
            b.enumValue("BusInterface", static_cast<int>(Bus::Interface), "Menus.");

            b.valueType("Voice", sizeof(PlayingSound), asOBJ_POD | asOBJ_APP_CLASS_ALLINTS | asGetTypeTraits<PlayingSound>(), "Something that is playing. Safe to use after it has ended.");
            b.constructor("void f()", NX_OBJ_LAST(makePlaying), "Nothing.");
            b.method("bool get_playing() const property", NX_METHOD(PlayingSound, playing), "Whether it still is.");
            b.method("float get_position() const property", NX_METHOD(PlayingSound, position), "Seconds in.");
            b.method("void stop(float fadeSeconds = 0)", NX_METHOD(PlayingSound, stop), "Ends it.");
            b.method("void fade(float volume, float seconds)", NX_METHOD(PlayingSound, setVolume), "Changes how loud it is, over a time.");
            b.method("void set_pan(float) property", NX_METHOD(PlayingSound, setPan), "From -1, left, to 1, right.");
            b.method("void set_pitch(float) property", NX_METHOD(PlayingSound, setPitch), "1 is normal; 2 is an octave up, and twice as fast.");
            b.method("void set_paused(bool) property", NX_METHOD(PlayingSound, setPaused), "Holds it where it is.");

            b.reopen("Sound");
            b.method("Voice play(float volume = 1, float pan = 0, float pitch = 1, bool loop = false)", NX_OBJ_LAST(soundPlay), "Plays it as an effect. Any number can play at once, up to the voices there are.");
            b.method("Voice playAt(const Vec3 &in place, float volume = 1, float range = 30, float pitch = 1)", NX_OBJ_LAST(soundPlayAt), "Plays it from a place in space: quieter farther from the camera, silent beyond a range, and from one side or the other.");
            b.method("Voice playMusic(float volume = 1, float fadeSeconds = 0, bool loop = true)", NX_OBJ_LAST(soundPlayMusic), "Plays it as music, fading in.");
            b.method("float get_seconds() const property", NX_OBJ_LAST(soundSeconds), "How long it is; 0 if that isn't known.");
            b.method("bool get_ready() const property", NX_METHOD(SoundRef, ready), "Whether it has arrived. Until then it plays as silence.");
            b.method("bool get_failed() const property", NX_METHOD(SoundRef, failed), "Whether the file could not be loaded.");

            b.section("Audio", "Volumes, and what is being heard. A screen's sounds pause while it is covered and stop when it closes.");
            b.names("Audio");
            b.function("void setVolume(Bus bus, float volume)", NX_FN(setBus), "0 to 1.");
            b.function("float volume(Bus bus)", NX_FN(getBus), "0 to 1.");
            b.function("void set_masterVolume(float) property", NX_FN(setMaster), "Everything, 0 to 1.");
            b.function("float get_masterVolume() property", NX_FN(getMaster), "0 to 1.");
            b.function("void lowPass(Bus bus, float hertz, float seconds = 0)", NX_FN(audioLowPass), "Takes away what is higher than so many hertz, as if heard under water or through a wall: 600 is muffled, 20000 is everything again. Glides there over so many seconds.");
            b.function("void highPass(Bus bus, float hertz, float seconds = 0)", NX_FN(audioHighPass), "Takes away what is lower, as a small radio or a telephone does: 1000 is thin, 0 is everything again.");
            b.function("void reverb(Bus bus, float amount, float room = 0.6f, float seconds = 0)", NX_FN(audioReverb), "A room's echoes: amount 0 to 1, how much is heard; room 0 to 1, from a small, dead one to a hall or a cave. 0 for none.");
            b.function("void echo(Bus bus, float amount, float delay = 0.3f, float feedback = 0.4f, float seconds = 0)", NX_FN(audioEcho), "Each sound again, after so many seconds (up to 0.75), and again, quieter each time by feedback (0 to 0.9). 0 for none.");
            b.function("void clearEffects(float seconds = 0)", NX_FN(audioClearEffects), "Every bus back to none. A screen starts with none, and they go when it closes.");
            b.function("float get_level() property", NX_FN(audioLevel), "How loud it has been lately, 0 to 1.");
            b.function("float spectrum(uint band)", NX_FN(audioSpectrum), "How strong one of 32 bands is, low to high, 0 to 1.");
            b.function("float wave(uint index)", NX_FN(audioWave), "One of the last 512 samples heard, -1 to 1.");
            b.function("uint get_voices() property", NX_FN(audioVoices), "How many are playing.");
            b.function("uint get_voiceLimit() property", NX_FN(audioVoiceLimit), "How many can.");
            b.function("string get_output() property", NX_FN(audioOutput), "What is playing it.");
            b.names("");
        }

        void bindCoroutines(Binder& b)
        {
            b.funcdef("void Coroutine()", "A function that can wait. It may be a method: `Coroutine(this.intro)`.");
            b.section("Coroutines", "Functions that run over many frames. They belong to their screen, pause with it and end with it.");
            b.function("void start(Coroutine@ function)", NX_FN(coStart), "Runs it from the next update, alongside everything else.");
            b.function("void wait(float seconds)", NX_FN(coWait), "Inside a coroutine: carries on after a time.");
            b.function("void nextUpdate()", NX_FN(coNext), "Inside a coroutine: carries on at the next update.");
            b.function("uint coroutines()", NX_FN(coCount), "How many are running.");
        }

        void bindSave(Binder& b)
        {
            b.section("Save", "Text that is kept between runs, by name.");
            b.names("Save");
            b.function("void set(const string &in key, const string &in value)", NX_FN(saveSet), "Keeps something.");
            b.function("string get(const string &in key, const string &in otherwise = \"\")", NX_FN(saveGet), "What was kept, or otherwise.");
            b.function("void remove(const string &in key)", NX_FN(saveRemove), "Forgets it.");
            b.names("");
        }

        void bindScreens(Binder& b)
        {
            b.interfaceType("Screen", "One place in the app. A screen's main.as has a class that implements this; it may have any of these methods.");
            b.callback("void onEnter()", "Called once, when it opens and everything it asked for has loaded.");
            b.callback("void update(float dt)", "Called 60 times a second. dt is in seconds.");
            b.callback("void draw(Canvas@ canvas)", "Called to draw each frame.");
            b.callback("void onSuspend()", "Called when another screen opens on top.");
            b.callback("void onResume()", "Called when that one closes again.");
            b.callback("void onExit()", "Called once, before it closes.");

            b.section("Screens", "Moving between screens. Changes happen after the current update.");
            b.names("Screens");
            b.function("void push(const string &in id)", NX_FN(screensPush), "Opens a screen on top of this one.");
            b.function("void pop()", NX_FN(screensPop), "Closes this screen and returns to the one below.");
            b.function("void replace(const string &in id)", NX_FN(screensReplace), "Closes this screen and opens another in its place.");
            b.function("void home()", NX_FN(screensHome), "Closes everything above the first screen.");
            b.function("uint get_count() property", NX_FN(screensCount), "How many screens can be opened.");
            b.function("string id(uint index)", NX_FN(screensId), "The name to pass to push().");
            b.function("string title(uint index)", NX_FN(screensTitle), "From its screen.json.");
            b.function("string description(uint index)", NX_FN(screensDescription), "From its screen.json.");
            b.function("string category(uint index)", NX_FN(screensCategory), "From its screen.json.");
            b.names("");
        }

        void bindApp(Binder& b)
        {
            b.section("App", "The machine and how it is doing.");
            b.names("App");
            b.function("float get_time() property", NX_FN(appTime), "Seconds since the app started.");
            b.function("double get_now() property", NX_FN(appNow), "Seconds from some moment, to a millionth or so: for timing how long something takes. Unlike `time`, it goes on during a frame.");
            b.function("float get_fps() property", NX_FN(appFps), "Frames a second, averaged.");
            b.function("uint get_draws() property", NX_FN(appDraws), "Draw calls in the last frame.");
            b.function("string get_platform() property", NX_FN(appPlatform), "What it is running on.");
            b.function("string get_gpu() property", NX_FN(appGpu), "What is drawing.");
            b.function("uint get_memoryKilobytes() property", NX_FN(appMemoryKilobytes), "Memory the engine has allocated, not counting video memory.");
            b.function("uint get_scriptKilobytes() property", NX_FN(appScriptKilobytes), "How much of that is scripts.");
            b.function("uint get_freeKilobytes() property", NX_FN(appFreeKilobytes), "Memory the machine has left.");

            b.function("bool get_development() property", NX_FN(appDevelopment), "Whether changed scripts and pictures are reloaded as they are saved, and Assets::write writes to the media: started with --dev, or with a folder called dev beside the title on a console, or turned on here.");
            b.function("void set_development(bool) property", NX_FN(appSetDevelopment), "Turns it on or off, as a title may for its own reasons, such as a hidden menu.");
            b.function("bool debugger(uint port = 4711, bool otherMachines = true)", NX_FN(appDebugger), "Lets a script debugger connect, as --debug does: from other machines too unless told not to, as a console needs. True if it is listening, already or now.");
            b.function("bool get_debugging() property", NX_FN(appDebugging), "Whether a debugger is connected.");
            b.function("uint get_reloads() property", NX_FN(appReloads), "How many times a script or image has been reloaded because its file changed.");

            b.names("");
            b.section("Vec2", "A place or a direction, across and down. Vec3 is the same with z.");
            b.callback("Vec2()", "Zero.");
            b.callback("Vec2(float x, float y)", "From its parts.");
            b.callback("float x", "Across.");
            b.callback("float y", "Down.");
            b.callback("Vec2 + Vec2, Vec2 - Vec2, -Vec2", "Added, taken away, turned round. += and -= too.");
            b.callback("Vec2 * float, Vec2 / float", "Longer or shorter. *= too.");
            b.callback("float dot(const Vec2 &in other) const", "How far one goes along the other.");
            b.section("Vec3", "A place or a direction in space: x right, y up, z towards you, in metres.");
            b.callback("Vec3()", "Zero.");
            b.callback("Vec3(float x, float y, float z)", "From its parts.");
            b.callback("Vec3 + Vec3, Vec3 - Vec3, -Vec3, Vec3 * float, Vec3 / float", "As for Vec2.");
            b.callback("float dot(const Vec3 &in other) const", "How far one goes along the other.");
            b.callback("Vec3 cross(const Vec3 &in other) const", "The direction across both.");
            b.callback("float length() const", "How long it is.");
            b.callback("Vec3 normalized() const", "The same direction, 1 long.");
            b.section("Maths", "As well as AngelScript's own `sin`, `cos`, `sqrt` and so on, in radians.");
            b.function("float random()", NX_FN(randomUnit), "From 0 up to 1.");
            b.function("float random(float low, float high)", NX_FN(randomRange), "Between two numbers.");
            b.function("int randomInt(int low, int high)", NX_FN(randomInt), "A whole number from low to high, including both.");
            b.function("float lerp(float a, float b, float t)", NX_FN(lerp), "From a to b as t goes from 0 to 1.");
            b.function("float clamp(float value, float low, float high)", NX_FN(clampFloat), "Kept within limits.");
            b.function("float min(float a, float b)", NX_FN(leastOf), "The smaller of two.");
            b.function("float max(float a, float b)", NX_FN(mostOf), "The larger of two.");
            b.function("float smooth(float t)", NX_FN(smooth), "0 to 1, starting and ending gently.");
            b.function("float bounce(float t)", NX_FN(bounce), "0 to 1, bouncing as it lands.");

            b.enumType("Ease", "Ways of getting from one value to another.");
            b.enumValue("EaseLinear", static_cast<int>(Ease::Linear), "Steadily.");
            b.enumValue("EaseSmooth", static_cast<int>(Ease::Smooth), "Starting and ending gently.");
            b.enumValue("EaseIn", static_cast<int>(Ease::In), "Starting gently.");
            b.enumValue("EaseOut", static_cast<int>(Ease::Out), "Ending gently.");
            b.enumValue("EaseBounce", static_cast<int>(Ease::Bounce), "Bouncing as it lands.");
            b.enumValue("EaseOvershoot", static_cast<int>(Ease::Overshoot), "Going past and coming back.");
            b.enumValue("EaseElastic", static_cast<int>(Ease::Elastic), "Springing.");
            b.valueType("Tween", sizeof(Tween), asOBJ_POD | asGetTypeTraits<Tween>(), "A number that moves by itself. There is nothing to update: just read its value.");
            b.constructor("void f()", NX_OBJ_LAST(makeTween), "Resting at 0.");
            b.constructor("void f(float value)", NX_OBJ_LAST(makeTweenAt), "Resting at a value.");
            b.constructor("void f(float from, float to, float seconds, Ease ease = EaseSmooth)", NX_OBJ_LAST(makeTweenFull), "Moving, from now.");
            b.method("float get_value() const property", NX_METHOD(Tween, value), "Where it is now.");
            b.method("bool get_done() const property", NX_METHOD(Tween, done), "Whether it has arrived.");
            b.method("void go(float to, float seconds, Ease ease = EaseSmooth)", NX_METHOD(Tween, go), "Moves somewhere else, from wherever it is.");
            b.function("float ease(Ease ease, float t)", NX_FN(ease), "The same curves, for your own use.");
        }
    }

    void loadSettings(Services& s)
    {
        s.input->loadBindings(s.save->get("bindings", ""));
        s.audio->setMasterVolume(static_cast<float>(atof(s.save->get("volume", "1"))));
        s.input->setStickPointer(strcmp(s.save->get("stickPointer", "0"), "1") == 0);
        s.input->setRumbleOn(strcmp(s.save->get("rumble", "1"), "1") == 0);
        if (s.net)
        {
            s.net->setMeetingPlace(s.save->get("meeting", ""));
        }
        for (u32 i = 0; i < static_cast<u32>(Bus::Count); i++)
        {
            char key[16];
            snprintf(key, sizeof(key), "volume%u", static_cast<unsigned>(i));
            s.audio->setBusVolume(static_cast<Bus>(i), static_cast<float>(atof(s.save->get(key, "1"))));
        }
    }

    void saveSettings(Services& s)
    {
        const auto volume = [&](const char* key, float value)
        {
            char text[16];
            snprintf(text, sizeof(text), "%.2f", static_cast<double>(value));
            s.save->set(key, text);
        };
        s.save->set("bindings", s.input->saveBindings().c_str());
        s.save->set("stickPointer", s.input->stickPointer() ? "1" : "0");
        s.save->set("rumble", s.input->rumbleOn() ? "1" : "0");
        if (s.net)
        {
            s.save->set("meeting", s.net->meetingPlace().c_str());
        }
        volume("volume", s.audio->masterVolume());
        for (u32 i = 0; i < static_cast<u32>(Bus::Count); i++)
        {
            char key[16];
            snprintf(key, sizeof(key), "volume%u", static_cast<unsigned>(i));
            volume(key, s.audio->busVolume(static_cast<Bus>(i)));
        }
    }

    bool bindEngine(Binder& binder, Services& s)
    {
        s.host->engine()->SetUserData(&s);
        bindColor(binder);
        bindAssets(binder);
        bindCanvas(binder);
        bindInput(binder);
        bindAudio(binder);
        bindCoroutines(binder);
        bindSave(binder);
        bindJson(binder);
        bindPath(binder);
        bindScene(binder);
        bindSpace(binder);
        bindUi(binder);
        bindNet(binder);
        bindXbox(binder);
        bindMachines(binder);
        binder.reopen("Entity");
        binder.method("Voice sound(Sound@+ sound, float volume = 1, float range = 30, bool loop = true)", NX_OBJ_LAST(entitySound), "Plays a sound from it, wherever it goes, until it is destroyed.");
        s.host->setPrelude(behaviourPrelude());
        s.entityArray = s.host->hold("array<Entity@>");
        s.host->hold("array<Widget@>");
        bindScreens(binder);
        bindApp(binder);
        return binder.ok();
    }
}
