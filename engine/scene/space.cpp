#include "scene/space.h"
#include "base/log.h"
#include "scene/world.h"

#include <algorithm>
#include <math.h>
#include <string.h>

namespace nx
{
    namespace
    {
        constexpr float kDegrees = 3.14159265f / 180.0f;
        constexpr gpu::Color kWhite = { 1, 1, 1, 1 };

        float largest(const Vec3& v)
        {
            return fmaxf(fabsf(v.x), fmaxf(fabsf(v.y), fabsf(v.z)));
        }

        // How much a matrix enlarges, at most.
        float enlarges(const Mat4& m)
        {
            const float x = m.m[0] * m.m[0] + m.m[1] * m.m[1] + m.m[2] * m.m[2];
            const float y = m.m[4] * m.m[4] + m.m[5] * m.m[5] + m.m[6] * m.m[6];
            const float z = m.m[8] * m.m[8] + m.m[9] * m.m[9] + m.m[10] * m.m[10];
            return sqrtf(fmaxf(x, fmaxf(y, z)));
        }
    }

    // --- Meshes that everyone uses ---

    bool SolidMeshes::make()
    {
        if (m_made || m_device == nullptr)
        {
            return m_made;
        }
        gpu::Device& device = *m_device;
        bool ok = true;
        for (u32 detail = 0; detail < 3; detail++)
        {
            static const u32 rounds[3] = { 28, 14, 8 };
            const u32 around = rounds[detail];
            for (u32 s = 1; s < static_cast<u32>(Solid::Count); s++)
            {
                MeshData d;
                switch (static_cast<Solid>(s))
                {
                case Solid::Box: d.box({}, { 0.5f, 0.5f, 0.5f }, kWhite); break;
                case Solid::Sphere: d.sphere({}, 0.5f, around, kWhite); break;
                case Solid::Cylinder: d.cylinder({ 0, -0.5f, 0 }, 1, 0.5f, 0.5f, around, kWhite); break;
                case Solid::Cone: d.cylinder({ 0, -0.5f, 0 }, 1, 0.5f, 0, around, kWhite); break;
                case Solid::Torus: d.torus(0.35f, 0.15f, around, kWhite); break;
                case Solid::Capsule: d.capsule(0.5f, 0.5f, around, kWhite); break;
                default: d.plane(0.5f, 0.5f, 1, kWhite); break;
                }
                m_meshes[s][detail] = device.createMesh(d.vertices.begin(), d.vertices.count(), d.indices.begin(), d.indices.count(), false);
                ok = ok && m_meshes[s][detail];
            }
        }
        MeshData square;
        square.quad({ -1, -1, 0 }, { 1, -1, 0 }, { 1, 1, 0 }, { -1, 1, 0 }, kWhite);
        m_square = device.createMesh(square.vertices.begin(), 4, square.indices.begin(), 6, false);
        // Seen from inside. Its colours are set by where it is drawn.
        MeshData dome;
        dome.sphere({}, 1, 20, kWhite);
        m_dome = device.createMesh(dome.vertices.begin(), dome.vertices.count(), dome.indices.begin(), dome.indices.count(), true);
        u8 soft[32 * 32 * 4];
        for (u32 i = 0; i < 32 * 32; i++)
        {
            const float dx = (static_cast<float>(i % 32) - 15.5f) / 15.0f, dy = (static_cast<float>(i / 32) - 15.5f) / 15.0f;
            const float left = 1 - sqrtf(dx * dx + dy * dy);
            soft[i * 4] = soft[i * 4 + 1] = soft[i * 4 + 2] = 255;
            soft[i * 4 + 3] = left > 0 ? static_cast<u8>(left * left * (3 - 2 * left) * 255) : 0;
        }
        m_soft = device.createTexture(gpu::rgbaTexture(soft, 32, 32));
        // Grey, a little lighter and darker, the same at each edge as at the other.
        constexpr u32 kGrain = 128;
        u8* grain = static_cast<u8*>(memAlloc(kGrain * kGrain * 4, MemTag::General));
        if (grain)
        {
            u32 random = 1234567;
            float level[kGrain];
            memset(level, 0, sizeof(level));
            for (u32 i = 0; i < kGrain * kGrain; i++)
            {
                const u32 x = i % kGrain, y = i / kGrain;
                random = random * 1664525u + 1013904223u;
                const float speck = static_cast<float>(random >> 24) / 255.0f - 0.5f;
                level[x] = level[x] * 0.6f + speck * 0.4f;
                const float wave = sinf(static_cast<float>(x) * 0.1963f + sinf(static_cast<float>(y) * 0.0982f) * 2) * sinf(static_cast<float>(y) * 0.1473f + sinf(static_cast<float>(x) * 0.0491f) * 3);
                const float v = 0.88f + level[x] * 0.28f + speck * 0.08f + wave * 0.05f;
                grain[i * 4] = grain[i * 4 + 1] = grain[i * 4 + 2] = static_cast<u8>(v < 0 ? 0 : v > 1 ? 255 : v * 255);
                grain[i * 4 + 3] = 255;
            }
            gpu::TextureData data = gpu::rgbaTexture(grain, kGrain, kGrain);
            data.mips = true;
            m_grain = device.createTexture(data);
            memFree(grain);
        }
        m_made = ok && m_grain && m_square && m_dome && m_soft;
        return m_made;
    }

    void SolidMeshes::shutdown()
    {
        if (m_device == nullptr)
        {
            return;
        }
        m_made = false;
        for (auto& solid : m_meshes)
        {
            for (gpu::Mesh& mesh : solid)
            {
                m_device->destroyMesh(mesh);
                mesh = {};
            }
        }
        m_device->destroyMesh(m_square);
        m_device->destroyMesh(m_dome);
        m_device->destroyTexture(m_soft);
        m_device->destroyTexture(m_grain);
        m_device = nullptr;
    }

    void CustomMesh::release()
    {
        if (--refs == 0)
        {
            if (device)
            {
                device->destroyMesh(mesh);
            }
            destroy(this);
        }
    }

    void CustomMesh::upload()
    {
        if (!changed || device == nullptr)
        {
            return;
        }
        changed = false;
        device->destroyMesh(mesh);
        mesh = {};
        if (data.indices.count() >= 3)
        {
            mesh = device->createMesh(data.vertices.begin(), data.vertices.count(), data.indices.begin(), data.indices.count(), false);
        }
        data.bounds(middle, radius);
    }

    // --- Components ---

    template <typename T>
    T* Space::get(GrowArray<T>& parts, u32 kind, u32 id)
    {
        const World::Entity* e = m_world.find(id);
        return e && e->parts3[kind] != kNone ? &parts[e->parts3[kind]] : nullptr;
    }

    template <typename T>
    T* Space::getOrAdd(GrowArray<T>& parts, u32 kind, u32 id, const T& fresh)
    {
        World::Entity* e = m_world.find(id);
        if (e == nullptr)
        {
            return nullptr;
        }
        u32& at = e->parts3[kind];
        if (at == kNone)
        {
            if (!parts.push(fresh))
            {
                return nullptr;
            }
            at = parts.count() - 1;
            parts[at].owner = id & 0xFFFF;
        }
        return &parts[at];
    }

