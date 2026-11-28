#include "bindings/script_refs.h"

#include <scriptarray/scriptarray.h>

#include <stdio.h>
#include <string.h>

namespace nx
{
    namespace
    {
        // Given to every module, so that a behaviour has its entity and need only write what it uses.
        const char kPrelude[] =
            "shared abstract class Behaviour : IBehaviour\n"
            "{\n"
            "    Entity@ entity;\n"
            "    void start() {}\n"
            "    void update(float dt) {}\n"
            "    void onHit(Entity@ other) {}\n"
            "    void onLeave(Entity@ other) {}\n"
            "    void onGone() {}\n"
            "}\n";

        World& world()
        {
            return *services().world;
        }

        // --- Entity ---

        World::Entity* entityOf(const EntityRef* e)
        {
            return e->world ? e->world->find(e->id) : nullptr;
        }

        bool entityAlive(const EntityRef* e) { return entityOf(e) != nullptr; }
        void entityDestroy(EntityRef* e) { if (e->world) e->world->destroyEntity(e->id); }

        Vec2 getPosition(const EntityRef* e) { const World::Entity* x = entityOf(e); return x ? x->position : Vec2(); }
        void setPosition(const Vec2& v, EntityRef* e) { if (World::Entity* x = entityOf(e)) x->position = v; }
        float getX(const EntityRef* e) { return getPosition(e).x; }
        float getY(const EntityRef* e) { return getPosition(e).y; }
        void setX(float v, EntityRef* e) { if (World::Entity* x = entityOf(e)) x->position.x = v; }
        void setY(float v, EntityRef* e) { if (World::Entity* x = entityOf(e)) x->position.y = v; }
        Vec2 getWorldPosition(const EntityRef* e) { return e->world ? e->world->worldPosition(e->id) : Vec2(); }
        float getRotation(const EntityRef* e) { const World::Entity* x = entityOf(e); return x ? x->rotation : 0; }
        void setRotation(float v, EntityRef* e) { if (World::Entity* x = entityOf(e)) x->rotation = v; }
        Vec2 getScale(const EntityRef* e) { const World::Entity* x = entityOf(e); return x ? x->scale : Vec2(); }
        void setScale(const Vec2& v, EntityRef* e) { if (World::Entity* x = entityOf(e)) x->scale = v; }
        bool getVisible(const EntityRef* e) { const World::Entity* x = entityOf(e); return x && x->visible; }
        void setVisible(bool v, EntityRef* e) { if (World::Entity* x = entityOf(e)) x->visible = v; }
        int getLayer(const EntityRef* e) { const World::Entity* x = entityOf(e); return x ? x->layer : 0; }
        void setLayer(int v, EntityRef* e) { if (e->world) e->world->setLayer(e->id, v); }
        std::string getName(const EntityRef* e) { const World::Entity* x = entityOf(e); return x ? x->name : ""; }
        void setName(const std::string& v, EntityRef* e) { if (World::Entity* x = entityOf(e)) snprintf(x->name, sizeof(x->name), "%s", v.c_str()); }
        EntityRef* getParent(const EntityRef* e) { return e->world ? e->world->ref(e->world->parentOf(e->id)) : nullptr; }
        void setParent(const EntityRef* parent, EntityRef* e) { if (e->world) e->world->setParent(e->id, parent ? parent->id : 0); }

        // In space rather than on the plane.
        bool inSpace(const EntityRef* e) { return e->world && e->world->space().getSpatial(e->id); }
        Appearance* appearanceOf(const EntityRef* e) { return e->world ? e->world->space().getAppearance(e->id) : nullptr; }
        Body3* body3Of(const EntityRef* e) { return inSpace(e) ? e->world->space().body(e->id) : nullptr; }
        const Body3* someBody3(const EntityRef* e) { return e->world ? e->world->space().getBody(e->id) : nullptr; }
        Collider3* collider3Of(const EntityRef* e) { return e->world ? e->world->space().getCollider(e->id) : nullptr; }

        Visual* visualOf(EntityRef* e) { return e->world ? e->world->getVisual(e->id) : nullptr; }
        Color getColor(EntityRef* e)
        {
            if (const Appearance* a = appearanceOf(e))
            {
                return a->color;
            }
            const Visual* v = visualOf(e);
            return v ? v->color : Color{ 1, 1, 1, 1 };
        }
        void setColor(const Color& c, EntityRef* e)
        {
            if (Appearance* a = appearanceOf(e))
            {
                a->color = c;
            }
            else if (e->world && !inSpace(e))
            {
                if (Visual* v = e->world->visual(e->id))
                {
                    v->color = c;
                }
            }
        }
        float getAlpha(EntityRef* e) { return getColor(e).a; }
        void setAlpha(float a, EntityRef* e)
        {
            if (Appearance* x = appearanceOf(e))
            {
                x->color.a = a;
            }
            else if (Visual* v = visualOf(e))
            {
                v->color.a = a;
            }
        }
        bool getFlip(EntityRef* e) { const Visual* v = visualOf(e); return v && v->flipX; }
        void setFlip(bool f, EntityRef* e) { if (Visual* v = visualOf(e)) v->flipX = f; }
        u32 getFrame(EntityRef* e) { const Visual* v = visualOf(e); return v ? v->frame : 0; }
        void setFrame(u32 f, EntityRef* e) { if (Visual* v = visualOf(e)) v->frame = static_cast<u16>(f); }
        void setBlend(gpu::Blend b, EntityRef* e)
        {
            if (static_cast<u32>(b) >= 3 || e->world == nullptr)
            {
                return;
            }
            if (Appearance* a = appearanceOf(e))
            {
                a->blend = b;
            }
            else if (Visual* v = e->world->visual(e->id))
            {
                v->blend = b;
            }
        }

