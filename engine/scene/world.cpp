#include "scene/world.h"
#include "base/log.h"
#include "platform/platform.h"

#include <angelscript.h>

#include <algorithm>
#include <math.h>
#include <stdio.h>
#include <string.h>

namespace nx
{
    namespace
    {
        constexpr float kDegrees = 3.14159265f / 180.0f;
    }

    // --- References ---

    void EntityRef::release()
    {
        if (--refs > 0)
        {
            return;
        }
        if (world == nullptr)
        {
            destroy(this);
            return;
        }
        if (World::Entity* e = world->find(id))
        {
            e->ref = nullptr;
        }
        else if (id && (id & 0xFFFF) < world->m_entities.count() && world->m_entities[id & 0xFFFF].ref == this)
        {
            world->m_entities[id & 0xFFFF].ref = nullptr;  // being destroyed
        }
        (previous ? previous->next : world->m_refs) = next;
        if (next)
        {
            next->previous = previous;
        }
        if (world->m_freeRefCount < World::kFreeRefs)
        {
            world->m_freeRefs[world->m_freeRefCount++] = this;
        }
        else
        {
            destroy(this);
        }
    }

    EntityRef* World::makeRef()
    {
        EntityRef* r = m_freeRefCount ? m_freeRefs[--m_freeRefCount] : create<EntityRef>(MemTag::Script);
        if (r)
        {
            *r = EntityRef();
            r->world = this;
            r->next = m_refs;
            if (m_refs)
            {
                m_refs->previous = r;
            }
            m_refs = r;
        }
        return r;
    }

    EntityRef* World::ref(u32 id)
    {
        Entity* e = find(id);
        if (e == nullptr)
        {
            return nullptr;
        }
        if (e->ref == nullptr)
        {
            EntityRef* r = makeRef();
            if (r == nullptr)
            {
                return nullptr;
            }
            r->id = id;
            e->ref = r;
        }
        e->ref->addRef();
        return e->ref;
    }

    // --- Entities ---

    World::World(Assets& assets, ScriptHost& host, const char* module, u32 maxEntities, u32 maxParticles)
        : m_assets(assets), m_host(host), m_maxEntities(maxEntities < (1u << kIndexBits) ? maxEntities : (1u << kIndexBits) - 1), m_maxParticles(maxParticles), m_space(*this, assets)
    {
        snprintf(m_module, sizeof(m_module), "%s", module);
    }

    World::~World()
    {
        clear();
        releaseLightMap();
        // References that scripts still hold must not look for this world any more.
        for (EntityRef* r = m_refs; r;)
        {
            EntityRef* next = r->next;
            r->world = nullptr;
            r->id = 0;
            r->next = r->previous = nullptr;
            r = next;
        }
        for (u32 i = 0; i < m_freeRefCount; i++)
        {
            destroy(m_freeRefs[i]);
        }
        for (u32 i = 0; i < m_mapCount; i++)
        {
            memFree(m_maps[i].tiles);
            if (m_maps[i].texture)
            {
                m_assets.release(m_maps[i].texture);
            }
        }
        for (PrefabPart& p : m_parts)
        {
            if (p.visual.asset)
            {
                m_assets.release(p.visual.asset);
            }
        }
        for (ParticleStyle& s : m_styles)
        {
            if (s.texture)
            {
                m_assets.release(s.texture);
            }
        }
        for (u32 i = 0; i < m_fileCount; i++)
        {
            destroy(m_files[i].json);
        }
    }

    u32 World::spawn(Vec2 position)
    {
        u32 index = m_freeEntity;
        if (index != kNone)
        {
            m_freeEntity = m_entities[index].next;
        }
        else
        {
            if (m_entities.count() >= m_maxEntities)
            {
                return 0;
            }
            Entity fresh = {};
            fresh.generation = 1;
            index = m_entities.count();
            if (!m_entities.push(fresh))
            {
                return 0;
            }
        }
        Entity& e = m_entities[index];
        const u32 generation = e.generation;
        e = {};
        e.generation = generation;
        e.live = e.visible = e.shown = true;
        e.parent = e.child = e.next = e.previous = e.behaviour = kNone;
        for (u32& part : e.parts)
        {
            part = kNone;
        }
        for (u32& part : e.parts3)
        {
            part = kNone;
        }
        e.position = position;
        e.scale = { 1, 1 };
        e.cosine = 1;
        m_live++;
        m_orderDirty = true;
        transform(index);
        return idOf(index);
    }

    World::Entity* World::find(u32 id)
    {
        const u32 index = id & ((1u << kIndexBits) - 1);
        if (index >= m_entities.count())
        {
            return nullptr;
        }
        Entity& e = m_entities[index];
        return e.live && !e.dying && e.generation == id >> kIndexBits ? &e : nullptr;
    }

    void World::destroyEntity(u32 id)
    {
        if (find(id))
        {
            markDying(id & ((1u << kIndexBits) - 1));
        }
    }

    void World::markDying(u32 index)
    {
        // Without recursion: a long chain of children mustn't use up the stack.
        u32 at = index;
        for (;;)
        {
            Entity& e = m_entities[at];
            if (!e.dying)
            {
                e.dying = true;
                m_dying++;
            }
            if (e.child != kNone)
            {
                at = e.child;
                continue;
            }
            while (at != index && m_entities[at].next == kNone)
            {
                at = m_entities[at].parent;
            }
            if (at == index)
            {
                return;
            }
            at = m_entities[at].next;
        }
    }

    u32 World::countGroup(u32 group) const
    {
        u32 count = 0;
        for (const Collider& c : m_colliders)
        {
            count += c.group == group && !m_entities[c.owner].dying;
        }
        count += m_space.countGroup(group);
        return count;
    }

    u32 World::findNamed(const char* name) const
    {
        for (u32 i = 0; i < m_entities.count(); i++)
        {
            const Entity& e = m_entities[i];
            if (e.live && !e.dying && strcmp(e.name, name) == 0)
            {
                return idOf(i);
            }
        }
        return 0;
    }

    void World::clearParticles()
    {
        for (auto& particles : m_particles)
        {
            particles.clear();
        }
        m_particleCount = 0;
        m_space.clearParticles();
    }

    void World::clear()
    {
        for (u32 i = 0; i < m_entities.count(); i++)
        {
            if (m_entities[i].live)
            {
                remove(i);
            }
        }
        clearParticles();
        m_pairs.clear();
        m_pairsBefore.clear();
        m_touches.clear();
        m_touchesBefore.clear();
        m_joints.clear();
        m_space.forget();
        m_space.unjoin(0);
        m_order.clear();
        m_orderDirty = m_drawDirty = true;
        m_dying = 0;
    }

    template <typename T>
    void World::removePart(GrowArray<T>& parts, Component kind, u32 index)
    {
        const u32 at = m_entities[index].parts[static_cast<size_t>(kind)];
        if (at == kNone)
        {
            return;
        }
        parts.swapRemove(at);
        if (at < parts.count())
        {
            m_entities[parts[at].owner].parts[static_cast<size_t>(kind)] = at;
        }
        m_entities[index].parts[static_cast<size_t>(kind)] = kNone;
    }

    void World::freeVisual(Visual& v)
    {
        if (v.asset)
        {
            m_assets.release(v.asset);
            v.asset = nullptr;
        }
        memFree(v.text);
        v.text = nullptr;
    }