    template <typename T>
    void Space::removePart(GrowArray<T>& parts, u32 kind, u32 index)
    {
        u32& at = m_world.m_entities[index].parts3[kind];
        if (at == kNone)
        {
            return;
        }
        parts.swapRemove(at);
        if (at < parts.count())
        {
            m_world.m_entities[parts[at].owner].parts3[kind] = at;
        }
        at = kNone;
    }

    Spatial* Space::spatial(u32 id)
    {
        Spatial fresh = {};
        fresh.turn = Quat();
        fresh.size = { 1, 1, 1 };
        Spatial* s = getOrAdd(m_spatials, 0, id, fresh);
        if (s)
        {
            transform(s->owner);
        }
        return s;
    }

    Spatial* Space::getSpatial(u32 id) { return get(m_spatials, 0, id); }

    Appearance* Space::appearance(u32 id)
    {
        Appearance fresh = {};
        fresh.color = kWhite;
        fresh.casts = true;
        fresh.animation = fresh.before = -1;
        fresh.speed = 1;
        spatial(id);
        return getOrAdd(m_appearances, 1, id, fresh);
    }

    Appearance* Space::getAppearance(u32 id) { return get(m_appearances, 1, id); }

    Light3* Space::light(u32 id)
    {
        Light3 fresh = {};
        fresh.range = 5;
        spatial(id);
        return getOrAdd(m_lights, 2, id, fresh);
    }

    Collider3* Space::collider(u32 id)
    {
        Collider3 fresh = {};
        fresh.half = { 0.5f, 0.5f, 0.5f };
        spatial(id);
        Collider3* c = getOrAdd(m_colliders, 3, id, fresh);
        if (c)
        {
            c->version = 0;  // whoever asks may be about to change it
        }
        return c;
    }

    Collider3* Space::getCollider(u32 id)
    {
        Collider3* c = get(m_colliders, 3, id);
        if (c)
        {
            c->version = 0;
        }
        return c;
    }

    Body3* Space::body(u32 id)
    {
        Body3 fresh = {};
        fresh.friction = 0.6f;
        fresh.ground = { 0, 1, 0 };
        spatial(id);
        return getOrAdd(m_bodies, 4, id, fresh);
    }

    Body3* Space::getBody(u32 id) { return get(m_bodies, 4, id); }

    void Space::freeAppearance(Appearance& a)
    {
        if (a.model)
        {
            m_assets.release(a.model);
        }
        if (a.texture)
        {
            m_assets.release(a.texture);
        }
        if (a.sprite)
        {
            m_assets.release(a.sprite);
            a.sprite = nullptr;
        }
        if (a.custom)
        {
            a.custom->release();
        }
        if (a.bent && m_solids)
        {
            m_solids->device()->destroyMesh(a.bent);
        }
        if (a.effect)
        {
            a.effect->release();
        }
        a.model = a.texture = nullptr;
        a.custom = nullptr;
        a.effect = nullptr;
        a.bent = {};
    }

    void Space::removed(u32 index)
    {
        const u32 look = m_world.m_entities[index].parts3[1];
        if (look != kNone)
        {
            freeAppearance(m_appearances[look]);
        }
        removePart(m_spatials, 0, index);
        removePart(m_appearances, 1, index);
        removePart(m_lights, 2, index);
        removePart(m_colliders, 3, index);
        removePart(m_bodies, 4, index);
        removePart(m_emitters, 5, index);
    }

    void Space::forget()
    {
        m_rigid.clear();
        m_touches.clear();
        m_touchesBefore.clear();
    }

    u32 Space::countGroup(u32 group) const
    {
        u32 count = 0;
        for (const Collider3& c : m_colliders)
        {
            count += c.group == group && visibleCollider(c);
        }
        return count;
    }

    void Space::clear()
    {
        freeCards();
        clearMists();
        clearVoxels();
        clearNav();
        for (const Waiting& w : m_waiting)
        {
            m_assets.release(w.sound);
        }
        m_waiting.clear();
        setSkyPicture(nullptr);
        clearEdits();
        forget();
        gpu::Device* device = m_solids ? m_solids->device() : nullptr;
        if (device)
        {
            device->destroyMesh(m_waterMesh);
            for (gpu::Mesh& m : m_particleMeshes)
            {
                device->destroyMesh(m);
                m = {};
            }
            for (gpu::Mesh& m : m_fieldMeshes)
            {
                device->destroyMesh(m);
                m = {};
            }
        }
        m_waterMesh = {};
        while (m_chunks.count())
        {
            removeChunk(m_chunks.count() - 1);
        }
        m_landOn = false;
        m_field.count = 0;
    }

    void Space::setModel(u32 id, Asset* model)
    {
        if (Appearance* a = appearance(id))
        {
            m_assets.addRef(model);
            if (a->model)
            {
                m_assets.release(a->model);
            }
            if (a->bent)
            {
                m_solids->device()->destroyMesh(a->bent);
                a->bent = {};
            }
            a->model = model;
            a->animation = a->before = -1;
        }
    }

    void Space::setTexture(u32 id, Asset* texture)
    {
        if (Appearance* a = appearance(id))
        {
            if (texture)
            {
                m_assets.addRef(texture);
            }
            if (a->texture)
            {
                m_assets.release(a->texture);
            }
            a->texture = texture;
        }
    }

    void Space::setCustom(u32 id, CustomMesh* mesh)
    {
        if (Appearance* a = appearance(id))
        {
            mesh->addRef();
            if (a->custom)
            {
                a->custom->release();
            }
            a->custom = mesh;
            a->solid = Solid::None;
        }
    }

    void Space::setSolid(u32 id, Solid solid)
    {
        if (Appearance* a = appearance(id))
        {
            a->solid = solid;
        }
    }

    void Space::setEffect(u32 id, EffectUse* effect)
    {
        if (Appearance* a = appearance(id))
        {
            if (effect)
            {
                effect->addRef();
            }
            if (a->effect)
            {
                a->effect->release();
            }
            a->effect = effect;
        }
    }

    bool Space::play(u32 id, const char* name, bool loop, float change)
    {
        Appearance* a = getAppearance(id);
        if (a == nullptr || a->model == nullptr || a->model->model == nullptr)
        {
            return false;
        }
        const int animation = a->model->model->findAnimation(name);
        if (animation < 0)
        {
            return false;
        }
        if (animation != a->animation || (!loop && a->time >= a->model->model->animations[static_cast<u32>(animation)].length))
        {
            a->before = a->animation;
            a->beforeTime = a->time;
            a->change = a->changeFor = a->animation >= 0 ? change : 0;
            a->animation = animation;
            a->time = 0;
        }
        a->loop = loop;
        return true;
    }

    void Space::pendingAnimation(u32 id, const char* name)
    {
        if (Appearance* a = getAppearance(id))
        {
            snprintf(a->waiting, sizeof(a->waiting), "%s", name);
        }
    }