        void look(EntityRef* e, Look kind, Asset* asset, float w, float h)
        {
            if (e->world == nullptr)
            {
                return;
            }
            e->world->setLook(e->id, kind, asset);
            if (Visual* v = visualOf(e))
            {
                v->width = w;
                v->height = h;
            }
        }
        void entityImage(const TextureRef* texture, float w, float h, EntityRef* e)
        {
            if (texture == nullptr)
            {
                scriptError("null texture");
                return;
            }
            look(e, Look::Image, texture->asset, w, h);
        }
        void entityRect(float w, float h, const Color& c, EntityRef* e) { look(e, Look::Rect, nullptr, w, h); setColor(c, e); }
        void entityDisc(float r, const Color& c, EntityRef* e) { look(e, Look::Disc, nullptr, r * 2, r * 2); setColor(c, e); }
        void entityText(const FontRef* font, const std::string& text, const Color& c, EntityRef* e)
        {
            if (font == nullptr)
            {
                scriptError("null font");
                return;
            }
            look(e, Look::Text, font->asset, 0, 0);
            e->world ? e->world->setText(e->id, text.c_str()) : void();
            setColor(c, e);
        }
        void entitySetText(const std::string& text, EntityRef* e) { if (e->world) e->world->setText(e->id, text.c_str()); }
        std::string entityGetText(EntityRef* e) { const Visual* v = visualOf(e); return v && v->text ? v->text : ""; }
        void entitySheet(u32 columns, u32 rows, EntityRef* e)
        {
            if (Visual* v = visualOf(e))
            {
                v->columns = static_cast<u16>(columns < 1 ? 1 : columns > 1024 ? 1024 : columns);
                v->rows = static_cast<u16>(rows < 1 ? 1 : rows > 1024 ? 1024 : rows);
            }
        }
        void entityPlay(u32 first, u32 last, float fps, bool loop, EntityRef* e)
        {
            Animation* a = e->world ? e->world->animation(e->id) : nullptr;
            // Asking for what is already playing carries on with it.
            if (a && (a->first != first || a->last != last || a->fps != fps || a->loop != loop || a->done))
            {
                a->first = static_cast<u16>(first);
                a->last = static_cast<u16>(last < first ? first : last);
                a->fps = fps;
                a->loop = loop;
                a->done = false;
                a->at = 0;
                setFrame(first, e);
            }
        }
        bool entityPlayed(EntityRef* e) { const Animation* a = e->world ? e->world->getAnimation(e->id) : nullptr; return a == nullptr || a->done; }

        Body* bodyOf(EntityRef* e) { return e->world ? e->world->body(e->id) : nullptr; }
        const Body* someBody(const EntityRef* e) { return e->world ? e->world->getBody(e->id) : nullptr; }
        Vec2 getVelocity(const EntityRef* e) { const Body* b = someBody(e); return b ? b->velocity : Vec2(); }
        void setVelocity(const Vec2& v, EntityRef* e) { if (Body* b = bodyOf(e)) b->velocity = v; }
        float getSpin(const EntityRef* e) { const Body* b = someBody(e); return b ? b->spin : 0; }
        void setSpin(float v, EntityRef* e) { if (Body* b = bodyOf(e)) b->spin = v; }
        void setDrag(float v, EntityRef* e)
        {
            if (Body3* s = body3Of(e))
            {
                s->drag = v;
            }
            else if (Body* b = bodyOf(e))
            {
                b->drag = v;
            }
        }
        void setGravity(float v, EntityRef* e)
        {
            if (Body3* s = body3Of(e))
            {
                s->gravity = v;
            }
            else if (Body* b = bodyOf(e))
            {
                b->gravity = v;
            }
        }
        void setBounce(float v, EntityRef* e)
        {
            if (Body3* s = body3Of(e))
            {
                s->bounce = v;
            }
            else if (Body* b = bodyOf(e))
            {
                b->bounce = v;
            }
        }
        void setFriction(float v, EntityRef* e)
        {
            if (Body3* s = body3Of(e))
            {
                s->friction = v < 0 ? 0 : v;
            }
            else if (Body* b = bodyOf(e))
            {
                b->friction = v < 0 ? 0 : v;
            }
        }
        float getMass(const EntityRef* e)
        {
            const Body3* s = someBody3(e);
            const Body* b = someBody(e);
            return s ? s->mass : b ? b->mass : 0;
        }
        void setMass(float v, EntityRef* e)
        {
            if (Body3* s = body3Of(e))
            {
                s->mass = v < 0 ? 0 : v;
            }
            else if (Body* b = bodyOf(e))
            {
                b->mass = v < 0 ? 0 : v;
            }
        }
        // Of either kind of body.
        float bodyValue(const EntityRef* e, float Body3::*in3, float Body::*in2)
        {
            const Body3* s = someBody3(e);
            const Body* b = someBody(e);
            return s ? s->*in3 : b ? b->*in2 : 0;
        }
        float getDrag(const EntityRef* e) { return bodyValue(e, &Body3::drag, &Body::drag); }
        float getGravity(const EntityRef* e) { return bodyValue(e, &Body3::gravity, &Body::gravity); }
        float getBounce(const EntityRef* e) { return bodyValue(e, &Body3::bounce, &Body::bounce); }
        float getFriction(const EntityRef* e) { return bodyValue(e, &Body3::friction, &Body::friction); }
        bool getSolid(const EntityRef* e) { const Body* b = someBody(e); return b && b->solid; }
        int getBlend(EntityRef* e)   // a whole int, as scripts' enums are
        {
            const Appearance* a = appearanceOf(e);
            const Visual* v = visualOf(e);
            return static_cast<int>(a ? a->blend : v ? v->blend : gpu::Blend::Alpha);
        }
        void setSolid(bool v, EntityRef* e) { if (Body* b = bodyOf(e)) b->solid = v; }
        bool getOnGround(const EntityRef* e)
        {
            const Body3* s = someBody3(e);
            const Body* b = someBody(e);
            return s ? s->onGround : b && b->onGround;
        }
        bool getOnCeiling(const EntityRef* e) { const Body* b = someBody(e); return b && b->onCeiling; }
        int getOnWall(const EntityRef* e) { const Body* b = someBody(e); return b ? b->onWall : 0; }
        void entityPush(const Vec2& impulse, EntityRef* e) { if (e->world) e->world->push(e->id, impulse, nullptr); }
        void entityPushAt(const Vec2& impulse, const Vec2& at, EntityRef* e) { if (e->world) e->world->push(e->id, impulse, &at); }