    void World::remove(u32 index)
    {
        unlink(index);
        Entity& e = m_entities[index];
        // Children have their own removal coming; until then they stand alone.
        for (u32 child = e.child; child != kNone;)
        {
            const u32 next = m_entities[child].next;
            m_entities[child].parent = m_entities[child].next = m_entities[child].previous = kNone;
            child = next;
        }
        if (e.parts[static_cast<size_t>(Component::Visual)] != kNone)
        {
            freeVisual(m_visuals[e.parts[static_cast<size_t>(Component::Visual)]]);
            m_drawDirty = true;
        }
        removePart(m_visuals, Component::Visual, index);
        removePart(m_animations, Component::Animation, index);
        removePart(m_bodies, Component::Body, index);
        removePart(m_colliders, Component::Collider, index);
        removePart(m_lifetimes, Component::Lifetime, index);
        removePart(m_emitters, Component::Emitter, index);
        removePart(m_lights, Component::Light, index);
        m_space.removed(index);
        for (u32 b = e.behaviour; b != kNone;)
        {
            Behaviour& behaviour = m_behaviours[b];
            const u32 next = behaviour.next;
            asIScriptObject* object = behaviour.object;
            m_unstarted -= behaviour.started ? 0 : 1;
            behaviour = {};
            behaviour.next = m_freeBehaviour;
            m_freeBehaviour = b;
            b = next;
            object->Release();  // may run a destructor, so it comes last
        }
        Entity& gone = m_entities[index];
        if (gone.ref)
        {
            gone.ref->id = 0;
            gone.ref = nullptr;
        }
        if (gone.dying)
        {
            m_dying--;
        }
        gone.live = gone.dying = false;
        gone.generation = gone.generation >= 0xFFFF ? 1 : gone.generation + 1;
        gone.next = m_freeEntity;
        gone.behaviour = kNone;
        m_freeEntity = index;
        m_live--;
        m_orderDirty = true;
    }

    // --- Hierarchy ---

    void World::unlink(u32 index)
    {
        Entity& e = m_entities[index];
        if (e.parent != kNone)
        {
            (e.previous != kNone ? m_entities[e.previous].next : m_entities[e.parent].child) = e.next;
            if (e.next != kNone)
            {
                m_entities[e.next].previous = e.previous;
            }
        }
        e.parent = e.next = e.previous = kNone;
        m_orderDirty = true;
    }

    void World::setParent(u32 id, u32 parentId)
    {
        Entity* e = find(id);
        Entity* parent = find(parentId);
        if (e == nullptr)
        {
            return;
        }
        // Attached afresh: to the whole of the parent, unless attachToBone says which bone after this.
        if (Spatial* s = m_space.getSpatial(id))
        {
            s->boneName[0] = 0;
            s->bone = -1;
        }
        const u32 index = id & ((1u << kIndexBits) - 1);
        const u32 parentIndex = parent ? parentId & ((1u << kIndexBits) - 1) : kNone;
        // Nothing may be inside itself.
        for (u32 at = parentIndex; at != kNone; at = m_entities[at].parent)
        {
            if (at == index)
            {
                return;
            }
        }
        unlink(index);
        if (parent)
        {
            e->parent = parentIndex;
            e->next = parent->child;
            if (parent->child != kNone)
            {
                m_entities[parent->child].previous = index;
            }
            parent->child = index;
        }
        transform(index);
    }

    u32 World::parentOf(u32 id) const
    {
        const Entity* e = find(id);
        return e && e->parent != kNone ? idOf(e->parent) : 0;
    }

    Vec2 World::worldPosition(u32 id) const
    {
        const Entity* e = find(id);
        if (e == nullptr)
        {
            return {};
        }
        Vec2 p = e->position;
        for (u32 at = e->parent; at != kNone; at = m_entities[at].parent)
        {
            const Entity& up = m_entities[at];
            const float c = cosf(up.rotation * kDegrees), s = sinf(up.rotation * kDegrees);
            const Vec2 scaled(p.x * up.scale.x, p.y * up.scale.y);
            p = up.position + Vec2(scaled.x * c - scaled.y * s, scaled.x * s + scaled.y * c);
        }
        return p;
    }

    void World::transform(u32 index)
    {
        Entity& e = m_entities[index];
        if (e.parts3[0] != kNone)
        {
            m_space.transform(index);
        }
        if (e.rotation != e.turned)
        {
            e.turned = e.rotation;
            e.cosine = cosf(e.rotation * kDegrees);
            e.sine = sinf(e.rotation * kDegrees);
        }
        const float m[6] = { e.cosine * e.scale.x, e.sine * e.scale.x, -e.sine * e.scale.y, e.cosine * e.scale.y, e.position.x, e.position.y };
        if (e.parent == kNone)
        {
            memcpy(e.world, m, sizeof(m));
            e.shown = e.visible;
            return;
        }
        const Entity& parent = m_entities[e.parent];
        const float* p = parent.world;
        e.world[0] = p[0] * m[0] + p[2] * m[1];
        e.world[1] = p[1] * m[0] + p[3] * m[1];
        e.world[2] = p[0] * m[2] + p[2] * m[3];
        e.world[3] = p[1] * m[2] + p[3] * m[3];
        e.world[4] = p[0] * m[4] + p[2] * m[5] + p[4];
        e.world[5] = p[1] * m[4] + p[3] * m[5] + p[5];
        e.shown = e.visible && parent.shown;
    }

    void World::updateTransforms()
    {
        if (m_orderDirty)
        {
            m_orderDirty = false;
            m_order.clear();
            m_order.reserve(m_live);
            for (u32 i = 0; i < m_entities.count(); i++)
            {
                if (m_entities[i].live && m_entities[i].parent == kNone)
                {
                    m_order.push(i);
                }
            }
            for (u32 i = 0; i < m_order.count(); i++)
            {
                for (u32 child = m_entities[m_order[i]].child; child != kNone; child = m_entities[child].next)
                {
                    m_order.push(child);
                }
            }
        }
        for (const u32 index : m_order)
        {
            transform(index);
        }
    }

    void World::setLayer(u32 id, int layer)
    {
        Entity* e = find(id);
        if (e && e->layer != layer)
        {
            e->layer = layer;
            m_drawDirty = true;
        }
    }

    // --- Components ---

    Light2* World::light(u32 id)
    {
        Light2 fresh = {};
        fresh.color = { 1, 1, 1, 1 };
        fresh.radius = 100;
        return getOrAdd(m_lights, Component::Light, id, fresh);
    }

    void World::removeLight(u32 id)
    {
        const Entity* e = find(id);
        if (e && e->parts[static_cast<size_t>(Component::Light)] != kNone)
        {
            removePart(m_lights, Component::Light, id & ((1u << kIndexBits) - 1));
        }
    }

    Visual* World::visual(u32 id)
    {
        Visual fresh = {};
        fresh.blend = gpu::Blend::Alpha;
        fresh.columns = fresh.rows = 1;
        fresh.color = { 1, 1, 1, 1 };
        const u32 before = m_visuals.count();
        Visual* v = getOrAdd(m_visuals, Component::Visual, id, fresh);
        m_drawDirty = m_drawDirty || m_visuals.count() != before;
        return v;
    }

    Animation* World::animation(u32 id)
    {
        Animation fresh = {};
        fresh.loop = true;
        return getOrAdd(m_animations, Component::Animation, id, fresh);
    }

    Body* World::body(u32 id)
    {
        Body fresh = {};
        fresh.friction = 0.4f;
        return getOrAdd(m_bodies, Component::Body, id, fresh);
    }

    Collider* World::collider(u32 id)
    {
        Collider fresh = {};
        fresh.halfWidth = fresh.halfHeight = 8;
        return getOrAdd(m_colliders, Component::Collider, id, fresh);
    }

    Lifetime* World::lifetime(u32 id)
    {
        return getOrAdd(m_lifetimes, Component::Lifetime, id, Lifetime());
    }