    bool Space::played(u32 id) const
    {
        const Appearance* a = const_cast<Space*>(this)->getAppearance(id);
        return a == nullptr || a->model == nullptr || a->model->model == nullptr || a->animation < 0 || (!a->loop && a->time >= a->model->model->animations[static_cast<u32>(a->animation)].length);
    }

    void Space::transform(u32 index)
    {
        const World::Entity& e = m_world.m_entities[index];
        if (e.parts3[0] == kNone)
        {
            return;
        }
        Spatial& s = m_spatials[e.parts3[0]];
        const Spatial* parent = e.parent != kNone && m_world.m_entities[e.parent].parts3[0] != kNone ? &m_spatials[m_world.m_entities[e.parent].parts3[0]] : nullptr;
        const u32 above = parent ? parent->version : 0;
        const float now[10] = { s.place.x, s.place.y, s.place.z, s.turn.x, s.turn.y, s.turn.z, s.turn.w, s.size.x, s.size.y, s.size.z };
        if (s.version && s.parentVersion == above && memcmp(now, s.made, sizeof(now)) == 0)
        {
            return;
        }
        memcpy(s.made, now, sizeof(now));
        s.parentVersion = above;
        s.version = ++m_version;
        s.world = compose(s.place, s.turn, s.size);
        if (parent)
        {
            s.world = parent->world * s.world;
        }
    }

    Vec3 Space::worldPlace(u32 id) const
    {
        const World::Entity* e = m_world.find(id);
        if (e == nullptr || e->parts3[0] == kNone)
        {
            return {};
        }
        Vec3 p = m_spatials[e->parts3[0]].place;
        for (u32 at = e->parent; at != kNone; at = m_world.m_entities[at].parent)
        {
            const u32 part = m_world.m_entities[at].parts3[0];
            if (part != kNone)
            {
                const Spatial& up = m_spatials[part];
                p = up.place + up.turn.rotate({ p.x * up.size.x, p.y * up.size.y, p.z * up.size.z });
            }
        }
        return p;
    }

    // --- Every step ---

    void Space::update(float dt)
    {
        m_time += dt;
        listen();
        stream();
        animate(dt);
        follow(dt);
        walk(dt);
        physics(dt);
        emitParticles(dt);
        moveParticles(dt);
    }

    void Space::animate(float dt)
    {
        m_animFrame++;
        for (Appearance& a : m_appearances)
        {
            if (a.waiting[0] && a.model && a.model->model)
            {
                const int found = a.model->model->findAnimation(a.waiting);
                a.waiting[0] = 0;
                a.animation = found;
                a.time = 0;
                a.loop = true;
            }
            if (a.animation < 0 || a.model == nullptr || a.model->model == nullptr)
            {
                continue;
            }
            const ModelData& model = *a.model->model;
            const float length = model.animations[static_cast<u32>(a.animation)].length;
            a.time += dt * a.speed;
            if (a.loop && length > 0)
            {
                a.time = fmodf(a.time, length);
                a.time = a.time < 0 ? a.time + length : a.time;
            }
            else
            {
                a.time = a.time > length ? length : a.time;
            }
            if (a.change > 0)
            {
                a.change -= dt;
                a.beforeTime += dt * a.speed;
            }
            // Bent again every frame when near; further away, every second or fourth, a few at a time.
            const u32 every = a.detail == 0 ? 1 : a.detail == 1 ? 2 : 4;
            a.bentReady = a.bentReady && (m_animFrame + a.owner) % every != 0;
        }
    }

    // --- Seeing ---

    bool Space::Frustum::sees(const Vec3& m, float radius) const
    {
        for (const auto& p : planes)
        {
            if (p[0] * m.x + p[1] * m.y + p[2] * m.z + p[3] < -radius)
            {
                return false;
            }
        }
        return true;
    }

    bool Space::toScreen(const Vec3& place, Vec2& out) const
    {
        const Mat4 m = m_lastView.projection * m_lastView.view;
        const float w = m.m[3] * place.x + m.m[7] * place.y + m.m[11] * place.z + m.m[15];
        const Vec3 p = m.transformPoint(place);
        if (w <= 0.001f)
        {
            return false;
        }
        out = { m_lastAt.x + (p.x / w * 0.5f + 0.5f) * m_lastSize.x, m_lastAt.y + (0.5f - p.y / w * 0.5f) * m_lastSize.y };
        return true;
    }

    Vec3 Space::rayThrough(const Vec2& screen) const
    {
        const float x = m_lastSize.x > 0 ? (screen.x - m_lastAt.x) / m_lastSize.x * 2 - 1 : 0;
        const float y = m_lastSize.y > 0 ? 1 - (screen.y - m_lastAt.y) / m_lastSize.y * 2 : 0;
        return (m_lastView.forward + m_lastView.right * x + m_lastView.up * y).normalized();
    }

    void Space::boundsOf(const Appearance& a, const Mat4& world, Vec3& middle, float& radius) const
    {
        Vec3 local;
        radius = 0.87f;
        if (a.model)
        {
            radius = a.model->model ? a.model->model->radius : 0;
            local = a.model->model ? a.model->model->middle : Vec3();
        }
        else if (a.custom)
        {
            radius = a.custom->radius;
            local = a.custom->middle;
        }
        middle = world.transformPoint(local);
        radius *= enlarges(world);
    }

    // Each point of the model, where its bones have put it.
    bool Space::bend(Appearance& a)
    {
        const ModelData& model = *a.model->model;
        gpu::Device& device = *m_solids->device();
        if (a.bentReady && a.bent)
        {
            return true;
        }
        const u32 count = model.weights.count();
        gpu::MeshVertex* moved = static_cast<gpu::MeshVertex*>(memAlloc(count * sizeof(gpu::MeshVertex), MemTag::Frame));
        if (moved == nullptr)
        {
            return static_cast<bool>(a.bent);
        }
        Mat4 bones[kModelBones];
        if (a.change > 0)
        {
            model.pose(a.before, a.beforeTime, a.animation, a.time, 1 - a.change / a.changeFor, bones);
        }
        else
        {
            model.pose(a.animation, a.time, bones);
        }
        model.bend(bones, moved);
        if (a.bent)
        {
            device.updateMesh(a.bent, moved, count);
        }
        else
        {
            a.bent = device.createMesh(moved, count, model.mesh.indices.begin(), model.mesh.indices.count(), true);
        }
        memFree(moved);
        a.bentReady = true;
        return static_cast<bool>(a.bent);
    }

    void Space::pickLights(const gpu::View& view, const Vec3& middle, float radius, gpu::MeshDraw& draw) const
    {
        // The strongest few that reach it.
        float strengths[gpu::kMeshLights] = {};
        for (u32 i = 0; i < view.lightCount; i++)
        {
            const gpu::PointLight& l = view.lights[i];
            const Vec3 d = l.place - middle;
            const float far = sqrtf(d.dot(d)) - radius;
            if (far >= l.range)
            {
                continue;
            }
            const float strength = (1 - (far > 0 ? far : 0) / l.range) * (l.color.r + l.color.g + l.color.b);
            u32 slot = draw.lightCount;
            if (slot == gpu::kMeshLights)
            {
                for (u32 k = 0; k < gpu::kMeshLights; k++)
                {
                    slot = slot == gpu::kMeshLights || strengths[k] < strengths[slot] ? k : slot;
                }
                if (strengths[slot] >= strength)
                {
                    continue;
                }
            }
            else
            {
                draw.lightCount++;
            }
            strengths[slot] = strength;
            draw.lights[slot] = static_cast<u8>(i);
        }
    }