        void collide(EntityRef* e, Shape shape, float w, float h, u32 group, u32 hits)
        {
            Collider* c = e->world ? e->world->collider(e->id) : nullptr;
            if (group >= kGroups)
            {
                scriptError("groups are 0 to 15");
            }
            else if (c)
            {
                c->shape = shape;
                c->halfWidth = w;
                c->halfHeight = h;
                c->group = static_cast<u8>(group);
                c->hits = static_cast<u16>(hits);
            }
        }
        void entityCircle(float r, u32 group, u32 hits, EntityRef* e) { collide(e, Shape::Circle, r, r, group, hits); }
        void entityBox(float w, float h, u32 group, u32 hits, EntityRef* e) { collide(e, Shape::Box, w * 0.5f, h * 0.5f, group, hits); }
        void entityPolygon(const CScriptArray* points, u32 group, u32 hits, EntityRef* e)
        {
            Vec2 p[64];
            const u32 n = copyArray(points, p, 64);
            if (group >= kGroups)
            {
                scriptError("groups are 0 to 15");
            }
            else if (!e->world || !e->world->setPolygon(e->id, p, n, group, hits))
            {
                scriptError("a polygon needs three corners or more, not all in a line");
            }
        }
        void entityChain(const CScriptArray* points, bool loop, u32 group, u32 hits, EntityRef* e)
        {
            Vec2 p[256];
            const u32 n = points && points->GetSize() <= 256 ? copyArray(points, p, 256) : 0;
            if (group >= kGroups)
            {
                scriptError("groups are 0 to 15");
            }
            else if (!e->world || n < 2 || !e->world->setChain(e->id, p, n, loop, group, hits))
            {
                scriptError("a chain needs 2 to 256 points");
            }
        }
        bool getOneWay(const EntityRef* e) { const Collider* c = e->world ? e->world->getCollider(e->id) : nullptr; return c && c->oneWay; }
        void setOneWay(bool v, EntityRef* e) { if (Collider* c = e->world ? e->world->getCollider(e->id) : nullptr) c->oneWay = v; }
        void setWall(bool v, EntityRef* e)
        {
            if (Collider3* s = collider3Of(e))
            {
                s->wall = v;
            }
            else if (Collider* c = e->world ? e->world->collider(e->id) : nullptr)
            {
                c->wall = v;
            }
        }
        int getGroup(const EntityRef* e)
        {
            const Collider3* s = collider3Of(e);
            const Collider* c = e->world ? e->world->getCollider(e->id) : nullptr;
            return s ? s->group : c ? c->group : -1;
        }
        void setHits(u32 hits, EntityRef* e)
        {
            if (Collider3* s = collider3Of(e))
            {
                s->hits = static_cast<u16>(hits);
            }
            else if (Collider* c = e->world ? e->world->getCollider(e->id) : nullptr)
            {
                c->hits = static_cast<u16>(hits);
            }
        }
        bool entityTouching(const EntityRef* other, EntityRef* e) { return other && e->world && (e->world->touching(e->id, other->id) || e->world->space().touching(e->id, other->id)); }
        void setOccluder(bool v, EntityRef* e)
        {
            if (Collider* c = e->world ? e->world->getCollider(e->id) : nullptr)
            {
                c->occluder = v;
            }
        }
        bool getOccluder(const EntityRef* e)
        {
            const Collider* c = e->world ? e->world->getCollider(e->id) : nullptr;
            return c && c->occluder;
        }
        bool getWall(const EntityRef* e)
        {
            const Collider3* s = collider3Of(e);
            const Collider* c = e->world ? e->world->getCollider(e->id) : nullptr;
            return s ? s->wall : c && c->wall;
        }
        u32 getHits(const EntityRef* e)
        {
            const Collider3* s = collider3Of(e);
            const Collider* c = e->world ? e->world->getCollider(e->id) : nullptr;
            return s ? s->hits : c ? c->hits : 0u;
        }
        float getLifetime(const EntityRef* e) { const Lifetime* l = e->world ? e->world->getLifetime(e->id) : nullptr; return l ? l->left : -1; }
        void setLifetime(float seconds, EntityRef* e) { if (e->world) if (Lifetime* l = e->world->lifetime(e->id)) l->left = seconds; }

        void entityAdd(asIScriptObject* behaviour, EntityRef* e)
        {
            if (behaviour == nullptr)
            {
                scriptError("null behaviour");
            }
            else if (e->world && e->world->alive(e->id) && !e->world->addBehaviour(e->id, behaviour))
            {
                scriptError("a behaviour is a class that comes from Behaviour");
            }
        }
        asIScriptObject* entityBehaviour(const EntityRef* e)
        {
            asIScriptObject* object = e->world ? e->world->behaviour(e->id) : nullptr;
            if (object)
            {
                object->AddRef();
            }
            return object;
        }

        // --- scene:: ---

        EntityRef* sceneSpawn(const Vec2& at)
        {
            const u32 id = world().spawn(at);
            if (id == 0)
            {
                scriptError("too many entities");
            }
            return world().ref(id);
        }

        EntityRef* sceneSpawnPrefab(const std::string& name, const Vec2& at)
        {
            const u32 id = world().spawnPrefab(name.c_str(), at);
            if (id == 0)
            {
                scriptError(world().takeError());
            }
            return world().ref(id);
        }