    void World::setLook(u32 id, Look look, Asset* asset)
    {
        Visual* v = visual(id);
        if (v == nullptr)
        {
            return;
        }
        if (asset)
        {
            m_assets.addRef(asset);
        }
        if (v->asset)
        {
            m_assets.release(v->asset);
        }
        v->asset = asset;
        v->look = look;
        m_drawDirty = true;
    }

    void World::setText(u32 id, const char* text)
    {
        if (Visual* v = visual(id))
        {
            const size_t size = strlen(text) + 1;
            // Counters change often and rarely get longer.
            if (v->text == nullptr || strlen(v->text) + 1 < size)
            {
                memFree(v->text);
                v->text = static_cast<char*>(memAlloc(size, MemTag::Scene));
            }
            if (v->text)
            {
                memcpy(v->text, text, size);
            }
        }
    }

    // --- Behaviours ---

    int World::classOf(asITypeInfo* type)
    {
        for (u32 i = 0; i < m_classCount; i++)
        {
            if (m_classes[i].type == type)
            {
                return static_cast<int>(i);
            }
        }
        if (m_classCount == kMaxClasses)
        {
            return -1;
        }
        Class c = {};
        c.type = type;
        c.entityProperty = -1;
        // Only what the class itself provides: calling the empty ones it inherits would cost as much.
        const auto method = [&](const char* declaration)
        {
            asIScriptFunction* real = type->GetMethodByDecl(declaration, false);
            return real && real->GetObjectType() && strcmp(real->GetObjectType()->GetName(), "Behaviour") != 0 ? type->GetMethodByDecl(declaration) : nullptr;
        };
        c.start = method("void start()");
        c.update = method("void update(float)");
        c.onHit = method("void onHit(Entity@)");
        c.onLeave = method("void onLeave(Entity@)");
        c.onGone = method("void onGone()");
        for (asUINT i = 0; i < type->GetPropertyCount(); i++)
        {
            const char* name = nullptr;
            type->GetProperty(i, &name);
            if (strcmp(name, "entity") == 0)
            {
                c.entityProperty = static_cast<int>(i);
            }
        }
        m_classes[m_classCount] = c;
        return static_cast<int>(m_classCount++);
    }

    bool World::addBehaviour(u32 id, asIScriptObject* object)
    {
        Entity* e = find(id);
        const int type = e && object ? classOf(object->GetObjectType()) : -1;
        if (type < 0 || m_classes[type].entityProperty < 0)
        {
            return false;
        }
        const u32 index = id & ((1u << kIndexBits) - 1);
        Behaviour b = {};
        b.object = object;
        b.owner = index;
        b.ownerId = id;
        b.type = static_cast<u16>(type);
        u32 at = m_freeBehaviour;
        if (at != kNone)
        {
            m_freeBehaviour = m_behaviours[at].next;
            m_behaviours[at] = b;
        }
        else
        {
            at = m_behaviours.count();
            if (!m_behaviours.push(b))
            {
                return false;
            }
        }
        // At the end, so they are called in the order they were added.
        m_behaviours[at].next = kNone;
        u32* link = &e->behaviour;
        while (*link != kNone)
        {
            link = &m_behaviours[*link].next;
        }
        *link = at;
        object->AddRef();
        m_unstarted++;
        EntityRef** property = static_cast<EntityRef**>(object->GetAddressOfProperty(static_cast<asUINT>(m_classes[type].entityProperty)));
        EntityRef* before = *property;
        *property = ref(id);
        if (before)
        {
            before->release();
        }
        return true;
    }

    asIScriptObject* World::behaviour(u32 id) const
    {
        const Entity* e = find(id);
        return e && e->behaviour != kNone ? m_behaviours[e->behaviour].object : nullptr;
    }

    bool World::startBehaviours()
    {
        // Starting one may add more.
        for (u32 i = 0; m_unstarted && i < m_behaviours.count(); i++)
        {
            Behaviour& b = m_behaviours[i];
            if (b.object == nullptr || b.started)
            {
                continue;
            }
            b.started = true;
            m_unstarted--;
            if (m_classes[b.type].start && !m_entities[b.owner].dying)
            {
                asIScriptObject* object = b.object;
                if (!m_host.callMethod(object, m_classes[b.type].start))
                {
                    return false;
                }
            }
        }
        return true;
    }

    bool World::updateBehaviours(float dt)
    {
        // Those added meanwhile have their first turn next step.
        const u32 count = m_behaviours.count();
        for (u32 i = 0; i < count; i++)
        {
            const Behaviour& b = m_behaviours[i];
            if (b.object && b.started && m_classes[b.type].update && !m_entities[b.owner].dying)
            {
                if (!m_host.callMethod(b.object, m_classes[b.type].update, dt))
                {
                    return false;
                }
            }
        }
        return true;
    }

    // Passes the other entity if there is one.
    bool World::tell(u32 id, asIScriptFunction* Class::*method, u32 other)
    {
        const u32 index = id & ((1u << kIndexBits) - 1);
        const u32 generation = id >> kIndexBits;
        u32 at = index < m_entities.count() && m_entities[index].live && m_entities[index].generation == generation ? m_entities[index].behaviour : kNone;
        while (at != kNone)
        {
            const Behaviour& b = m_behaviours[at];
            const u32 next = b.next;
            asIScriptFunction* function = m_classes[b.type].*method;
            if (function)
            {
                bool ok = true;
                if (other)
                {
                    if (EntityRef* r = ref(other))
                    {
                        ok = m_host.callMethod(b.object, function, r);
                        r->release();
                    }
                }
                else
                {
                    ok = m_host.callMethod(b.object, function);
                }
                if (!ok)
                {
                    return false;
                }
            }
            at = next;
        }
        return true;
    }

    // --- The step ---

    bool World::update(float dt)
    {
        if (m_live == 0 && m_particleCount == 0 && !m_space.streaming() && m_space.particles() == 0)
        {
            return true;
        }
        bool ok = startBehaviours() && updateBehaviours(dt) && startBehaviours();
        if (ok)
        {
            move(dt);
            physics(dt);
            m_space.update(dt);
            animate(dt);
            age(dt);
            updateTransforms();
            emit(dt);
            moveParticles(dt);
            ok = collide();
        }
        ok = removeDead() && ok;
        updateTransforms();
        m_stats.entities = m_live;
        m_stats.particles = m_particleCount + m_space.particles();
        return ok;
    }

    bool World::removeDead()
    {
        bool ok = true;
        // Being told may destroy more; what is left after a few rounds waits for the next step.
        for (u32 round = 0; m_dying && round < 8; round++)
        {
            const u32 count = m_entities.count();
            for (u32 i = 0; i < count; i++)
            {
                if (!m_entities[i].live || !m_entities[i].dying)
                {
                    continue;
                }
                const u32 id = idOf(i);
                ok = tell(id, &Class::onGone, 0) && ok;
                if (m_entities[i].live && idOf(i) == id)
                {
                    remove(i);
                }
            }
        }
        return ok;
    }

    void World::move(float dt)
    {
        for (u32 i = 0; i < m_bodies.count(); i++)
        {
            Body& b = m_bodies[i];
            Entity& e = m_entities[b.owner];
            if (e.dying || b.mass > 0)
            {
                continue;
            }
            b.velocity = b.velocity + m_gravity * (b.gravity * dt);
            if (b.drag > 0)
            {
                const float kept = 1 - b.drag * dt;
                b.velocity = b.velocity * (kept > 0 ? kept : 0);
            }
            e.rotation += b.spin * dt;
            const Vec2 by = b.velocity * dt;
            if (b.solid && e.parent == kNone)
            {
                moveSolid(i, by);
            }
            else
            {
                e.position = e.position + by;
            }
        }
    }