    // Either what is solid, or what can be seen through.
    void Space::drawAppearance(const Appearance& a, u32 detail, const gpu::View& view, bool shadow, bool see)
    {
        gpu::Device& device = *m_solids->device();
        const Mat4& world = m_spatials[m_world.m_entities[a.owner].parts3[0]].world;
        gpu::MeshDraw draw;
        draw.world = world;
        if (a.sway != 0 && (m_wind.x != 0 || m_wind.z != 0))
        {
            // Leant downwind, more the higher up: the base stays where it is. Gusts roll along the way the wind
            // blows, so a wood moves as a wave, and a sheared matrix needs nothing of the device.
            const float along = (world.m[12] * m_wind.x + world.m[14] * m_wind.z) * 0.12f;
            const float t = view.time;
            const float gust = (sinf(t * 1.3f - along) * 0.55f + sinf(t * 2.9f - along * 1.7f + world.m[12]) * 0.2f + 0.35f) * a.sway;
            const float kx = gust * m_wind.x, kz = gust * m_wind.z;
            for (u32 c = 0; c < 3; c++)
            {
                draw.world.m[c * 4 + 0] += kx * world.m[c * 4 + 1];
                draw.world.m[c * 4 + 2] += kz * world.m[c * 4 + 1];
            }
        }
        draw.color = a.color;
        draw.glow = a.glow;
        draw.shine = a.shine;
        draw.blend = a.blend;
        draw.depthWrite = a.blend == gpu::Blend::None;
        draw.cull = a.twoSided ? gpu::Cull::None : gpu::Cull::Back;
        draw.repeat = a.repeat;
        draw.lines = m_lines && !shadow;
        if (a.effect && a.effect->works())
        {
            draw.effect = a.effect->asset->effect;
            draw.values = a.effect->values;
        }
        const bool ownTexture = a.texture && a.texture->ready && !a.texture->failed;
        if (ownTexture)
        {
            draw.texture = a.texture->texture;
        }
        if (!shadow)
        {
            Vec3 middle;
            float radius;
            boundsOf(a, world, middle, radius);
            pickLights(view, middle, radius, draw);
        }
        const bool through = a.blend != gpu::Blend::None;
        if (a.model)
        {
            const ModelData* model = a.model->model;
            if (model == nullptr)
            {
                return;
            }
            draw.mesh = model->weights.count() ? a.bent : model->gpuMesh;
            if (!draw.mesh)
            {
                return;
            }
            for (const ModelPart& part : model->parts)
            {
                if ((part.see || through) != see)
                {
                    continue;
                }
                gpu::MeshDraw d = draw;
                d.first = part.first[detail];
                d.count = part.count[detail];
                d.color = { a.color.r * part.color.r, a.color.g * part.color.g, a.color.b * part.color.b, a.color.a * part.color.a };
                d.glow = part.glow > a.glow ? part.glow : a.glow;
                d.indoors = part.indoors;
                d.cull = part.twoSided ? gpu::Cull::None : draw.cull;
                if (part.see)
                {
                    d.blend = a.blend == gpu::Blend::None ? gpu::Blend::Alpha : a.blend;
                    d.depthWrite = false;
                }
                if (!ownTexture && part.texture >= 0)
                {
                    d.texture = model->textures[static_cast<u32>(part.texture)].texture;
                    d.repeat = true;
                }
                device.drawMesh(d);
            }
            return;
        }
        if (through != see)
        {
            return;
        }
        draw.mesh = a.custom ? a.custom->mesh : a.solid != Solid::None ? m_solids->get(a.solid, detail) : gpu::Mesh();
        if (draw.mesh)
        {
            device.drawMesh(draw);
        }
    }