        void sceneLoad(const std::string& name)
        {
            Services& s = services();
            char path[192];
            if (!resolveAsset(s, name.c_str(), path, sizeof(path)))
            {
                scriptError("asset names start with asset: or global: (" + name + ")");
            }
            else if (!world().load(path, s.folder))
            {
                scriptError(world().takeError());
            }
        }

        void sceneDraw(Canvas* canvas, int first, int last)
        {
            if (canvas)
            {
                world().draw(*canvas, first, last);
            }
        }
        void sceneCamera(const Vec2& at, float zoom, float rotation) { world().setCamera(at, zoom, rotation); }
        Vec2 sceneToWorld(const Vec2& p) { return world().toWorld(p); }
        Vec2 sceneToScreen(const Vec2& p) { return world().toScreen(p); }
        u32 sceneCount() { return world().count(); }
        u32 sceneCountGroup(u32 group) { return world().countGroup(group); }
        u32 sceneParticles() { return world().stats().particles; }
        void sceneClearParticles() { world().clearParticles(); }
        u32 sceneDrawn() { return world().stats().drawn; }
        u32 scenePairs() { return world().stats().pairsTested; }
        u32 sceneContacts() { return world().stats().contacts; }
        int sceneGroup(const std::string& name)
        {
            const u32 group = world().groupNamed(name.c_str());
            return group < kGroups ? static_cast<int>(group) : -1;
        }
        EntityRef* sceneFind(const std::string& name) { return world().ref(world().findNamed(name.c_str())); }
        EntityRef* sceneNearest(const Vec2& at, float radius, u32 groups, const EntityRef* except) { return world().ref(world().nearest(at, radius, groups, except ? except->id : 0)); }
        EntityRef* sceneRaycast(const Vec2& from, const Vec2& to, Vec2& hit, u32 groups) { return world().ref(world().raycast(from, to, groups, hit)); }
        CScriptArray* sceneWithin(const Vec2& at, float radius, u32 groups)
        {
            CScriptArray* list = CScriptArray::Create(services().entityArray);
            world().within(at, radius, groups, [&](u32 id)
            {
                EntityRef* e = world().ref(id);
                list->InsertLast(&e);  // adds its own
                e->release();
            });
            return list;
        }
        Vec2 sceneGetGravity() { return world().gravity(); }
        bool sceneShowShapes() { return world().m_showShapes; }
        void sceneSetShowShapes(bool v) { world().m_showShapes = v; }
        void sceneGravity(const Vec2& g) { world().setGravity(g); }
        Color sceneAmbient() { return world().ambient(); }
        void sceneSetAmbient(const Color& c) { world().setAmbient(c); }
        int sceneLightLayer() { return world().lightLayer(); }
        void sceneSetLightLayer(int layer) { world().setLightLayer(layer); }
        void sceneBurst(const std::string& name, const Vec2& at, u32 count)
        {
            if (!world().burst(name.c_str(), at, count))
            {
                scriptError("no particles called " + name);
            }
        }
        bool sceneJoin(const EntityRef* a, const EntityRef* b, const Vec2& at, float softness) { return a && world().join(a->id, b ? b->id : 0, at, softness); }
        u32 idOrZero(const EntityRef* e) { return e ? e->id : 0; }
        bool sceneHinge(const EntityRef* a, const EntityRef* b, const Vec2& at, float low, float high, float speed, float torque)
        {
            return a && world().hinge(a->id, idOrZero(b), at, low, high, speed, torque);
        }
        bool sceneWeld(const EntityRef* a, const EntityRef* b, const Vec2& at) { return a && world().weld(a->id, idOrZero(b), at); }
        bool sceneRope(const EntityRef* a, const EntityRef* b, const Vec2& atA, const Vec2& atB, float length) { return a && world().rope(a->id, idOrZero(b), atA, atB, length); }
        bool sceneSpring(const EntityRef* a, const EntityRef* b, const Vec2& atA, const Vec2& atB, float frequency, float damping, float length)
        {
            return a && world().spring(a->id, idOrZero(b), atA, atB, frequency, damping, length);
        }
        bool sceneSlider(const EntityRef* a, const EntityRef* b, const Vec2& at, const Vec2& axis, float low, float high)
        {
            return a && world().slider(a->id, idOrZero(b), at, axis, low, high);
        }
        bool sceneWheel(const EntityRef* a, const EntityRef* b, const Vec2& axis, float frequency, float damping, float speed, float torque)
        {
            return a && world().wheel(a->id, idOrZero(b), axis, frequency, damping, speed, torque);
        }
        bool scenePulley(const EntityRef* a, const EntityRef* b, const Vec2& groundA, const Vec2& groundB, const Vec2& atA, const Vec2& atB, float ratio)
        {
            return a && b && world().pulley(a->id, b->id, groundA, groundB, atA, atB, ratio);
        }
        bool sceneGear(const EntityRef* a, const EntityRef* b, float ratio) { return a && b && world().gear(a->id, b->id, ratio); }
        bool sceneFriction(const EntityRef* a, const EntityRef* b, float strength) { return a && world().friction(a->id, idOrZero(b), strength); }
        bool sceneFollow(const EntityRef* a, const EntityRef* b, float strength, float correction) { return a && b && world().follow(a->id, b->id, strength, correction); }
        void sceneFollowTarget(const EntityRef* a, const EntityRef* b, const Vec2& offset, float degrees)
        {
            if (a && b)
            {
                world().followTarget(a->id, b->id, offset, degrees);
            }
        }
        bool sceneDrag(const EntityRef* a, const Vec2& at, float strength) { return a && world().drag(a->id, at, strength); }
        void sceneDragTo(const EntityRef* a, const Vec2& to) { if (a) world().dragTo(a->id, to); }
        void sceneMotor(const EntityRef* a, float speed, float torque) { if (a) world().setMotor(a->id, speed, torque); }
        void sceneUnjoin(const EntityRef* a) { if (a) world().unjoin(a->id); }
        bool sceneContact(const EntityRef* a, const EntityRef* b, Vec2& at, Vec2& normal, float& impulse)
        {
            at = normal = {};
            impulse = 0;
            return a && b && world().contact(a->id, b->id, at, normal, impulse);
        }
        bool scenePath(const Vec2& from, const Vec2& to, CScriptArray* out, bool diagonal)
        {
            GrowArray<Vec2> way;
            const bool reached = world().findPath(from, to, diagonal, way);
            if (out)
            {
                out->Resize(way.count());
                for (u32 i = 0; i < way.count(); i++)
                {
                    *static_cast<Vec2*>(out->At(i)) = way[i];
                }
            }
            return reached;
        }
        bool sceneBlocked(const Vec2& at) { return world().blockedAt(at); }
        u32 sceneTile(const Vec2& at) { return world().tileAt(at); }
        void sceneSetTile(const Vec2& at, u32 tile) { world().setTileAt(at, tile); }
        void sceneTerrain(u32 tile, int first) { world().setTerrain(tile, first); }
        Vec2 sceneMapSize() { return world().mapSize(); }
    }