    void World::moveSolid(u32 bodyIndex, Vec2 by)
    {
        Body& b = m_bodies[bodyIndex];
        Entity& e = m_entities[b.owner];
        float halfX = 8, halfY = 8;
        const u32 collider = e.parts[static_cast<size_t>(Component::Collider)];
        if (collider != kNone)
        {
            // Without turning, whatever the picture does: a character leaning over still stands.
            const Collider& c = m_colliders[collider];
            halfX = c.halfWidth * fabsf(e.scale.x);
            halfY = c.halfHeight * fabsf(e.scale.y);
        }
        b.onGround = b.onCeiling = false;
        b.onWall = 0;
        // It may be a little inside something, having grown or been put there: out by the shortest way,
        // so that the floor isn't taken for a wall.
        for (u32 i = 0; i < 4; i++)
        {
            const Box box = { e.position.x - halfX, e.position.x + halfX, e.position.y - halfY, e.position.y + halfY };
            float most = 0.02f;
            Vec2 out;
            solids(box, b.owner, [&](bool fromAbove, const Box& solid)
            {
                if (fromAbove)
                {
                    return;
                }
                const float ways[4] = { box.maxX - solid.minX, solid.maxX - box.minX, box.maxY - solid.minY, solid.maxY - box.minY };
                u32 least = 0;
                for (u32 w = 1; w < 4; w++)
                {
                    least = ways[w] < ways[least] ? w : least;
                }
                if (ways[least] > most)
                {
                    most = ways[least];
                    out = least == 0 ? Vec2(-most - 0.01f, 0) : least == 1 ? Vec2(most + 0.01f, 0) : least == 2 ? Vec2(0, -most - 0.01f) : Vec2(0, most + 0.01f);
                }
            });
            if (out.x == 0 && out.y == 0)
            {
                break;
            }
            e.position = e.position + out;
        }
        // In steps too short to pass through anything.
        const float longest = fabsf(by.x) > fabsf(by.y) ? fabsf(by.x) : fabsf(by.y);
        const float limit = (halfX < halfY ? halfX : halfY) > 2 ? (halfX < halfY ? halfX : halfY) : 2;
        u32 steps = static_cast<u32>(longest / limit) + 1;
        steps = steps > 32 ? 32 : steps;
        constexpr float gap = 0.01f;
        for (u32 s = 0; s < steps; s++)
        {
            const float n = static_cast<float>(steps);
            const float dx = by.x / n, dy = by.y / n;
            float edge = 0;
            const float bottomBefore = e.position.y + halfY;
            if (dx != 0)
            {
                e.position.x += dx;
                const Box box = { e.position.x - halfX, e.position.x + halfX, e.position.y - halfY, e.position.y + halfY };
                if (obstacle(box, 0, dx > 0, bottomBefore, b.owner, edge))
                {
                    e.position.x = dx > 0 ? edge - halfX - gap : edge + halfX + gap;
                    b.onWall = dx > 0 ? 1 : -1;
                    b.velocity.x *= -b.bounce;
                    by.x = 0;
                }
            }
            if (dy != 0)
            {
                e.position.y += dy;
                const Box box = { e.position.x - halfX, e.position.x + halfX, e.position.y - halfY, e.position.y + halfY };
                if (obstacle(box, 1, dy > 0, bottomBefore, b.owner, edge))
                {
                    e.position.y = dy > 0 ? edge - halfY - gap : edge + halfY + gap;
                    (dy > 0 ? b.onGround : b.onCeiling) = true;
                    // Small bounces come to rest.
                    b.velocity.y = fabsf(b.velocity.y) > 60 ? b.velocity.y * -b.bounce : 0;
                    by.y = 0;
                }
            }
        }
    }

    // Everything solid that a box overlaps: whether it is solid only from above, and its edges.
    template <typename Fn>
    void World::solids(const Box& box, u32 self, Fn fn) const
    {
        constexpr float touching = 0.004f;
        for (u32 m = 0; m < m_mapCount; m++)
        {
            const TileMap& map = m_maps[m];
            const float across = 1.0f / map.size;
            const int x0 = static_cast<int>(floorf((box.minX - map.origin.x) * across)), x1 = static_cast<int>(floorf((box.maxX - map.origin.x) * across));
            const int y0 = static_cast<int>(floorf((box.minY - map.origin.y) * across)), y1 = static_cast<int>(floorf((box.maxY - map.origin.y) * across));
            for (int y = y0 < 0 ? 0 : y0; y <= y1 && y < static_cast<int>(map.height); y++)
            {
                for (int x = x0 < 0 ? 0 : x0; x <= x1 && x < static_cast<int>(map.width); x++)
                {
                    const u16 tile = map.tiles[static_cast<size_t>(y) * map.width + static_cast<size_t>(x)];
                    const u8 solid = tile && tile < 64 ? map.solid[tile] : 0;
                    const float left = map.origin.x + static_cast<float>(x) * map.size, top = map.origin.y + static_cast<float>(y) * map.size;
                    // Resting exactly on one isn't being inside it.
                    if (solid && box.maxX > left + touching && box.minX < left + map.size - touching && box.maxY > top + touching && box.minY < top + map.size - touching)
                    {
                        fn(solid == 2, Box{ left, left + map.size, top, top + map.size });
                    }
                }
            }
        }
        for (const Collider& c : m_colliders)
        {
            if (!c.wall || c.owner == self || m_entities[c.owner].dying)
            {
                continue;
            }
            float hx = 0, hy = 0;
            halfExtent(c, hx, hy);
            const float* w = m_entities[c.owner].world;
            if (box.minX < w[4] + hx && box.maxX > w[4] - hx && box.minY < w[5] + hy && box.maxY > w[5] - hy)
            {
                fn(false, Box{ w[4] - hx, w[4] + hx, w[5] - hy, w[5] + hy });
            }
        }
    }

    // The nearest edge of anything solid that a box has moved into along an axis.
    bool World::obstacle(const Box& box, u32 axis, bool positive, float bottomBefore, u32 self, float& edge) const
    {
        bool found = false;
        solids(box, self, [&](bool fromAbove, const Box& s)
        {
            if (fromAbove && (axis == 0 || !positive || bottomBefore > s.minY + 0.5f))
            {
                return;
            }
            const float at = axis == 0 ? (positive ? s.minX : s.maxX) : (positive ? s.minY : s.maxY);
            edge = !found ? at : positive ? (at < edge ? at : edge) : (at > edge ? at : edge);
            found = true;
        });
        return found;
    }

    void World::animate(float dt)
    {
        for (Animation& a : m_animations)
        {
            if (a.done || a.fps <= 0)
            {
                continue;
            }
            const float count = static_cast<float>(a.last - a.first + 1);
            a.at += a.fps * dt;
            if (a.at >= count)
            {
                a.done = !a.loop;
                a.at = a.loop ? fmodf(a.at, count) : count - 1;
            }
            const u32 v = m_entities[a.owner].parts[static_cast<size_t>(Component::Visual)];
            if (v != kNone)
            {
                m_visuals[v].frame = static_cast<u16>(a.first + static_cast<u16>(a.at));
            }
        }
    }

    void World::age(float dt)
    {
        for (u32 i = 0; i < m_lifetimes.count(); i++)
        {
            m_lifetimes[i].left -= dt;
            if (m_lifetimes[i].left <= 0)
            {
                markDying(m_lifetimes[i].owner);
            }
        }
    }

    // --- Particles ---

    u32 World::random()
    {
        m_random ^= m_random << 13;
        m_random ^= m_random >> 17;
        m_random ^= m_random << 5;
        return m_random;
    }

    float World::random(float low, float high)
    {
        return low + (high - low) * (static_cast<float>(random() >> 8) / 16777216.0f);
    }