    void Space::draw(Canvas& canvas, float x, float y, float w, float h)
    {
        if (m_solids == nullptr || !m_solids->make())
        {
            return;
        }
        canvas.finish();
        gpu::Device& device = *m_solids->device();
        if (w <= 0 || h <= 0)
        {
            x = y = 0;
            w = canvas.width();
            h = canvas.height();
        }
        m_lastAt = { x, y };
        m_lastSize = { w, h };
        gpu::View view;
        view.area = canvas.toPixels(x, y, w, h);
        if (view.area.width <= 0 || view.area.height <= 0)
        {
            return;
        }
        const float aspect = static_cast<float>(view.area.width) / static_cast<float>(view.area.height);
        const float angle = m_angle * kDegrees;
        view.view = Mat4::lookAt(m_from, m_to, { 0, 1, 0 });
        view.projection = Mat4::perspective(angle, aspect, 0.1f, m_far);
        view.eye = m_from;
        const float tall = tanf(angle * 0.5f);
        view.right = Vec3(view.view.m[0], view.view.m[4], view.view.m[8]) * (tall * aspect);
        view.up = Vec3(view.view.m[1], view.view.m[5], view.view.m[9]) * tall;
        view.forward = Vec3(-view.view.m[2], -view.view.m[6], -view.view.m[10]);
        view.sunDirection = m_sun;
        view.sunColor = m_sunColor;
        view.ambientSky = m_ambientSky;
        view.ambientGround = m_ambientGround;
        view.fogColor = m_fog;
        view.fogStart = m_fogStart;
        view.fogEnd = m_fogEnd;
        view.skyTop = m_skyTop;
        view.skyHorizon = m_skyHorizon;
        view.time = m_time;

        // The nearest lights, if there are too many.
        m_items.clear();
        for (u32 i = 0; i < m_lights.count(); i++)
        {
            const World::Entity& e = m_world.m_entities[m_lights[i].owner];
            if (e.shown && !e.dying && e.parts3[0] != kNone)
            {
                const Mat4& at = m_spatials[e.parts3[0]].world;
                const Vec3 d = Vec3(at.m[12], at.m[13], at.m[14]) - m_from;
                m_items.push({ d.dot(d), i });
            }
        }
        std::sort(m_items.begin(), m_items.end(), [](const Item& a, const Item& b) { return a.key < b.key; });
        for (u32 i = 0; i < m_items.count() && view.lightCount < gpu::kLights; i++)
        {
            const Light3& l = m_lights[m_items[i].index];
            const Mat4& at = m_spatials[m_world.m_entities[l.owner].parts3[0]].world;
            gpu::PointLight& out = view.lights[view.lightCount++];
            out.place = { at.m[12], at.m[13], at.m[14] };
            out.range = l.range;
            out.color = l.color;
        }

        const Mat4 both = view.projection * view.view;
        Frustum frustum;
        for (u32 i = 0; i < 6; i++)
        {
            // Clip space is -w to w across and down, and 0 to w deep.
            const u32 row = i / 2;
            const float sign = i % 2 ? -1.0f : 1.0f;
            for (u32 c = 0; c < 4; c++)
            {
                frustum.planes[i][c] = (i == 4 ? 0.0f : both.m[c * 4 + 3]) + sign * both.m[c * 4 + row];
            }
            float* p = frustum.planes[i];
            const float length = sqrtf(p[0] * p[0] + p[1] * p[1] + p[2] * p[2]);
            for (u32 c = 0; c < 4 && length > 0; c++)
            {
                p[c] /= length;
            }
        }

        // What can be seen, at what level of detail. Custom meshes go to the GPU here.
        m_stats.drawn = m_stats.hidden = 0;
        memset(m_stats.details, 0, sizeof(m_stats.details));
        m_items.clear();
        for (u32 i = 0; i < m_appearances.count(); i++)
        {
            Appearance& a = m_appearances[i];
            const World::Entity& e = m_world.m_entities[a.owner];
            if (a.custom)
            {
                a.custom->upload();
            }
            if (!e.shown || e.dying || e.parts3[0] == kNone || a.color.a <= 0)
            {
                continue;
            }
            const Mat4& placed = m_spatials[e.parts3[0]].world;
            if (a.sprite)
            {
                // A picture: about as big as it is said to be.
                const Vec3 up = { placed.m[4], placed.m[5], placed.m[6] }, across = { placed.m[0], placed.m[1], placed.m[2] };
                const float high = sqrtf(up.dot(up)), wide = sqrtf(across.dot(across));
                if (frustum.sees(Vec3(placed.m[12], placed.m[13], placed.m[14]) + up * 0.5f, high + wide) && card(a, placed, view))
                {
                    m_stats.drawn++;
                }
                continue;
            }
            Vec3 middle;
            float radius;
            boundsOf(a, placed, middle, radius);
            const Vec3 d = middle - m_from;
            const float far = sqrtf(d.dot(d));
            const float size = radius / (fmaxf(far - radius, 0.1f) * tall);  // of half the picture
            const bool seen = frustum.sees(middle, radius) && size > 0.004f;
            // With some give, so that it doesn't change its mind every frame.
            static const float enough[2] = { 0.16f, 0.05f };
            u32 detail = a.detail;
            while (detail > 0 && size > enough[detail - 1] * 1.15f)
            {
                detail--;
            }
            while (detail < 2 && size < enough[detail] * 0.87f)
            {
                detail++;
            }
            a.detail = static_cast<u8>(detail);
            m_stats.hidden += !seen;
            if (seen && a.impostor && m_impostorFar > 0 && far > m_impostorFar && card(a, placed, view))
            {
                m_stats.drawn++;
                continue;
            }
            if (seen)
            {
                if (a.model && a.model->model && a.model->model->weights.count())
                {
                    bend(a);
                }
                m_stats.drawn++;
                m_stats.details[detail]++;
                m_items.push({ far, i });
            }
        }
        std::sort(m_items.begin(), m_items.end(), [](const Item& a, const Item& b) { return a.key < b.key; });

        // Shadows, around what the camera is looking at.
        if (m_shadowStrength > 0)
        {
            const float reach = m_shadowDistance;
            const Vec3 focus = m_from + view.forward * (reach * 0.8f);
            Mat4 look = Mat4::lookAt(focus + m_sun * (reach * 3), focus, { 0, 1, 0 });
            // In whole steps of the map, so that edges don't swim as the camera moves.
            const float step = 2 * reach / static_cast<float>(device.shadowSize());
            look.m[12] = floorf(look.m[12] / step) * step;
            look.m[13] = floorf(look.m[13] / step) * step;
            // Depth grows away from the light, which looks along -z.
            Mat4 flat;
            flat.m[0] = 1 / reach;
            flat.m[5] = 1 / reach;
            flat.m[10] = -1 / (reach * 6);
            view.shadows = flat * look;
            if (device.beginShadows(view.shadows))
            {
                view.shadowStrength = m_shadowStrength;
                // Where that is dear, only things cast them, as simply as they can be drawn.
                const bool dear = device.shadowSize() < gpu::kShadowSize;
                if (!dear)
                {
                    drawLand(view, frustum, true);
                    drawVoxels(view, frustum, true);
                }
                for (Appearance& a : m_appearances)
                {
                    const World::Entity& e = m_world.m_entities[a.owner];
                    if (!a.casts || a.glowBall || a.sprite || !e.shown || e.dying || e.parts3[0] == kNone || a.blend != gpu::Blend::None)
                    {
                        continue;
                    }
                    Vec3 middle;
                    float radius;
                    boundsOf(a, m_spatials[e.parts3[0]].world, middle, radius);
                    const Vec3 d = middle - focus;
                    const float within = reach * (dear ? 1.0f : 1.5f) + radius;
                    if (d.dot(d) < within * within && (!dear || radius > reach * 0.012f))
                    {
                        if (a.model && a.model->model && a.model->model->weights.count())
                        {
                            bend(a);
                        }
                        drawAppearance(a, dear ? 2 : a.detail, view, true, false);
                    }
                }
                device.endShadows();
            }
        }

        m_lastView = view;
        device.beginView(view);
        if (m_hasSky && !device.drawSky())
        {
            // A dome around the camera, coloured by how high each part of it is.
            const gpu::Color colors[3] = { m_skyTop, m_skyHorizon, m_sunColor };
            MeshData dome;
            const bool stale = memcmp(colors, m_domeColors, sizeof(colors)) != 0 || !(m_domeSun == m_sun) || m_solids->domeOwner() != this;
            if (stale)
            {
                dome.sphere({}, 1, 20, kWhite);
                memcpy(m_domeColors, colors, sizeof(colors));
                m_domeSun = m_sun;
                m_solids->setDomeOwner(this);
            }
            for (gpu::MeshVertex& v : dome.vertices)
            {
                const float above = fmaxf(v.y + 0.08f, 0), up = 1 - expf(-3.6f * above * above / (above + 0.2f));
                const float sun = powf(fmaxf(0, Vec3(v.x, v.y, v.z).dot(m_sun)), 24) * 0.2f;
                const gpu::MeshVertex c = meshVertex({}, {}, { m_skyHorizon.r + (m_skyTop.r - m_skyHorizon.r) * up + m_sunColor.r * sun, m_skyHorizon.g + (m_skyTop.g - m_skyHorizon.g) * up + m_sunColor.g * sun,
                    m_skyHorizon.b + (m_skyTop.b - m_skyHorizon.b) * up + m_sunColor.b * sun, 1 }, 0, 0);
                v.r = c.r;
                v.g = c.g;
                v.b = c.b;
            }
            if (stale)
            {
                device.updateMesh(m_solids->dome(), dome.vertices.begin(), dome.vertices.count());
            }
            gpu::MeshDraw d;
            d.mesh = m_solids->dome();
            d.world = compose(m_from, Quat(), Vec3(1, 1, 1) * (m_far * 0.5f));
            d.cull = gpu::Cull::None;
            d.depthWrite = d.lit = d.fogged = false;
            device.drawMesh(d);
        }
        if (m_skyPicture && m_skyPicture->ready && !m_skyPicture->failed)
        {
            gpu::MeshDraw d;
            d.mesh = m_solids->get(Solid::Sphere, 0);
            d.texture = m_skyPicture->texture;
            d.repeat = true;
            // From the inside, which turns it round.
            d.world = compose(m_from, Quat(), Vec3(-m_far, m_far, m_far) * 0.9f);
            d.cull = gpu::Cull::None;
            d.depthWrite = d.lit = d.fogged = false;
            device.drawMesh(d);
        }
        drawLand(view, frustum, false);
        drawVoxels(view, frustum, false);
        drawGrass(view, frustum);
        for (const Item& item : m_items)
        {
            const Appearance& a = m_appearances[item.index];
            if (!a.glowBall)
            {
                drawAppearance(a, a.detail, view, false, false);
            }
        }
        drawCards(view);
        if (m_field.count && !device.drawField(m_field))
        {
            // As triangles, made afresh as it moves. Planes go on for ever, so they are drawn as they are.
            gpu::Field solid;
            solid.repeat = m_field.repeat;
            for (u32 i = 0; i < m_field.count; i++)
            {
                const gpu::FieldShape& s = m_field.shapes[i];
                if (s.kind == gpu::FieldKind::Plane && s.operation == gpu::FieldOperation::Join)
                {
                    gpu::MeshDraw d;
                    d.mesh = m_solids->get(Solid::Plane, 0);
                    d.world = compose({ m_from.x, s.place.y, m_from.z }, Quat(), Vec3(1, 1, 1) * (m_field.farthest * 2));
                    d.color = s.color;
                    device.drawMesh(d);
                }
                else
                {
                    solid.shapes[solid.count++] = s;
                }
            }
            if (solid.count && (m_time - m_fieldMade > 0.045f || m_fieldMade < 0))
            {
                m_fieldMade = m_time;
                Vec3 low(1e6f, 1e6f, 1e6f), high(-1e6f, -1e6f, -1e6f);
                for (u32 i = 0; i < solid.count; i++)
                {
                    const gpu::FieldShape& s = solid.shapes[i];
                    if (s.operation != gpu::FieldOperation::Join)
                    {
                        continue;
                    }
                    Vec3 reach = Vec3(1, 1, 1) * s.size.x;
                    if (s.kind == gpu::FieldKind::Box)
                    {
                        const float across = sqrtf(s.size.x * s.size.x + s.size.z * s.size.z);
                        reach = { across, s.size.y, across };
                    }
                    else if (s.kind == gpu::FieldKind::Torus)
                    {
                        reach = { s.size.x + s.size.y, s.size.y, s.size.x + s.size.y };
                    }
                    else if (s.kind != gpu::FieldKind::Sphere)
                    {
                        reach.y = s.kind == gpu::FieldKind::Capsule ? s.size.x + s.size.y : s.size.y;
                    }
                    reach = reach + Vec3(1, 1, 1) * (s.soft * 0.3f + 0.08f);
                    low = { fminf(low.x, s.place.x - reach.x), fminf(low.y, s.place.y - reach.y), fminf(low.z, s.place.z - reach.z) };
                    high = { fmaxf(high.x, s.place.x + reach.x), fmaxf(high.y, s.place.y + reach.y), fmaxf(high.z, s.place.z + reach.z) };
                }
                MeshData& made = m_fieldData;
                fieldMesh(solid, (low + high) * 0.5f, largest(high - low) * 0.5f, 24, made);
                m_fieldLatest = (m_fieldLatest + 1) % 4;
                device.destroyMesh(m_fieldMeshes[m_fieldLatest]);
                m_fieldMeshes[m_fieldLatest] = made.indices.count() ? device.createMesh(made.vertices.begin(), made.vertices.count(), made.indices.begin(), made.indices.count(), false) : gpu::Mesh();
            }
            if (m_fieldMeshes[m_fieldLatest])
            {
                gpu::MeshDraw d;
                d.mesh = m_fieldMeshes[m_fieldLatest];
                d.shine = m_field.shine;
                d.cull = gpu::Cull::None;
                device.drawMesh(d);
                // Once more to each side, if it goes on and on.
                for (int along = -2; m_field.repeat > 0 && along <= 2; along++)
                {
                    for (int side = -2; side <= 2; side++)
                    {
                        if (side || along)
                        {
                            d.world = Mat4::translation({ static_cast<float>(side) * m_field.repeat, 0, static_cast<float>(along) * m_field.repeat });
                            device.drawMesh(d);
                        }
                    }
                }
            }
        }
        drawWater(view);
        drawMists(view, frustum);
        if (view.shadowStrength <= 0 && m_shadowStrength > 0)
        {
            drawBlobs(view, frustum);
        }
        // What can be seen through, from far to near.
        for (u32 i = m_items.count(); i-- > 0;)
        {
            const Appearance& a = m_appearances[m_items[i].index];
            if (a.glowBall)
            {
                const Mat4& at = m_spatials[m_world.m_entities[a.owner].parts3[0]].world;
                gpu::MeshDraw d;
                d.mesh = m_solids->square();
                const float size = enlarges(at) * 0.5f;
                const Vec3 rx = view.right.normalized() * size, ry = view.up.normalized() * size, rz = view.forward * -size;
                d.world.m[0] = rx.x;
                d.world.m[1] = rx.y;
                d.world.m[2] = rx.z;
                d.world.m[4] = ry.x;
                d.world.m[5] = ry.y;
                d.world.m[6] = ry.z;
                d.world.m[8] = rz.x;
                d.world.m[9] = rz.y;
                d.world.m[10] = rz.z;
                d.world.m[12] = at.m[12];
                d.world.m[13] = at.m[13];
                d.world.m[14] = at.m[14];
                d.texture = m_solids->soft();
                d.color = a.color;
                d.blend = gpu::Blend::Additive;
                d.depthWrite = d.lit = d.fogged = false;
                device.drawMesh(d);
            }
            else
            {
                drawAppearance(a, a.detail, view, false, true);
            }
        }
        drawParticles(view);
    }