    void bindScene(Binder& b)
    {
        b.refType("Entity", 0, "Something in the scene. A handle is always safe: once the entity has been destroyed, alive is false and nothing done to it has any effect.");
        b.behaviour(asBEHAVE_ADDREF, "void f()", NX_METHOD(EntityRef, addRef));
        b.behaviour(asBEHAVE_RELEASE, "void f()", NX_METHOD(EntityRef, release));
        b.interfaceType("IBehaviour", "What Behaviour comes from. Use Behaviour.");
        b.section("Behaviour", "A script on an entity. Write a class that comes from it and any of these methods; `entity` is the entity it is on.");
        b.callback("Entity@ entity", "The entity.");
        b.callback("void start()", "Called once, before its first update.");
        b.callback("void update(float dt)", "Called 60 times a second.");
        b.callback("void onHit(Entity@ other)", "Called when its collider begins to touch one in a group it hits.");
        b.callback("void onLeave(Entity@ other)", "Called when they stop touching.");
        b.callback("void onGone()", "Called once, as the entity is destroyed.");

        b.reopen("Entity");
        b.method("bool get_alive() const property", NX_OBJ_LAST(entityAlive), "Whether it is still there.");
        b.method("void destroy()", NX_OBJ_LAST(entityDestroy), "Removes it and its children, at the end of this step.");
        b.method("string get_name() const property", NX_OBJ_LAST(getName), "A name to find it by, up to 23 letters.");
        b.method("void set_name(const string &in) property", NX_OBJ_LAST(setName), "Names it.");
        b.method("Vec2 get_position() const property", NX_OBJ_LAST(getPosition), "Where it is, from its parent if it has one.");
        b.method("void set_position(const Vec2 &in) property", NX_OBJ_LAST(setPosition), "Moves it.");
        b.method("float get_x() const property", NX_OBJ_LAST(getX), "Part of its position.");
        b.method("void set_x(float) property", NX_OBJ_LAST(setX), "Moves it.");
        b.method("float get_y() const property", NX_OBJ_LAST(getY), "Part of its position.");
        b.method("void set_y(float) property", NX_OBJ_LAST(setY), "Moves it.");
        b.method("Vec2 get_worldPosition() const property", NX_OBJ_LAST(getWorldPosition), "Where it is in the scene, whatever its parents.");
        b.method("float get_rotation() const property", NX_OBJ_LAST(getRotation), "Degrees clockwise.");
        b.method("void set_rotation(float) property", NX_OBJ_LAST(setRotation), "Turns it.");
        b.method("Vec2 get_scale() const property", NX_OBJ_LAST(getScale), "Its size, across and down; 1 is normal.");
        b.method("void set_scale(const Vec2 &in) property", NX_OBJ_LAST(setScale), "Resizes it.");
        b.method("Entity@ get_parent() const property", NX_OBJ_LAST(getParent), "What it moves with, or null.");
        b.method("void attach(Entity@+ parent)", NX_OBJ_LAST(setParent), "Makes it move with another, its position then being from there; null lets go.");
        b.method("int get_layer() const property", NX_OBJ_LAST(getLayer), "Higher layers are drawn over lower ones.");
        b.method("void set_layer(int) property", NX_OBJ_LAST(setLayer), "Moves it to another layer.");
        b.method("bool get_visible() const property", NX_OBJ_LAST(getVisible), "Whether it and its children are drawn.");
        b.method("void set_visible(bool) property", NX_OBJ_LAST(setVisible), "Shows or hides them.");

        b.method("void image(Texture@+ texture, float w = 0, float h = 0)", NX_OBJ_LAST(entityImage), "Looks like an image, centred; its own size unless one is given.");
        b.method("void rect(float w, float h, const Color &in color)", NX_OBJ_LAST(entityRect), "Looks like a rectangle.");
        b.method("void disc(float radius, const Color &in color)", NX_OBJ_LAST(entityDisc), "Looks like a disc.");
        b.method("void label(Font@+ font, const string &in text, const Color &in color = White)", NX_OBJ_LAST(entityText), "Looks like some text, centred.");
        b.method("string get_text() const property", NX_OBJ_LAST(entityGetText), "The text.");
        b.method("void set_text(const string &in) property", NX_OBJ_LAST(entitySetText), "Changes the text.");
        b.method("Color get_color() const property", NX_OBJ_LAST(getColor), "Multiplies how it looks.");
        b.method("void set_color(const Color &in) property", NX_OBJ_LAST(setColor), "Changes it.");
        b.method("float get_alpha() const property", NX_OBJ_LAST(getAlpha), "The colour's alpha.");
        b.method("void set_alpha(float) property", NX_OBJ_LAST(setAlpha), "Fades it.");
        b.method("Blend get_blend() const property", NX_OBJ_LAST(getBlend), "How it combines with what is behind.");
        b.method("void set_blend(Blend) property", NX_OBJ_LAST(setBlend), "How it combines with what is behind.");
        b.method("bool get_flip() const property", NX_OBJ_LAST(getFlip), "Whether the image is turned left to right.");
        b.method("void set_flip(bool) property", NX_OBJ_LAST(setFlip), "Turns it.");
        b.method("void sheet(uint columns, uint rows)", NX_OBJ_LAST(entitySheet), "Says that the image is a sheet of equal frames.");
        b.method("uint get_frame() const property", NX_OBJ_LAST(getFrame), "Which frame, along each row.");
        b.method("void set_frame(uint) property", NX_OBJ_LAST(setFrame), "Changes it.");
        b.method("void play(uint first, uint last, float fps, bool loop = true)", NX_OBJ_LAST(entityPlay), "Plays frames. Asking again for what is playing does nothing, so it can be called every update.");
        b.method("bool get_played() const property", NX_OBJ_LAST(entityPlayed), "Whether an animation that doesn't loop has finished.");

        b.method("Vec2 get_velocity() const property", NX_OBJ_LAST(getVelocity), "Units a second.");
        b.method("void set_velocity(const Vec2 &in) property", NX_OBJ_LAST(setVelocity), "Sets it going.");
        b.method("float get_spin() const property", NX_OBJ_LAST(getSpin), "Degrees a second.");
        b.method("void set_spin(float) property", NX_OBJ_LAST(setSpin), "Sets it turning.");
        b.method("float get_drag() const property", NX_OBJ_LAST(getDrag), "Part of its speed lost each second.");
        b.method("void set_drag(float) property", NX_OBJ_LAST(setDrag), "Part of its speed lost each second.");
        b.method("float get_gravity() const property", NX_OBJ_LAST(getGravity), "How much of the gravity it feels.");
        b.method("void set_gravity(float) property", NX_OBJ_LAST(setGravity), "How much of the scene's gravity it feels; 0 to begin with.");
        b.method("float get_bounce() const property", NX_OBJ_LAST(getBounce), "Part of its speed kept when it hits something.");
        b.method("void set_bounce(float) property", NX_OBJ_LAST(setBounce), "Part of its speed kept when it hits something.");
        b.method("bool get_solid() const property", NX_OBJ_LAST(getSolid), "Whether solid tiles and walls stop it.");
        b.method("void set_solid(bool) property", NX_OBJ_LAST(setSolid), "Makes solid tiles and walls stop it, without turning it: for characters.");
        b.method("float get_mass() const property", NX_OBJ_LAST(getMass), "0 unless it is a rigid body.");
        b.method("void set_mass(float) property", NX_OBJ_LAST(setMass), "Above 0, makes it a rigid body, which pushes, stacks and turns. It needs a collider and no parent.");
        b.method("float get_friction() const property", NX_OBJ_LAST(getFriction), "Of a rigid body.");
        b.method("void set_friction(float) property", NX_OBJ_LAST(setFriction), "Of a rigid body: 0 is ice, 1 rubber.");
        b.method("void push(const Vec2 &in impulse)", NX_OBJ_LAST(entityPush), "Pushes it at its middle.");
        b.method("void push(const Vec2 &in impulse, const Vec2 &in at)", NX_OBJ_LAST(entityPushAt), "Pushes it at a place in the scene, which may turn it.");
        b.method("bool get_onGround() const property", NX_OBJ_LAST(getOnGround), "Whether something solid is holding it up.");
        b.method("bool get_onCeiling() const property", NX_OBJ_LAST(getOnCeiling), "Whether it has just hit something above.");
        b.method("int get_onWall() const property", NX_OBJ_LAST(getOnWall), "-1 for something solid on its left, 1 on its right, otherwise 0.");

        b.method("void circle(float radius, uint group = 0, uint hits = 0)", NX_OBJ_LAST(entityCircle), "Gives it a round collider in a group from 0 to 15. hits has a bit for each group it wants to know about: 1 << group.");
        b.method("void box(float w, float h, uint group = 0, uint hits = 0)", NX_OBJ_LAST(entityBox), "The same with a box, which turns with it.");
        b.method("void polygon(const array<Vec2> &in corners, uint group = 0, uint hits = 0)", NX_OBJ_LAST(entityPolygon), "The same with a convex polygon, around its place, which turns and grows with it: up to 8 corners, made convex if they aren't. A rigid body of any shape: a wedge, a rock, a car's body.");
        b.method("void chain(const array<Vec2> &in points, bool loop = false, uint group = 0, uint hits = 0)", NX_OBJ_LAST(entityChain), "Lines through up to 256 points, around its place, that never move: ground with slopes and hills, a curved ramp, the inside of a bowl if loop. Bodies hit them from either side; characters stand on them and walk up their gentle parts, and are stopped by the steep ones.");
        b.method("bool get_oneWay() const property", NX_OBJ_LAST(getOneWay), "Whether a wall is solid only from above.");
        b.method("void set_oneWay(bool) property", NX_OBJ_LAST(setOneWay), "Makes a wall solid only from above, as a platform that what is under it can jump up through and land on: for characters and rigid bodies alike.");
        b.method("bool get_wall() const property", NX_OBJ_LAST(getWall), "Whether others stop at its collider.");
        b.method("bool get_occluder() const property", NX_OBJ_LAST(getOccluder), "Whether its collider casts shadows from lights on the plane.");
        b.method("void set_occluder(bool) property", NX_OBJ_LAST(setOccluder), "Makes its collider, a box or a circle, cast shadows from lights on the plane, as solid tiles do. It needs a collider first.");
        b.method("void set_wall(bool) property", NX_OBJ_LAST(setWall), "Makes solid and rigid bodies stop at its collider.");
        b.method("int get_group() const property", NX_OBJ_LAST(getGroup), "Its collider's group, or -1.");
        b.method("uint get_hits() const property", NX_OBJ_LAST(getHits), "The groups it wants to know about, a bit for each.");
        b.method("void set_hits(uint) property", NX_OBJ_LAST(setHits), "Changes the groups it wants to know about.");
        b.method("bool touching(Entity@+ other) const", NX_OBJ_LAST(entityTouching), "Whether their colliders overlap now.");
        b.method("float get_lifetime() const property", NX_OBJ_LAST(getLifetime), "Seconds until it is destroyed, or -1.");
        b.method("void set_lifetime(float) property", NX_OBJ_LAST(setLifetime), "Destroys it after some seconds.");
        b.method("void add(IBehaviour@+ behaviour)", NX_OBJ_LAST(entityAdd), "Adds a behaviour.");
        b.method("IBehaviour@ get_behaviour() const property", NX_OBJ_LAST(entityBehaviour), "Its first behaviour, or null. `cast<Enemy>(e.behaviour)` gives the class back.");

        b.section("Scene", "Entities that move, collide and are drawn by the engine. Each screen has its own scene, and it is stepped after the screen's update().");
        b.names("Scene");
        b.function("Entity@ spawn(const Vec2 &in position)", NX_FN(sceneSpawn), "A new, empty entity.");
        b.function("Entity@ spawn(const string &in prefab, const Vec2 &in position)", NX_FN(sceneSpawnPrefab), "A new entity from a prefab that has been loaded.");
        b.function("void load(const string &in name)", NX_FN(sceneLoad), "Reads a scene file: groups, particles, prefabs, maps and entities. See the guide.");
        b.function("void draw(Canvas@ canvas, int first = -32768, int last = 32767)", NX_FN(sceneDraw), "Draws the entities in some layers. Call it from draw(), after design().");
        b.function("void camera(const Vec2 &in position, float zoom = 1, float rotation = 0)", NX_FN(sceneCamera), "Puts a place in the middle of the screen. Without it, the scene's corner is the screen's.");
        b.function("Vec2 toWorld(const Vec2 &in screen)", NX_FN(sceneToWorld), "A place on the screen, in the scene.");
        b.function("Vec2 toScreen(const Vec2 &in world)", NX_FN(sceneToScreen), "A place in the scene, on the screen.");
        b.function("uint get_count() property", NX_FN(sceneCount), "How many entities there are.");
        b.function("uint countIn(uint group)", NX_FN(sceneCountGroup), "How many have a collider in a group.");
        b.function("int group(const string &in name)", NX_FN(sceneGroup), "The number of a group named in a scene file, or -1.");
        b.function("Entity@ find(const string &in name)", NX_FN(sceneFind), "The first of that name, or null.");
        b.function("Entity@ nearest(const Vec2 &in position, float radius, uint groups = 0xFFFF, Entity@+ except = null)", NX_FN(sceneNearest), "The nearest with a collider in some groups (a bit for each), or null.");
        b.function("array<Entity@>@ within(const Vec2 &in position, float radius, uint groups = 0xFFFF)", NX_FN(sceneWithin), "Everything whose collider touches a circle.");
        b.function("Entity@ raycast(const Vec2 &in from, const Vec2 &in to, Vec2 &out hit, uint groups = 0xFFFF)", NX_FN(sceneRaycast), "The first collider on a line, and where, or null.");
        b.function("Vec2 get_gravity() property", NX_FN(sceneGetGravity), "What pulls on those that feel it.");
        b.function("bool get_showShapes() property", NX_FN(sceneShowShapes), "Whether colliders are drawn over the scene.");
        b.function("void set_showShapes(bool) property", NX_FN(sceneSetShowShapes), "Draws every collider over the scene, to see what touches what: grey walls, green bodies with mass, blue characters and the rest, yellow areas that only feel.");
        b.function("void set_gravity(const Vec2 &in) property", NX_FN(sceneGravity), "Units a second, each second.");
        b.function("Color get_ambient() property", NX_FN(sceneAmbient), "The light everywhere, before lights add to it. White, as it starts, is no lighting at all; anything darker turns lighting on, so that entities' lights show and solid tiles and occluders cast shadows.");
        b.function("void set_ambient(const Color &in) property", NX_FN(sceneSetAmbient), "The light everywhere, before lights add to it: Color(0.1f, 0.1f, 0.2f) is a dark night.");
        b.function("int get_lightLayer() property", NX_FN(sceneLightLayer), "Layers above this one are drawn after the lighting, as they are: glows, sparks, and what is shown over the world. All of them are lit unless it is set.");
        b.function("void set_lightLayer(int) property", NX_FN(sceneSetLightLayer), "Layers above this one are drawn after the lighting, as they are.");
        b.function("void burst(const string &in particles, const Vec2 &in position, uint count)", NX_FN(sceneBurst), "Throws out some particles.");
        b.function("bool join(Entity@+ a, Entity@+ b, const Vec2 &in at, float softness = 0)", NX_FN(sceneJoin), "Pins two rigid bodies together at a place; null for b pins to the scene.");
        b.function("bool hinge(Entity@+ a, Entity@+ b, const Vec2 &in at, float low = 0, float high = 0, float motorSpeed = 0, float motorTorque = 0)", NX_FN(sceneHinge), "The same, with limits: a turns only between low and high degrees from how it is now, if low is less than high; and a motor that turns it at so many degrees a second, as strongly as its torque, if that isn't 0. A door, a flipper, a wheel that drives.");
        b.function("bool weld(Entity@+ a, Entity@+ b, const Vec2 &in at)", NX_FN(sceneWeld), "Holds two together as one, turned as they are now; null for b holds a where it is.");
        b.function("bool rope(Entity@+ a, Entity@+ b, const Vec2 &in atA, const Vec2 &in atB, float length = -1)", NX_FN(sceneRope), "A place on each no further apart than a length, as a rope: it pulls, never pushes. Below 0, as far apart as they are now.");
        b.function("bool spring(Entity@+ a, Entity@+ b, const Vec2 &in atA, const Vec2 &in atB, float frequency = 3, float damping = 0.5, float length = -1)", NX_FN(sceneSpring), "A place on each pulled towards a length and pushed away from it: frequency is how many times a second it would bounce, damping from 0, bouncing on, to 1, settling at once. The same whatever their weight.");
        b.function("bool slider(Entity@+ a, Entity@+ b, const Vec2 &in at, const Vec2 &in axis, float low = 0, float high = 0)", NX_FN(sceneSlider), "Lets a move only along a line through a place on b, which turns with b, without turning itself: a lift, a piston, a drawer. Between low and high along it, from where it is now, if low is less than high.");
        b.function("bool drag(Entity@+ a, const Vec2 &in at, float strength = 5)", NX_FN(sceneDrag), "Pulls a place on a body towards a place in the scene, softly, as a hand would: for picking things up with the mouse. No harder than strength times its weight. Move it on with dragTo; let go with unjoin.");
        b.function("void dragTo(Entity@+ a, const Vec2 &in place)", NX_FN(sceneDragTo), "Where a body's drag pulls towards now.");
        b.function("void motor(Entity@+ a, float speed, float torque)", NX_FN(sceneMotor), "Changes the motor of a's hinges and wheels: degrees a second, and how strongly; 0 torque for none.");
        b.function("bool wheel(Entity@+ wheel, Entity@+ body, const Vec2 &in axis = Vec2(0, 1), float frequency = 4, float damping = 0.7, float motorSpeed = 0, float motorTorque = 0)", NX_FN(sceneWheel), "A wheel on a car: it turns freely about its middle, and moves only along a line through where it is now, which turns with the body, sprung towards where it is: frequency is how many times a second it would bounce, damping how soon it settles. With a motor, as a hinge has, that motor() changes. Null for body puts it on the scene.");
        b.function("bool pulley(Entity@+ a, Entity@+ b, const Vec2 &in groundA, const Vec2 &in groundB, const Vec2 &in atA, const Vec2 &in atB, float ratio = 1)", NX_FN(scenePulley), "A rope from a place on a up over a fixed place, across to another and down to a place on b: as one goes down the other goes up, b ratio times as far as a. A lift and its weight, a block and tackle.");
        b.function("bool gear(Entity@+ a, Entity@+ b, float ratio = 1)", NX_FN(sceneGear), "As a turns one way, b turns the other, ratio times as far: two gears that mesh, each on its hinge. Below 0, the same way, as wheels joined by a chain.");
        b.function("bool friction(Entity@+ a, Entity@+ b = null, float strength = 1)", NX_FN(sceneFriction), "Slows a's moving and turning against b's, or the scene's, no harder than strength times its weight: the ground under what is seen from above, with no gravity, such as pucks, cars and crates.");
        b.function("bool follow(Entity@+ a, Entity@+ b, float strength = 5, float correction = 0.3)", NX_FN(sceneFollow), "Pushes b, no harder than strength times its weight, towards where it is now from a, as a moves and turns: something carried, a pet that keeps up. correction (0 to 1) is how much of the way it is put right each step.");
        b.function("void followTarget(Entity@+ a, Entity@+ b, const Vec2 &in offset, float degrees = 0)", NX_FN(sceneFollowTarget), "Where from a that b is pushed towards now, in a's own directions, and how far turned from it.");
        b.function("void unjoin(Entity@+ a)", NX_FN(sceneUnjoin), "Every joint that a is in.");
        b.function("bool contact(Entity@+ a, Entity@+ b, Vec2 &out at, Vec2 &out normal, float &out impulse)", NX_FN(sceneContact), "Whether two rigid bodies, or a body and a wall, touched in the last step: where, which way from a to b, and how hard they pressed. For a sound as loud as a knock, or sparks where it was."); 
        b.function("bool path(const Vec2 &in from, const Vec2 &in to, array<Vec2>@+ way, bool diagonal = true)", NX_FN(scenePath), "The way from one place to another round the solid tiles of the maps, on the grid of the last: its corners, made as straight as it can be, ending at `to`. Diagonally too unless told not to, but never across a corner. False if there is none.");
        b.function("bool blocked(const Vec2 &in place)", NX_FN(sceneBlocked), "Whether a solid tile is there, in any map.");
        b.function("uint tile(const Vec2 &in position)", NX_FN(sceneTile), "What the map has there: 0 is nothing.");
        b.function("void setTile(const Vec2 &in position, uint tile)", NX_FN(sceneSetTile), "Changes the map.");
        b.function("void terrain(uint tile, int firstFrame = 0)", NX_FN(sceneTerrain), "Makes a tile of the last map a terrain, which joins up with itself: of 16 frames in its sheet from firstFrame on, the one drawn is chosen by which sides meet the same tile, up 1, right 2, down 4 and left 8; drawn in colours, its open sides are edged and its outer corners rounded. -1 makes it plain again. A map's \"terrains\" does the same: { \"2\": 1 }.");
        b.function("Vec2 get_mapSize() property", NX_FN(sceneMapSize), "How large the map is.");
        b.function("uint get_particles() property", NX_FN(sceneParticles), "How many particles there are, on the plane and in space. There may be 8000, or 1500 on the original Xbox; more are left out.");
        b.function("void clearParticles()", NX_FN(sceneClearParticles), "Does away with all there are. Emitters go on.");
        b.function("uint get_drawn() property", NX_FN(sceneDrawn), "How many entities and tiles were drawn last.");
        b.function("uint get_pairs() property", NX_FN(scenePairs), "Pairs of colliders compared in the last step.");
        b.function("uint get_contacts() property", NX_FN(sceneContacts), "Where rigid bodies touched in the last step.");
        b.names("");
    }

    const char* behaviourPrelude()
    {
        return kPrelude;
    }
}