    u32 World::styleNamed(const char* name) const
    {
        for (u32 i = 0; i < m_styles.count(); i++)
        {
            if (strcmp(m_styles[i].name, name) == 0)
            {
                return i;
            }
        }
        return kNone;
    }

    bool World::burst(const char* name, Vec2 position, u32 count)
    {
        const u32 index = styleNamed(name);
        if (index == kNone)
        {
            return false;
        }
        const ParticleStyle& s = m_styles[index];
        for (u32 i = 0; i < count && m_particleCount < m_maxParticles; i++)
        {
            Particle p;
            const float direction = (s.angle + random(-0.5f, 0.5f) * s.spread) * kDegrees;
            const float speed = random(s.speedMin, s.speedMax);
            p.velocity = { cosf(direction) * speed, sinf(direction) * speed };
            p.position = position;
            if (s.radius > 0)
            {
                const float angle = random(0, 360) * kDegrees, away = sqrtf(random(0, 1)) * s.radius;
                p.position = position + Vec2(cosf(angle) * away, sinf(angle) * away);
            }
            p.age = 0;
            p.life = random(s.lifeMin, s.lifeMax);
            if (!m_particles[index].push(p))
            {
                break;
            }
            m_particleCount++;
        }
        return true;
    }

    void World::emit(float dt)
    {
        for (u32 i = 0; i < m_emitters.count(); i++)
        {
            Emitter& e = m_emitters[i];
            const Entity& owner = m_entities[e.owner];
            if (owner.dying || !owner.shown)
            {
                continue;
            }
            e.owed += e.rate * dt;
            const u32 count = static_cast<u32>(e.owed);
            e.owed -= static_cast<float>(count);
            if (count)
            {
                burst(m_styles[e.style].name, { owner.world[4], owner.world[5] }, count);
            }
        }
    }

    void World::moveParticles(float dt)
    {
        for (u32 s = 0; s < m_styles.count(); s++)
        {
            const ParticleStyle& style = m_styles[s];
            GrowArray<Particle>& particles = m_particles[s];
            const Vec2 pull = m_gravity * (style.gravity * dt);
            const float kept = 1 - style.drag * dt > 0 ? 1 - style.drag * dt : 0;
            for (u32 i = 0; i < particles.count();)
            {
                Particle& p = particles[i];
                p.age += dt;
                if (p.age >= p.life)
                {
                    particles.swapRemove(i);
                    m_particleCount--;
                    continue;
                }
                p.velocity = (p.velocity + pull) * kept;
                p.position = p.position + p.velocity * dt;
                i++;
            }
        }
    }

    // --- Collision ---

    void World::halfExtent(const Collider& c, float& x, float& y) const
    {
        const float* m = m_entities[c.owner].world;
        if (c.shape == Shape::Circle)
        {
            const float sx = m[0] * m[0] + m[1] * m[1], sy = m[2] * m[2] + m[3] * m[3];
            x = y = c.halfWidth * sqrtf(sx > sy ? sx : sy);
        }
        else
        {
            x = fabsf(m[0]) * c.halfWidth + fabsf(m[2]) * c.halfHeight;
            y = fabsf(m[1]) * c.halfWidth + fabsf(m[3]) * c.halfHeight;
        }
    }

    namespace
    {
        // A collider in world space: its middle, its axes and how far it goes along each.
        struct Placed
        {
            Vec2 at, u, v;
            float hx, hy;

            Placed(const Collider& c, const float (&m)[6])
            {
                at = { m[4], m[5] };
                const float sx = sqrtf(m[0] * m[0] + m[1] * m[1]), sy = sqrtf(m[2] * m[2] + m[3] * m[3]);
                u = sx > 0 ? Vec2(m[0] / sx, m[1] / sx) : Vec2(1, 0);
                v = sy > 0 ? Vec2(m[2] / sy, m[3] / sy) : Vec2(0, 1);
                hx = c.halfWidth * (c.shape == Shape::Circle && sy > sx ? sy : sx);
                hy = c.halfHeight * sy;
            }
        };

        bool circleInBox(const Placed& box, Vec2 at, float radius)
        {
            const Vec2 d = at - box.at;
            const float x = fabsf(d.dot(box.u)) - box.hx, y = fabsf(d.dot(box.v)) - box.hy;
            if (x > radius || y > radius)
            {
                return false;
            }
            return x <= 0 || y <= 0 || x * x + y * y <= radius * radius;
        }

        bool apart(const Placed& a, const Placed& b, Vec2 axis)
        {
            const float reach = a.hx * fabsf(a.u.dot(axis)) + a.hy * fabsf(a.v.dot(axis)) + b.hx * fabsf(b.u.dot(axis)) + b.hy * fabsf(b.v.dot(axis));
            return fabsf((b.at - a.at).dot(axis)) > reach;
        }
    }

    bool World::overlap(const Collider& a, const Collider& b) const
    {
        const Placed pa(a, m_entities[a.owner].world), pb(b, m_entities[b.owner].world);
        if (a.shape == Shape::Circle && b.shape == Shape::Circle)
        {
            const Vec2 d = pb.at - pa.at;
            const float reach = pa.hx + pb.hx;
            return d.dot(d) <= reach * reach;
        }
        if (a.shape == Shape::Circle)
        {
            return circleInBox(pb, pa.at, pa.hx);
        }
        if (b.shape == Shape::Circle)
        {
            return circleInBox(pa, pb.at, pb.hx);
        }
        return !apart(pa, pb, pa.u) && !apart(pa, pb, pa.v) && !apart(pa, pb, pb.u) && !apart(pa, pb, pb.v);
    }

    bool World::touchesCircle(const Collider& c, Vec2 position, float radius) const
    {
        const Placed p(c, m_entities[c.owner].world);
        if (c.shape == Shape::Circle)
        {
            const Vec2 d = position - p.at;
            return d.dot(d) <= (p.hx + radius) * (p.hx + radius);
        }
        return circleInBox(p, position, radius);
    }

    bool World::touching(u32 a, u32 b) const
    {
        const Entity* ea = find(a);
        const Entity* eb = find(b);
        const u32 ca = ea ? ea->parts[static_cast<size_t>(Component::Collider)] : kNone;
        const u32 cb = eb ? eb->parts[static_cast<size_t>(Component::Collider)] : kNone;
        return ca != kNone && cb != kNone && overlap(m_colliders[ca], m_colliders[cb]);
    }

    u32 World::nearest(Vec2 position, float radius, u32 groups, u32 except) const
    {
        u32 best = 0;
        float bestDistance = radius * radius;
        for (const Collider& c : m_colliders)
        {
            const Entity& e = m_entities[c.owner];
            if (!(groups & (1u << c.group)) || e.dying || idOf(c.owner) == except)
            {
                continue;
            }
            const Vec2 d = Vec2(e.world[4], e.world[5]) - position;
            const float distance = d.dot(d);
            if (distance <= bestDistance)
            {
                bestDistance = distance;
                best = idOf(c.owner);
            }
        }
        return best;
    }