    // --- Particles ---

    bool Space::burst(const char* name, const Vec3& place, u32 count, const Vec3& aim)
    {
        const u32 index = m_world.styleNamed(name);
        if (index == kNone)
        {
            return false;
        }
        const ParticleStyle& s = m_world.style(index);
        const Vec3 up = aim.length() > 0.0001f ? aim.normalized() : Vec3(0, 1, 0);
        const Vec3 side = (fabsf(up.x) > 0.7f ? Vec3(0, 1, 0) : Vec3(1, 0, 0)).cross(up).normalized();
        const Vec3 along = up.cross(side);
        const float widest = cosf(fminf(s.spread, 360) * 0.5f * kDegrees);
        for (u32 i = 0; i < count && m_particleCount + m_world.m_particleCount < m_world.m_maxParticles; i++)
        {
            Particle3 p;
            // Evenly over the cap of a ball.
            const float height = m_world.random(widest, 1), angle = m_world.random(0, 6.2831853f);
            const float out = sqrtf(fmaxf(0, 1 - height * height));
            p.velocity = (up * height + side * (out * cosf(angle)) + along * (out * sinf(angle))) * m_world.random(s.speedMin, s.speedMax);
            p.place = place;
            if (s.radius > 0)
            {
                const float a = m_world.random(0, 6.2831853f), away = sqrtf(m_world.random(0, 1)) * s.radius;
                p.place = p.place + side * (cosf(a) * away) + along * (sinf(a) * away);
            }
            p.age = 0;
            p.life = m_world.random(s.lifeMin, s.lifeMax);
            p.turn = s.spin != 0 ? m_world.random(0, 6.2831853f) : 0;
            p.spin = m_world.random(-s.spin, s.spin) * kDegrees;
            if (!m_particles[index].push(p))
            {
                break;
            }
            m_particleCount++;
        }
        return true;
    }