    u32 World::raycast(Vec2 from, Vec2 to, u32 groups, Vec2& hit) const
    {
        const Vec2 way = to - from;
        float best = 1;
        u32 found = 0;
        for (const Collider& c : m_colliders)
        {
            if (!(groups & (1u << c.group)) || m_entities[c.owner].dying)
            {
                continue;
            }
            const Placed p(c, m_entities[c.owner].world);
            const Vec2 d = from - p.at;
            float at = 2;
            if (c.shape == Shape::Circle)
            {
                const float a = way.dot(way), b = d.dot(way), k = d.dot(d) - p.hx * p.hx;
                const float root = b * b - a * k;
                at = a > 0 && root >= 0 ? (-b - sqrtf(root)) / a : 2;
                at = k <= 0 ? 0 : at;  // starts inside
            }
            else
            {
                float low = 0, high = 1;
                const float starts[2] = { d.dot(p.u), d.dot(p.v) }, ways[2] = { way.dot(p.u), way.dot(p.v) }, halves[2] = { p.hx, p.hy };
                for (u32 i = 0; i < 2; i++)
                {
                    if (fabsf(ways[i]) < 1e-6f)
                    {
                        low = fabsf(starts[i]) > halves[i] ? 2 : low;
                        continue;
                    }
                    const float t1 = (-halves[i] - starts[i]) / ways[i], t2 = (halves[i] - starts[i]) / ways[i];
                    low = fmaxf(low, fminf(t1, t2));
                    high = fminf(high, fmaxf(t1, t2));
                }
                at = low <= high ? low : 2;
            }
            if (at >= 0 && at <= best)
            {
                best = at;
                found = idOf(c.owner);
            }
        }
        hit = from + way * best;
        return found;
    }

    // On the plane or in space.
    bool World::wantsToKnow(u32 id, u32 otherId)
    {
        const Collider* self = getCollider(id);
        const Collider* other = getCollider(otherId);
        if (self && other && (self->hits >> other->group) & 1)
        {
            return true;
        }
        const Collider3* self3 = m_space.getCollider(id);
        const Collider3* other3 = m_space.getCollider(otherId);
        return self3 && other3 && (self3->hits >> other3->group) & 1;
    }

    bool World::collide()
    {
        m_extents.clear();
        m_extents.reserve(m_colliders.count());
        bool anyone = false;
        for (u32 i = 0; i < m_colliders.count(); i++)
        {
            const Collider& c = m_colliders[i];
            const Entity& e = m_entities[c.owner];
            if (e.dying)
            {
                continue;
            }
            float hx = 0, hy = 0;
            halfExtent(c, hx, hy);
            m_extents.push({ e.world[4] - hx, e.world[4] + hx, e.world[5] - hy, e.world[5] + hy, i });
            anyone = anyone || c.hits;
        }
        m_pairsBefore.swap(m_pairs);
        m_pairs.clear();
        m_stats.pairsTested = 0;
        if (anyone)
        {
            std::sort(m_extents.begin(), m_extents.end(), [](const Extent& a, const Extent& b) { return a.minX < b.minX; });
            for (u32 i = 0; i < m_extents.count(); i++)
            {
                const Extent& a = m_extents[i];
                const Collider& ca = m_colliders[a.collider];
                for (u32 k = i + 1; k < m_extents.count() && m_extents[k].minX <= a.maxX; k++)
                {
                    const Extent& b = m_extents[k];
                    const Collider& cb = m_colliders[b.collider];
                    if (b.minY > a.maxY || b.maxY < a.minY || !((ca.hits >> cb.group) & 1 || (cb.hits >> ca.group) & 1))
                    {
                        continue;
                    }
                    m_stats.pairsTested++;
                    if (overlap(ca, cb))
                    {
                        const u32 x = idOf(ca.owner), y = idOf(cb.owner);
                        m_pairs.push((static_cast<u64>(x < y ? x : y) << 32) | (x < y ? y : x));
                    }
                }
            }
        }
        m_space.collectPairs(m_pairs);
        std::sort(m_pairs.begin(), m_pairs.end());
        m_stats.touching = m_pairs.count();

        // What has begun or ended, worked out before any script runs and changes things.
        m_events.clear();
        u32 i = 0, k = 0;
        while (i < m_pairs.count() || k < m_pairsBefore.count())
        {
            const bool isNew = k == m_pairsBefore.count() || (i < m_pairs.count() && m_pairs[i] < m_pairsBefore[k]);
            const bool isOld = !isNew && (i == m_pairs.count() || m_pairsBefore[k] < m_pairs[i]);
            if (isNew || isOld)
            {
                const u64 pair = isNew ? m_pairs[i] : m_pairsBefore[k];
                const u32 ids[2] = { static_cast<u32>(pair >> 32), static_cast<u32>(pair) };
                for (u32 side = 0; side < 2; side++)
                {
                    if (wantsToKnow(ids[side], ids[1 - side]))
                    {
                        m_events.push({ ids[side], ids[1 - side], isNew });
                    }
                }
            }
            i += isOld ? 0 : 1;
            k += isNew ? 0 : 1;
        }
        for (u32 n = 0; n < m_events.count(); n++)
        {
            const Event e = m_events[n];
            if (alive(e.target) && alive(e.other) && !tell(e.target, e.began ? &Class::onHit : &Class::onLeave, e.other))
            {
                return false;
            }
        }
        return true;
    }

    // --- Maps ---

    u32 World::tileAt(Vec2 position) const
    {
        if (m_mapCount == 0)
        {
            return 0;
        }
        const TileMap& map = m_maps[m_mapCount - 1];
        const float x = floorf((position.x - map.origin.x) / map.size), y = floorf((position.y - map.origin.y) / map.size);
        return x >= 0 && y >= 0 && x < static_cast<float>(map.width) && y < static_cast<float>(map.height) ? map.tiles[static_cast<size_t>(y) * map.width + static_cast<size_t>(x)] : 0;
    }

    u32 World::terrainJoins(const TileMap& map, u32 x, u32 y)
    {
        const u16 tile = map.tiles[static_cast<size_t>(y) * map.width + x];
        const auto same = [&](i32 dx, i32 dy)
        {
            const i32 nx = static_cast<i32>(x) + dx, ny = static_cast<i32>(y) + dy;
            if (nx < 0 || ny < 0 || nx >= static_cast<i32>(map.width) || ny >= static_cast<i32>(map.height))
            {
                return true;
            }
            return map.tiles[static_cast<size_t>(ny) * map.width + static_cast<size_t>(nx)] == tile;
        };
        return (same(0, -1) ? 1u : 0u) | (same(1, 0) ? 2u : 0u) | (same(0, 1) ? 4u : 0u) | (same(-1, 0) ? 8u : 0u);
    }

    int World::tileFrame(Vec2 position) const
    {
        if (m_mapCount == 0)
        {
            return -1;
        }
        const TileMap& map = m_maps[m_mapCount - 1];
        const float fx = floorf((position.x - map.origin.x) / map.size), fy = floorf((position.y - map.origin.y) / map.size);
        if (fx < 0 || fy < 0 || fx >= static_cast<float>(map.width) || fy >= static_cast<float>(map.height))
        {
            return -1;
        }
        const u32 x = static_cast<u32>(fx), y = static_cast<u32>(fy);
        const u32 tile = map.tiles[static_cast<size_t>(y) * map.width + x];
        if (tile == 0)
        {
            return -1;
        }
        const u32 terrain = tile < 64 ? map.terrain[tile] : 0;
        return static_cast<int>(terrain ? terrain - 1 + terrainJoins(map, x, y) : tile - 1);
    }

    void World::setTerrain(u32 tile, int firstFrame)
    {
        if (m_mapCount && tile >= 1 && tile < 64)
        {
            m_maps[m_mapCount - 1].terrain[tile] = static_cast<u16>(firstFrame < 0 ? 0 : firstFrame + 1);
        }
    }

    void World::setTileAt(Vec2 position, u32 tile)
    {
        if (m_mapCount == 0)
        {
            return;
        }
        TileMap& map = m_maps[m_mapCount - 1];
        const float x = floorf((position.x - map.origin.x) / map.size), y = floorf((position.y - map.origin.y) / map.size);
        if (x >= 0 && y >= 0 && x < static_cast<float>(map.width) && y < static_cast<float>(map.height))
        {
            map.tiles[static_cast<size_t>(y) * map.width + static_cast<size_t>(x)] = static_cast<u16>(tile);
            m_walls.clear();
            for (u32 i = 0; i < m_mapCount; i++)
            {
                buildWalls(m_maps[i]);
            }
        }
    }

    Vec2 World::mapSize() const
    {
        if (m_mapCount == 0)
        {
            return {};
        }
        const TileMap& map = m_maps[m_mapCount - 1];
        return { static_cast<float>(map.width) * map.size, static_cast<float>(map.height) * map.size };
    }

    // --- Drawing ---

    void World::setCamera(Vec2 position, float zoom, float rotation)
    {
        m_hasCamera = true;
        m_cameraAt = position;
        m_cameraZoom = zoom > 0.001f ? zoom : 0.001f;
        m_cameraTurn = rotation;
    }

    Vec2 World::toWorld(Vec2 p) const
    {
        if (!m_hasCamera)
        {
            return p;
        }
        const Vec2 d = (p - Vec2(m_viewWidth, m_viewHeight) * 0.5f) / m_cameraZoom;
        const float c = cosf(m_cameraTurn * kDegrees), s = sinf(m_cameraTurn * kDegrees);
        return m_cameraAt + Vec2(d.x * c - d.y * s, d.x * s + d.y * c);
    }

    Vec2 World::toScreen(Vec2 p) const
    {
        if (!m_hasCamera)
        {
            return p;
        }
        const Vec2 d = (p - m_cameraAt) * m_cameraZoom;
        const float c = cosf(-m_cameraTurn * kDegrees), s = sinf(-m_cameraTurn * kDegrees);
        return Vec2(m_viewWidth, m_viewHeight) * 0.5f + Vec2(d.x * c - d.y * s, d.x * s + d.y * c);
    }

    void World::buildDrawList()
    {
        // By layer, then by image, so that what can share a draw call does.
        m_drawList.clear();
        const auto add = [&](int layer, u32 order, u32 sequence, u32 index, u8 kind)
        {
            const u32 place = static_cast<u32>(layer < -32768 ? 0 : layer > 32767 ? 65535 : layer + 32768);
            m_drawList.push({ (static_cast<u64>(place) << 48) | (static_cast<u64>(order & 0xFFFF) << 32) | sequence, index, kind });
        };
        for (u32 i = 0; i < m_mapCount; i++)
        {
            add(m_maps[i].layer, 0, i, i, 2);
        }
        for (u32 i = 0; i < m_visuals.count(); i++)
        {
            const Visual& v = m_visuals[i];
            const u32 image = static_cast<u32>((reinterpret_cast<uintptr_t>(v.asset) >> 4) ^ (reinterpret_cast<uintptr_t>(v.asset) >> 12));
            const u32 order = 1 + ((static_cast<u32>(v.look) << 14) | (image & 0xFFF) << 2 | static_cast<u32>(v.blend));
            add(m_entities[v.owner].layer, order < 0xFFFE ? order : 0xFFFE, v.owner, i, 0);
        }
        for (u32 i = 0; i < m_styles.count(); i++)
        {
            add(m_styles[i].layer, 0xFFFF, i, i, 1);
        }
        std::sort(m_drawList.begin(), m_drawList.end(), [](const DrawItem& a, const DrawItem& b) { return a.key < b.key; });
        m_drawDirty = false;
    }

    void World::draw(Canvas& canvas, int first, int last)
    {
        m_viewWidth = canvas.width();
        m_viewHeight = canvas.height();
        if (m_drawDirty)
        {
            buildDrawList();
        }
        canvas.push();
        float view[4] = { 0, 0, m_viewWidth, m_viewHeight };  // what can be seen: left, top, right, bottom
        if (m_hasCamera)
        {
            canvas.translate(m_viewWidth * 0.5f, m_viewHeight * 0.5f);
            canvas.scale(m_cameraZoom, m_cameraZoom);
            canvas.rotate(-m_cameraTurn);
            canvas.translate(-m_cameraAt.x, -m_cameraAt.y);
            float halfX = m_viewWidth * 0.5f / m_cameraZoom, halfY = m_viewHeight * 0.5f / m_cameraZoom;
            if (m_cameraTurn != 0)
            {
                halfX = halfY = sqrtf(halfX * halfX + halfY * halfY);
            }
            view[0] = m_cameraAt.x - halfX;
            view[1] = m_cameraAt.y - halfY;
            view[2] = m_cameraAt.x + halfX;
            view[3] = m_cameraAt.y + halfY;
        }
        m_stats.drawn = 0;
        gpu::Blend blend = gpu::Blend::Alpha;
        canvas.setBlend(blend);
        bool lit = !lightingOn() || first > m_lightLayer;
        for (const DrawItem& item : m_drawList)
        {
            const int layer = static_cast<int>(item.key >> 48) - 32768;
            if (layer < first)
            {
                continue;
            }
            if (layer > last)
            {
                break;
            }
            if (!lit && layer > m_lightLayer)
            {
                drawLights(canvas, view);
                lit = true;
                blend = gpu::Blend::Alpha;
                canvas.setBlend(blend);
            }
            const gpu::Blend wanted = item.kind == 0 ? m_visuals[item.index].blend : item.kind == 1 ? m_styles[item.index].blend : gpu::Blend::Alpha;
            if (wanted != blend)
            {
                blend = wanted;
                canvas.setBlend(blend);
            }
            if (item.kind == 0)
            {
                drawVisual(canvas, m_visuals[item.index], view);
            }
            else if (item.kind == 1)
            {
                drawParticles(canvas, item.index);
            }
            else
            {
                drawMap(canvas, m_maps[item.index], view);
            }
        }
        if (!lit && last >= m_lightLayer)
        {
            drawLights(canvas, view);
            canvas.setBlend(gpu::Blend::Alpha);
        }
        if (m_showShapes)
        {
            // Every collider, a line as wide on the screen however near the camera is.
            const float width = 1.5f / (m_hasCamera && m_cameraZoom > 0 ? m_cameraZoom : 1);
            for (const Collider& c : m_colliders)
            {
                const Entity& e = m_entities[c.owner];
                if (!e.live || e.dying)
                {
                    continue;
                }
                const u32 b = e.parts[static_cast<size_t>(Component::Body)];
                const Body* body = b != kNone ? &m_bodies[b] : nullptr;
                const Color color = c.wall ? Color(0.75f, 0.75f, 0.75f, 0.9f) : body && body->mass > 0 ? Color(0.35f, 1, 0.45f, 0.9f)
                                  : body ? Color(0.35f, 0.7f, 1, 0.9f) : Color(1, 0.85f, 0.3f, 0.9f);
                const Placed p(c, e.world);
                if (c.shape == Shape::Circle)
                {
                    canvas.ring(p.at.x, p.at.y, p.hx, width, color);
                    canvas.line(p.at.x, p.at.y, p.at.x + p.u.x * p.hx, p.at.y + p.u.y * p.hx, width, color);
                    continue;
                }
                const Vec2 x = p.u * p.hx, y = p.v * p.hy;
                const Vec2 corners[4] = { p.at - x - y, p.at + x - y, p.at + x + y, p.at - x + y };
                for (u32 i = 0; i < 4; i++)
                {
                    canvas.line(corners[i].x, corners[i].y, corners[(i + 1) % 4].x, corners[(i + 1) % 4].y, width, color);
                }
            }
        }
        canvas.pop();
    }