    bool Space::emit(u32 id, const char* name, float rate)
    {
        const u32 index = m_world.styleNamed(name);
        if (index == kNone)
        {
            return false;
        }
        Emitter3 fresh = {};
        spatial(id);
        if (Emitter3* e = getOrAdd(m_emitters, 5, id, fresh))
        {
            e->style = index;
            e->rate = rate < 0 ? 0 : rate;
        }
        return true;
    }

    void Space::clearParticles()
    {
        for (auto& particles : m_particles)
        {
            particles.clear();
        }
        m_particleCount = 0;
    }

    void Space::emitParticles(float dt)
    {
        for (u32 i = 0; i < m_emitters.count(); i++)
        {
            Emitter3& e = m_emitters[i];
            const World::Entity& owner = m_world.m_entities[e.owner];
            if (owner.dying || !owner.shown || owner.parts3[0] == kNone)
            {
                continue;
            }
            e.owed += e.rate * dt;
            const u32 count = static_cast<u32>(e.owed);
            e.owed -= static_cast<float>(count);
            if (count)
            {
                const Mat4& at = m_spatials[owner.parts3[0]].world;
                burst(m_world.style(e.style).name, { at.m[12], at.m[13], at.m[14] }, count, { at.m[4], at.m[5], at.m[6] });
            }
        }
    }

    void Space::moveParticles(float dt)
    {
        for (u32 s = 0; s < 16; s++)
        {
            GrowArray<Particle3>& particles = m_particles[s];
            if (particles.count() == 0)
            {
                continue;
            }
            const ParticleStyle& style = m_world.style(s);
            const Vec3 pull = m_gravity * (style.gravity * dt);
            const float kept = fmaxf(0, 1 - style.drag * dt);
            for (u32 i = 0; i < particles.count();)
            {
                Particle3& p = particles[i];
                p.age += dt;
                if (p.age >= p.life)
                {
                    particles.swapRemove(i);
                    m_particleCount--;
                    continue;
                }
                p.velocity = (p.velocity + pull) * kept;
                p.place = p.place + p.velocity * dt;
                p.turn += p.spin * dt;
                i++;
            }
        }
    }

    // Squares that face the camera: those that cover, from far to near, and then those that add light.
    void Space::drawParticles(const gpu::View& view)
    {
        if (m_particleCount == 0)
        {
            return;
        }
        gpu::Device& device = *m_solids->device();
        constexpr u32 kMost = 4096;
        MeshData& data = m_particleData;
        if (data.indices.count() == 0)
        {
            // The corners never change which is joined to which.
            for (u32 i = 0; i < kMost; i++)
            {
                for (u32 k = 0; k < 4; k++)
                {
                    data.vertex({}, { 0, 1, 0 }, kWhite, k == 1 || k == 2 ? 1.0f : 0.0f, k >= 2 ? 0.0f : 1.0f);
                }
                data.triangle(i * 4, i * 4 + 1, i * 4 + 2);
                data.triangle(i * 4, i * 4 + 2, i * 4 + 3);
            }
        }
        const Vec3 right = view.right.normalized(), up = view.up.normalized(), ahead = view.forward;
        struct Run
        {
            u32 style, first, count;
        };
        Run runs[16];
        u32 runCount = 0, used = 0;
        const gpu::Color day = { fminf(1, m_ambientSky.r * 0.7f + m_sunColor.r * 0.5f), fminf(1, m_ambientSky.g * 0.7f + m_sunColor.g * 0.5f), fminf(1, m_ambientSky.b * 0.7f + m_sunColor.b * 0.5f), 1 };
        for (u32 pass = 0; pass < 2; pass++)
        {
            for (u32 s = 0; s < 16; s++)
            {
                const GrowArray<Particle3>& particles = m_particles[s];
                if (particles.count() == 0)
                {
                    continue;
                }
                const ParticleStyle& style = m_world.style(s);
                const bool adds = style.blend == gpu::Blend::Additive;
                if (adds != (pass == 1) || (style.texture && !style.texture->ready))
                {
                    continue;
                }
                m_items.clear();
                for (u32 i = 0; i < particles.count(); i++)
                {
                    const float far = (particles[i].place - view.eye).dot(ahead);
                    if (far > 0.1f && far < m_far)
                    {
                        m_items.push({ far, i });
                    }
                }
                if (!adds)
                {
                    std::sort(m_items.begin(), m_items.end(), [](const Item& a, const Item& b) { return a.key > b.key; });
                }
                runs[runCount] = { s, used, 0 };
                for (const Item& item : m_items)
                {
                    if (used == kMost)
                    {
                        break;
                    }
                    const Particle3& p = particles[item.index];
                    const float t = p.age / p.life;
                    const float size = (style.sizeStart + (style.sizeEnd - style.sizeStart) * t) * 0.5f;
                    gpu::Color c = { style.colorStart.r + (style.colorEnd.r - style.colorStart.r) * t, style.colorStart.g + (style.colorEnd.g - style.colorStart.g) * t,
                        style.colorStart.b + (style.colorEnd.b - style.colorStart.b) * t, style.colorStart.a + (style.colorEnd.a - style.colorStart.a) * t };
                    c.a *= style.fadeIn > 0 && t < style.fadeIn ? t / style.fadeIn : 1;
                    if (!adds)
                    {
                        // Smoke is lit by the day, all over.
                        c.r *= day.r;
                        c.g *= day.g;
                        c.b *= day.b;
                    }
                    const gpu::MeshVertex shade = meshVertex({}, {}, { fminf(1, c.r), fminf(1, c.g), fminf(1, c.b), fminf(1, fmaxf(0, c.a)) }, 0, 0);
                    Vec3 x = right * size, y = up * size;
                    if (p.turn != 0)
                    {
                        const float cs = cosf(p.turn), sn = sinf(p.turn);
                        const Vec3 rx = x * cs + y * sn;
                        y = y * cs - x * sn;
                        x = rx;
                    }
                    const Vec3 corners[4] = { p.place - x - y, p.place + x - y, p.place + x + y, p.place - x + y };
                    for (u32 k = 0; k < 4; k++)
                    {
                        gpu::MeshVertex& v = data.vertices[used * 4 + k];
                        v.x = corners[k].x;
                        v.y = corners[k].y;
                        v.z = corners[k].z;
                        v.r = shade.r;
                        v.g = shade.g;
                        v.b = shade.b;
                        v.a = shade.a;
                    }
                    used++;
                }
                runs[runCount].count = used - runs[runCount].first;
                runCount += runs[runCount].count ? 1 : 0;
            }
        }
        if (used == 0)
        {
            return;
        }
        m_particleMesh = (m_particleMesh + 1) % 6;
        gpu::Mesh& mesh = m_particleMeshes[m_particleMesh];
        if (mesh)
        {
            device.updateMesh(mesh, data.vertices.begin(), used * 4);
        }
        else
        {
            mesh = device.createMesh(data.vertices.begin(), data.vertices.count(), data.indices.begin(), data.indices.count(), true);
        }
        for (u32 i = 0; mesh && i < runCount; i++)
        {
            const ParticleStyle& style = m_world.style(runs[i].style);
            gpu::MeshDraw d;
            d.mesh = mesh;
            d.first = runs[i].first * 6;
            d.count = runs[i].count * 6;
            d.texture = style.texture ? style.texture->texture : m_solids->soft();
            d.blend = style.blend == gpu::Blend::None ? gpu::Blend::Alpha : style.blend;
            d.cull = gpu::Cull::None;
            d.depthWrite = d.lit = false;
            d.fogged = style.blend != gpu::Blend::Additive;
            device.drawMesh(d);
        }
    }

    // --- Sound ---

    void Space::follow(Voice voice, u32 entity, const Vec3& place, float volume, float range)
    {
        if (voice)
        {
            m_heard.push({ voice, entity, place, volume, range > 0.1f ? range : 0.1f, -1, 0 });
        }
    }

    void Space::hear(const Vec3& place, float volume, float range, float& gain, float& pan) const
    {
        gain = pan = 0;
        for (u32 i = 0; i < m_earCount; i++)
        {
            const Vec3 d = place - m_ears[i];
            const float far = d.length();
            const float near = fmaxf(0, 1 - far / range);
            if (near * near * volume > gain)
            {
                gain = near * near * volume;
                pan = far > 0.01f ? d.dot(m_earSides[i]) / far * 0.85f * fminf(1, far * 0.5f) : 0;
            }
        }
    }

    // Where the cameras were, and what can be heard from there.
    void Space::setSkyPicture(Asset* texture)
    {
        if (texture)
        {
            m_assets.addRef(texture);
        }
        if (m_skyPicture)
        {
            m_assets.release(m_skyPicture);
        }
        m_skyPicture = texture;
    }

    void Space::startSounds()
    {
        for (u32 i = 0; i < m_waiting.count();)
        {
            const Waiting w = m_waiting[i];
            bool over = w.sound->failed || !m_world.alive(w.entity);
            if (!over && w.sound->sound)
            {
                PlayOptions o;
                const Vec3 place = worldPlace(w.entity);
                hear(place, w.volume, w.range, o.volume, o.pan);
                o.loop = true;
                o.priority = 140;
                // Whoever is too far to hear it needs no voice yet.
                const Voice voice = o.volume > 0 ? m_audio->play(*w.sound->sound, w.sound, o) : Voice();
                if (voice)
                {
                    m_assets.addRef(w.sound);  // for the voice
                    follow(voice, w.entity, place, w.volume, w.range);
                    over = true;
                }
            }
            if (over)
            {
                m_assets.release(w.sound);
                m_waiting.swapRemove(i);
                continue;
            }
            i++;
        }
    }

    void Space::listen()
    {
        if (m_focusCount)
        {
            m_earCount = m_focusCount;
            for (u32 i = 0; i < m_focusCount; i++)
            {
                m_ears[i] = m_focus[i];
                m_earSides[i] = (m_focusTo[i] - m_focus[i]).cross({ 0, 1, 0 }).normalized();
            }
        }
        if (m_audio == nullptr)
        {
            m_heard.clear();
            return;
        }
        startSounds();
        for (u32 i = 0; i < m_heard.count();)
        {
            Heard& h = m_heard[i];
            const bool gone = h.entity && !m_world.alive(h.entity);
            if (gone)
            {
                m_audio->stop(h.voice, 0.15f);
            }
            if (gone || !m_audio->playing(h.voice))
            {
                m_heard.swapRemove(i);
                continue;
            }
            float gain, pan;
            hear(h.entity ? worldPlace(h.entity) : h.place, h.volume, h.range, gain, pan);
            // Only when it makes a difference: the mixer is told through a short queue.
            if (fabsf(gain - h.gain) > 0.008f || (gain == 0 && h.gain > 0))
            {
                h.gain = gain;
                m_audio->setVolume(h.voice, gain, 0.06f);
            }
            if (fabsf(pan - h.pan) > 0.03f)
            {
                h.pan = pan;
                m_audio->setPan(h.voice, pan);
            }
            i++;
        }
    }

    // A soft dark patch under each thing, for a GPU that can't draw real shadows.
    void Space::drawBlobs(const gpu::View&, const Frustum& frustum)
    {
        gpu::Device& device = *m_solids->device();
        for (const Item& item : m_items)
        {
            const Appearance& a = m_appearances[item.index];
            if (!a.casts || a.glowBall || a.blend != gpu::Blend::None || item.key > m_shadowDistance * 0.7f)
            {
                continue;
            }
            Vec3 middle;
            float radius;
            boundsOf(a, m_spatials[m_world.m_entities[a.owner].parts3[0]].world, middle, radius);
            const float ground = heightAt(middle.x, middle.z);
            const float above = middle.y - ground;
            if (above < -radius || above > radius * 4 + 2 || radius > 12 || !frustum.sees({ middle.x, ground, middle.z }, radius))
            {
                continue;
            }
            const Vec3 up = normalAt(middle.x, middle.z);
            const float size = radius * 1.05f;
            const Vec3 x = Vec3(up.y, -up.x, 0).normalized() * size, z = x.cross(up).normalized() * size;
            gpu::MeshDraw d;
            d.mesh = m_solids->square();
            d.world.m[0] = x.x;
            d.world.m[1] = x.y;
            d.world.m[2] = x.z;
            d.world.m[4] = z.x;
            d.world.m[5] = z.y;
            d.world.m[6] = z.z;
            d.world.m[8] = up.x;
            d.world.m[9] = up.y;
            d.world.m[10] = up.z;
            d.world.m[12] = middle.x;
            d.world.m[13] = ground + 0.06f;
            d.world.m[14] = middle.z;
            d.texture = m_solids->soft();
            d.color = { 0, 0, 0, fminf(1, m_shadowStrength * 1.25f) * fmaxf(0, 1 - above / (radius * 4 + 2)) };
            d.blend = gpu::Blend::Alpha;
            d.cull = gpu::Cull::None;
            d.depthWrite = d.lit = false;
            device.drawMesh(d);
        }
    }
}