    void World::drawVisual(Canvas& canvas, const Visual& v, const float (&view)[4])
    {
        const Entity& e = m_entities[v.owner];
        if (!e.shown || e.dying || v.color.a <= 0)
        {
            return;
        }
        float w = v.width, h = v.height;
        if (v.look == Look::Image)
        {
            if (v.asset == nullptr || !v.asset->ready || v.columns == 0 || v.rows == 0)
            {
                return;
            }
            w = w > 0 ? w : static_cast<float>(v.asset->width) / v.columns;
            h = h > 0 ? h : static_cast<float>(v.asset->height) / v.rows;
        }
        if (v.look != Look::Text)
        {
            const float* m = e.world;
            const float rx = (fabsf(m[0]) * w + fabsf(m[2]) * h) * 0.5f, ry = (fabsf(m[1]) * w + fabsf(m[3]) * h) * 0.5f;
            if (m[4] + rx < view[0] || m[4] - rx > view[2] || m[5] + ry < view[1] || m[5] - ry > view[3])
            {
                return;
            }
        }
        m_stats.drawn++;
        switch (v.look)
        {
        case Look::Image:
        {
            const u32 frame = v.frame % (static_cast<u32>(v.columns) * v.rows);
            const float du = 1.0f / v.columns, dv = 1.0f / v.rows;
            float u0 = static_cast<float>(frame % v.columns) * du, u1 = u0 + du;
            const float v0 = static_cast<float>(frame / v.columns) * dv;
            if (v.flipX)
            {
                const float swap = u0;
                u0 = u1;
                u1 = swap;
            }
            canvas.placed(v.asset, e.world, w, h, u0, v0, u1, v0 + dv, v.color);
            break;
        }
        case Look::Rect:
            canvas.placed(nullptr, e.world, w, h, 0, 0, 1, 1, v.color);
            break;
        case Look::Disc:
            canvas.push();
            canvas.transform(e.world);
            canvas.circle(0, 0, w * 0.5f, v.color);
            canvas.pop();
            break;
        case Look::Text:
            if (v.asset && v.asset->font && v.text)
            {
                canvas.push();
                canvas.transform(e.world);
                canvas.text(*v.asset, v.text, 0, -v.asset->font->lineHeight * v.asset->font->scale * 0.5f, v.color, Align::Center);
                canvas.pop();
            }
            break;
        }
    }

    void World::drawParticles(Canvas& canvas, u32 index)
    {
        const ParticleStyle& s = m_styles[index];
        if (s.texture && !s.texture->ready)
        {
            return;
        }
        for (const Particle& p : m_particles[index])
        {
            const float t = p.age / p.life;
            const float size = s.sizeStart + (s.sizeEnd - s.sizeStart) * t;
            Color color = { s.colorStart.r + (s.colorEnd.r - s.colorStart.r) * t, s.colorStart.g + (s.colorEnd.g - s.colorStart.g) * t,
                s.colorStart.b + (s.colorEnd.b - s.colorStart.b) * t, s.colorStart.a + (s.colorEnd.a - s.colorStart.a) * t };
            color.a *= s.fadeIn > 0 && t < s.fadeIn ? t / s.fadeIn : 1;
            if (s.texture)
            {
                const float m[6] = { 1, 0, 0, 1, p.position.x, p.position.y };
                canvas.placed(s.texture, m, size, size, 0, 0, 1, 1, color);
            }
            else
            {
                canvas.glow(p.position.x, p.position.y, size * 0.5f, color);
            }
        }
    }

    void World::drawMap(Canvas& canvas, const TileMap& map, const float (&view)[4])
    {
        if (map.texture && !map.texture->ready)
        {
            return;
        }
        const auto cell = [&](float at, float origin, u32 count)
        {
            const float c = floorf((at - origin) / map.size);
            return c < 0 ? 0u : c > static_cast<float>(count) ? count : static_cast<u32>(c);
        };
        const u32 x0 = cell(view[0], map.origin.x, map.width), x1 = cell(view[2], map.origin.x, map.width);
        const u32 y0 = cell(view[1], map.origin.y, map.height), y1 = cell(view[3], map.origin.y, map.height);
        for (u32 y = y0; y <= y1 && y < map.height; y++)
        {
            for (u32 x = x0; x <= x1 && x < map.width; x++)
            {
                const u32 tile = map.tiles[static_cast<size_t>(y) * map.width + x];
                if (tile == 0)
                {
                    continue;
                }
                const float m[6] = { 1, 0, 0, 1, map.origin.x + (static_cast<float>(x) + 0.5f) * map.size, map.origin.y + (static_cast<float>(y) + 0.5f) * map.size };
                // A terrain: which of its sides meet more of it. Past the map's edge counts as more, so the edge isn't one.
                const u32 terrain = tile < 64 ? map.terrain[tile] : 0;
                const u32 joins = terrain ? terrainJoins(map, x, y) : 0;
                if (terrain && !map.texture)
                {
                    drawTerrainTile(canvas, map, x, y, map.colors[tile & 15], joins);
                    m_stats.drawn++;
                    continue;
                }
                if (map.texture)
                {
                    const u32 frame = (terrain ? terrain - 1 + joins : tile - 1) % (map.columns * map.rows);
                    // In a little from the edges, so a neighbour in the sheet doesn't show.
                    const float eu = 0.5f / static_cast<float>(map.texture->width), ev = 0.5f / static_cast<float>(map.texture->height);
                    const float du = 1.0f / static_cast<float>(map.columns), dv = 1.0f / static_cast<float>(map.rows);
                    const float u = static_cast<float>(frame % map.columns) * du, v = static_cast<float>(frame / map.columns) * dv;
                    canvas.placed(map.texture, m, map.size, map.size, u + eu, v + ev, u + du - eu, v + dv - ev, { 1, 1, 1, 1 });
                }
                else
                {
                    canvas.placed(nullptr, m, map.size, map.size, 0, 0, 1, 1, map.colors[tile & 15]);
                }
                m_stats.drawn++;
            }
        }
    }

    // Without a sheet: the colour, with a light top and a shadowed bottom where a side is open, and outer corners rounded.
    void World::drawTerrainTile(Canvas& canvas, const TileMap& map, u32 x, u32 y, const Color& color, u32 joins)
    {
        const float s = map.size, left = map.origin.x + static_cast<float>(x) * s, top = map.origin.y + static_cast<float>(y) * s;
        const float r = s * 0.3f, band = s * 0.14f;
        const bool up = joins & 1, right = joins & 2, down = joins & 4, leftSide = joins & 8;
        const bool nw = !up && !leftSide, ne = !up && !right, se = !down && !right, sw = !down && !leftSide;
        // Three strips, leaving out the corners that are rounded, and a quarter of a circle in each of those.
        canvas.rect(left + (nw ? r : 0), top, s - (nw ? r : 0) - (ne ? r : 0), r, color);
        canvas.rect(left, top + r, s, s - 2 * r, color);
        canvas.rect(left + (sw ? r : 0), top + s - r, s - (sw ? r : 0) - (se ? r : 0), r, color);
        const float corners[4][2] = { { left + r, top + r }, { left + s - r, top + r }, { left + s - r, top + s - r }, { left + r, top + s - r } };
        const bool rounded[4] = { nw, ne, se, sw };
        for (u32 i = 0; i < 4; i++)
        {
            if (rounded[i])
            {
                canvas.circle(corners[i][0], corners[i][1], r, color);
            }
        }
        const Color light = { color.r + (1 - color.r) * 0.35f, color.g + (1 - color.g) * 0.35f, color.b + (1 - color.b) * 0.35f, color.a };
        const Color dark = { color.r * 0.6f, color.g * 0.6f, color.b * 0.6f, color.a };
        if (!up)
        {
            canvas.rect(left + (nw ? r : 0), top, s - (nw ? r : 0) - (ne ? r : 0), band, light);
        }
        if (!down)
        {
            canvas.rect(left + (sw ? r : 0), top + s - band, s - (sw ? r : 0) - (se ? r : 0), band, dark);
        }
    }

    const WorldStats& World::stats()
    {
        m_stats.entities = m_live;
        m_stats.particles = m_particleCount + m_space.particles();
        return m_stats;
    }
}
