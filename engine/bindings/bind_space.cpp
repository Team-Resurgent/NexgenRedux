#include "base/json_value.h"
#include "bindings/script_refs.h"

#include <scriptarray/scriptarray.h>

#include <math.h>

namespace nx
{
    namespace
    {
        constexpr float kDegrees = 3.14159265f / 180.0f;

        Space& space()
        {
            return services().world->space();
        }

        Spatial* spatialOf(const EntityRef* e) { return e->world ? e->world->space().spatial(e->id) : nullptr; }
        const Spatial* someSpatial(const EntityRef* e) { return e->world ? e->world->space().getSpatial(e->id) : nullptr; }
        Appearance* lookOf(const EntityRef* e) { return e->world ? e->world->space().appearance(e->id) : nullptr; }
        Body3* bodyOf(const EntityRef* e) { return e->world ? e->world->space().body(e->id) : nullptr; }
        const Body3* someBody(const EntityRef* e) { return e->world ? e->world->space().getBody(e->id) : nullptr; }

        // --- Meshes ---

        CustomMesh* makeMesh()
        {
            CustomMesh* m = create<CustomMesh>(MemTag::Script);
            if (m)
            {
                m->device = services().device;
            }
            return m;
        }
        void meshFull() { scriptError("a mesh has up to 65536 vertices"); }
        bool room(CustomMesh* m, u32 vertices)
        {
            m->changed = true;
            if (m->data.vertices.count() + vertices > 65536)
            {
                meshFull();
                return false;
            }
            return true;
        }
        u32 roundness(u32 around) { return around < 3 ? 3 : around > 64 ? 64 : around; }
        u32 meshVertices(const CustomMesh* m) { return m->data.vertices.count(); }
        u32 meshTriangles(const CustomMesh* m) { return m->data.indices.count() / 3; }
        u32 meshAdd(const Vec3& place, const Vec3& normal, const Color& color, float u, float v, CustomMesh* m)
        {
            if (room(m, 1))
            {
                m->data.vertex(place, normal, color, u, v);
            }
            return m->data.vertices.count() - 1;
        }
        void meshTriangle(u32 a, u32 b, u32 c, CustomMesh* m)
        {
            const u32 count = m->data.vertices.count();
            if (a >= count || b >= count || c >= count)
            {
                scriptError("there is no such vertex");
                return;
            }
            m->changed = true;
            m->data.triangle(a, b, c);
        }
        void meshQuad(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d, const Color& color, CustomMesh* m) { if (room(m, 4)) m->data.quad(a, b, c, d, color); }
        void meshBox(const Vec3& middle, const Vec3& size, const Color& color, CustomMesh* m) { if (room(m, 24)) m->data.box(middle, size * 0.5f, color); }
        void meshSphere(const Vec3& middle, float radius, const Color& color, u32 around, CustomMesh* m) { if (room(m, 2300)) m->data.sphere(middle, radius, roundness(around), color); }
        void meshCylinder(const Vec3& bottom, float height, float low, float high, const Color& color, u32 around, CustomMesh* m) { if (room(m, 300)) m->data.cylinder(bottom, height, low, high, roundness(around), color); }
        void meshTorus(float ring, float tube, const Color& color, u32 around, CustomMesh* m) { if (room(m, 4300)) m->data.torus(ring, tube, roundness(around), color); }
        void meshPlane(float width, float depth, u32 squares, const Color& color, CustomMesh* m)
        {
            squares = squares < 1 ? 1 : squares > 200 ? 200 : squares;
            if (room(m, (squares + 1) * (squares + 1)))
            {
                m->data.plane(width * 0.5f, depth * 0.5f, squares, color);
            }
        }
        void meshMove(u32 first, const Vec3& place, const Vec3& angles, const Vec3& size, CustomMesh* m)
        {
            m->changed = true;
            m->data.transform(first, compose(place, Quat::fromAngles(angles), size));
        }
        void meshSmooth(CustomMesh* m)
        {
            m->changed = true;
            m->data.computeNormals();
        }
        void meshClear(CustomMesh* m)
        {
            m->changed = true;
            m->data.clear();
        }

        float modelRadius(const ModelRef* m) { return m->asset->model ? m->asset->model->radius : 0; }
        Vec3 modelLow(const ModelRef* m) { return m->asset->model ? m->asset->model->low : Vec3(); }
        Vec3 modelHigh(const ModelRef* m) { return m->asset->model ? m->asset->model->high : Vec3(); }
        u32 modelTriangles(const ModelRef* m)
        {
            u32 count = 0;
            if (m->asset->model)
            {
                for (const ModelPart& p : m->asset->model->parts)
                {
                    count += p.count[0] / 3;
                }
            }
            return count;
        }
        u32 modelAnimations(const ModelRef* m) { return m->asset->model ? m->asset->model->animations.count() : 0; }
        std::string modelAnimation(u32 index, const ModelRef* m) { return index < modelAnimations(m) ? m->asset->model->animations[index].name : ""; }

        // --- Entities ---

        Vec3 getPlace(const EntityRef* e) { const Spatial* s = someSpatial(e); return s ? s->place : Vec3(); }
        void setPlace(const Vec3& v, EntityRef* e) { if (Spatial* s = spatialOf(e)) s->place = v; }
        Vec3 getWorldPlace(const EntityRef* e) { return e->world ? e->world->space().worldPlace(e->id) : Vec3(); }
        Vec3 entityOutward(const Vec3& place, const EntityRef* e)
        {
            const Spatial* s = someSpatial(e);
            return s ? s->world.transformPoint(place) : place;
        }
        Vec3 getAngles(const EntityRef* e) { const Spatial* s = someSpatial(e); return s ? s->turn.angles() : Vec3(); }
        void setAngles(const Vec3& v, EntityRef* e) { if (Spatial* s = spatialOf(e)) s->turn = Quat::fromAngles(v); }
        Vec3 getSize(const EntityRef* e) { const Spatial* s = someSpatial(e); return s ? s->size : Vec3(1, 1, 1); }
        void setSize(const Vec3& v, EntityRef* e) { if (Spatial* s = spatialOf(e)) s->size = v; }
        Vec3 getForward(const EntityRef* e) { const Spatial* s = someSpatial(e); return s ? s->turn.rotate({ 0, 0, 1 }) : Vec3(0, 0, 1); }
        void entityFace(const Vec3& way, float part, EntityRef* e)
        {
            Spatial* s = spatialOf(e);
            if (s && (way.x != 0 || way.z != 0))
            {
                const Quat to = Quat::axisAngle({ 0, 1, 0 }, atan2f(way.x, way.z));
                s->turn = part >= 1 ? to : Quat::slerp(s->turn, to, part < 0 ? 0 : part);
            }
        }
        void entityTurn(const Vec3& about, float degrees, EntityRef* e)
        {
            Spatial* s = spatialOf(e);
            const float length = about.length();
            if (s && length > 0)
            {
                s->turn = (Quat::axisAngle(about / length, degrees * kDegrees) * s->turn).normalized();
            }
        }
        Vec3 getMotion(const EntityRef* e) { const Body3* b = someBody(e); return b ? b->velocity : Vec3(); }
        void setMotion(const Vec3& v, EntityRef* e)
        {
            if (Body3* b = bodyOf(e))
            {
                b->velocity = v;
                b->still = 0;
            }
        }
        Vec3 getTurning(const EntityRef* e) { const Body3* b = someBody(e); return b ? b->spin / kDegrees : Vec3(); }
        void setTurning(const Vec3& v, EntityRef* e)
        {
            if (Body3* b = bodyOf(e))
            {
                b->spin = v * kDegrees;
                b->still = 0;
            }
        }
        bool getWalker(const EntityRef* e) { const Body3* b = someBody(e); return b && b->walker; }
        void setWalker(bool v, EntityRef* e)
        {
            if (Body3* b = bodyOf(e))
            {
                b->walker = v;
                b->gravity = v && b->gravity == 0 ? 1 : b->gravity;
            }
        }
        Vec3 getGroundNormal(const EntityRef* e) { const Body3* b = someBody(e); return b ? b->ground : Vec3(0, 1, 0); }
        void entityPush(const Vec3& impulse, EntityRef* e) { if (e->world) e->world->space().push(e->id, impulse, nullptr); }
        void entityPushAt(const Vec3& impulse, const Vec3& at, EntityRef* e) { if (e->world) e->world->space().push(e->id, impulse, &at); }

        void entityModel(const ModelRef* model, EntityRef* e)
        {
            if (model == nullptr)
            {
                scriptError("null model");
            }
            else if (e->world)
            {
                e->world->space().setModel(e->id, model->asset);
            }
        }
        void entityMesh(CustomMesh* mesh, const Color& color, EntityRef* e)
        {
            if (mesh == nullptr)
            {
                scriptError("null mesh");
            }
            else if (e->world)
            {
                e->world->space().setCustom(e->id, mesh);
                if (Appearance* a = lookOf(e))
                {
                    a->color = color;
                }
            }
        }
        void entitySolid(Solid solid, const Color& color, EntityRef* e)
        {
            if (solid >= Solid::Count)
            {
                scriptError("no such solid");
            }
            else if (Appearance* a = lookOf(e))
            {
                a->solid = solid;
                a->color = color;
            }
        }
        void entityGlow(float size, const Color& color, EntityRef* e)
        {
            if (Appearance* a = lookOf(e))
            {
                a->glowBall = true;
                a->casts = false;
                a->blend = gpu::Blend::Additive;
                a->color = color;
                setSize({ size, size, size }, e);
            }
        }
        void entitySkin(const TextureRef* texture, bool repeat, EntityRef* e)
        {
            if (e->world)
            {
                e->world->space().setTexture(e->id, texture ? texture->asset : nullptr);
                if (Appearance* a = lookOf(e))
                {
                    a->repeat = repeat;
                }
            }
        }
        const Appearance* someLook(const EntityRef* e) { return e->world ? e->world->space().getAppearance(e->id) : nullptr; }
        float getShine(const EntityRef* e) { const Appearance* a = someLook(e); return a ? a->shine : 0; }
        float getGlowing(const EntityRef* e) { const Appearance* a = someLook(e); return a ? a->glow : 0; }
        bool getTwoSided(const EntityRef* e) { const Appearance* a = someLook(e); return a && a->twoSided; }
        bool getShadow(const EntityRef* e) { const Appearance* a = someLook(e); return a && a->casts; }
        float getAnimationSpeed(const EntityRef* e) { const Appearance* a = someLook(e); return a ? a->speed : 1; }
        bool getStreamed(const EntityRef* e) { const Spatial* s = someSpatial(e); return s && s->streamed; }
        Vec3 getColliderAt(const EntityRef* e) { const Collider3* c = e->world ? e->world->space().getCollider(e->id) : nullptr; return c ? c->middle : Vec3(); }
        void setShine(float v, EntityRef* e) { if (Appearance* a = lookOf(e)) a->shine = v; }
        float getSway(const EntityRef* e) { const Appearance* a = someLook(e); return a ? a->sway : 0; }
        bool getImpostor(const EntityRef* e) { const Appearance* a = someLook(e); return a && a->impostor; }
        void setImpostor(bool v, EntityRef* e) { if (Appearance* a = lookOf(e)) a->impostor = v; }
        void entitySprite(const TextureRef* sheet, u32 angles, u32 frames, EntityRef* e)
        {
            if (e->world)
            {
                e->world->space().setSprite(e->id, sheet ? sheet->asset : nullptr, angles, frames);
            }
        }
        u32 getSpriteFrame(const EntityRef* e) { const Appearance* a = someLook(e); return a ? a->spriteFrame : 0; }
        void setSpriteFrame(u32 v, EntityRef* e)
        {
            if (e->world)
            {
                e->world->space().setSpriteFrame(e->id, v);
            }
        }
        void setSway(float v, EntityRef* e) { if (Appearance* a = lookOf(e)) a->sway = v; }
        void setGlowing(float v, EntityRef* e) { if (Appearance* a = lookOf(e)) a->glow = v; }
        void setTwoSided(bool v, EntityRef* e) { if (Appearance* a = lookOf(e)) a->twoSided = v; }
        void setShadow(bool v, EntityRef* e) { if (Appearance* a = lookOf(e)) a->casts = v; }
        u32 getDetail(const EntityRef* e) { const Appearance* a = e->world ? e->world->space().getAppearance(e->id) : nullptr; return a ? a->detail : 0; }
        void entityEffect(EffectUse* effect, EntityRef* e) { if (e->world) e->world->space().setEffect(e->id, effect); }
        void entityLight(const Color& color, float range, EntityRef* e)
        {
            if (Light3* l = e->world ? e->world->space().light(e->id) : nullptr)
            {
                l->color = color;
                l->range = range > 0.01f ? range : 0.01f;
            }
        }
        void entityEmit(const std::string& style, float rate, EntityRef* e)
        {
            if (e->world && e->world->alive(e->id) && !e->world->space().emit(e->id, style.c_str(), rate))
            {
                scriptError("no particles called " + style);
            }
        }
        void spaceBurst(const std::string& style, const Vec3& place, u32 count, const Vec3& aim)
        {
            if (!space().burst(style.c_str(), place, count, aim))
            {
                scriptError("no particles called " + style);
            }
        }
        bool entityAnimate(const std::string& name, bool loop, float change, EntityRef* e) { return e->world && e->world->space().play(e->id, name.c_str(), loop, change); }
        bool getAnimated(const EntityRef* e) { return e->world == nullptr || e->world->space().played(e->id); }
        void setAnimationSpeed(float v, EntityRef* e) { if (Appearance* a = lookOf(e)) a->speed = v; }

        void collide(EntityRef* e, Shape3 shape, const Vec3& half, u32 group, u32 hits)
        {
            Collider3* c = e->world ? e->world->space().collider(e->id) : nullptr;
            if (group >= kGroups)
            {
                scriptError("groups are 0 to 15");
            }
            else if (c)
            {
                c->shape = shape;
                c->half = { half.x > 0.001f ? half.x : 0.001f, half.y > 0.001f ? half.y : 0.001f, half.z > 0.001f ? half.z : 0.001f };
                c->group = static_cast<u8>(group);
                c->hits = static_cast<u16>(hits);
            }
        }
        void entitySphere(float radius, u32 group, u32 hits, EntityRef* e) { collide(e, Shape3::Sphere, { radius, radius, radius }, group, hits); }
        void entityCube(const Vec3& size, u32 group, u32 hits, EntityRef* e) { collide(e, Shape3::Box, size * 0.5f, group, hits); }
        void entityCapsule(float radius, float height, u32 group, u32 hits, EntityRef* e) { collide(e, Shape3::Capsule, { radius, height * 0.5f, radius }, group, hits); }
        void entityExact(u32 group, u32 hits, EntityRef* e) { collide(e, Shape3::Shell, { 0.01f, 0.01f, 0.01f }, group, hits); }
        void setColliderAt(const Vec3& v, EntityRef* e) { if (Collider3* c = e->world ? e->world->space().getCollider(e->id) : nullptr) c->middle = v; }

        // --- Space:: ---

        EntityRef* spaceSpawn(const Vec3& at)
        {
            World& w = *services().world;
            const u32 id = w.spawn({});
            if (id == 0)
            {
                scriptError("too many entities");
            }
            else if (Spatial* s = w.space().spatial(id))
            {
                s->place = at;
                w.space().transform(id & 0xFFFF);
            }
            return w.ref(id);
        }
        void spaceDraw(Canvas* c) { if (c) space().draw(*c, 0, 0, 0, 0); }
        void spaceDrawIn(Canvas* c, float x, float y, float w, float h) { if (c && w > 0 && h > 0) space().draw(*c, x, y, w, h); }
        void spaceCamera(const Vec3& from, const Vec3& to, float angle) { space().setCamera(from, to, angle); }
        void spaceSun(const Vec3& direction, const Color& color) { space().setSun(direction, color); }
        void spaceAmbient(const Color& sky, const Color& ground) { space().setAmbient(sky, ground); }
        void spaceSky(const Color& top, const Color& horizon) { space().setSky(top, horizon); }
        void spaceFog(const Color& color, float start, float end) { space().setFog(color, start, end); }
        Vec3 spaceWind() { return space().wind(); }
        void spaceGrass(const ModelRef* tuft, float perSquareMetre, float distance) { space().setGrass(tuft ? tuft->asset : nullptr, perSquareMetre, distance); }
        float spaceImpostors() { return space().impostors(); }
        bool spaceNavigate(const Vec3& middle, float size, float cell, float climb, u32 groups) { return space().navigate(middle, size, cell, climb, groups); }
        bool spacePath(const Vec3& from, const Vec3& to, CScriptArray* out)
        {
            GrowArray<Vec3> way;
            const bool reached = space().findPath(from, to, way);
            if (out)
            {
                out->Resize(way.count());
                for (u32 i = 0; i < way.count(); i++)
                {
                    *static_cast<Vec3*>(out->At(i)) = way[i];
                }
            }
            return reached;
        }
        bool spaceWalkable(const Vec3& at) { return space().walkable(at); }
        Vec3 spaceNearestWalkable(const Vec3& at)
        {
            Vec3 out = at;
            space().nearestWalkable(at, out);
            return out;
        }
        void spaceSetImpostors(float v) { space().setImpostors(v); }
        void spaceSetWind(const Vec3& v) { space().setWind(v); }
        bool spaceLines() { return space().lines(); }
        void spaceSetLines(bool v) { space().setLines(v); }
        void spaceShadows(float strength, float distance) { space().setShadows(strength, distance); }
        void spaceFarthest(float distance) { space().setFar(distance); }
        Vec3 spaceGetGravity() { return space().gravity(); }
        void spaceGravity(const Vec3& g) { space().setGravity(g); }
        void spaceGround(float height) { space().setGround(height); }
        void spaceWater(float height, const Color& color) { space().setWater(height, color); }
        void spaceLand(u32 seed, u32 squares, float spacing, float height, float roughness, bool island)
        {
            if (!space().makeLand(seed, squares, spacing, height, roughness, island))
            {
                scriptError("no room for the land");
            }
        }
        void spaceEndless(u32 seed, float spacing, float height, float roughness, u32 reach)
        {
            if (!space().makeEndlessLand(seed, spacing, height, roughness, reach))
            {
                scriptError("no room for the land");
            }
        }
        bool spaceNextRegion(int& x, int& z, bool& arrived) { return space().nextRegion(x, z, arrived); }
        float spaceRegionSize() { return space().regionSize(); }
        u32 spaceRegions() { return space().regions(); }
        u32 spaceRegionsWaiting() { return space().regionsWaiting(); }
        int spaceLoad(const std::string& name, const Vec3& at, bool streamed)
        {
            Services& s = services();
            char path[192];
            if (!resolveAsset(s, name.c_str(), path, sizeof(path)))
            {
                scriptError("asset names start with asset: or global: (" + name + ")");
                return 0;
            }
            FileData file;
            if (!readOwn(s, name.c_str(), file))
            {
                scriptError(std::string(path) + " cannot be read");
                return 0;
            }
            const int count = space().load(path, file, s.folder, at, streamed);
            if (count < 0)
            {
                scriptError(s.world->takeError());
            }
            return count;
        }
        EntityRef* spaceMake(JsonValue* json, const Vec3& at)
        {
            Services& s = services();
            if (json == nullptr)
            {
                scriptError("null json");
                return nullptr;
            }
            // As text, so that there is one thing that reads them.
            const std::string text = json->write(true);
            JsonTree tree;
            const u32 id = tree.parse(text.data(), text.size()) ? space().make(tree.root(), s.folder, at) : 0;
            if (id == 0)
            {
                scriptError(s.world->takeError());
                return nullptr;
            }
            return s.world->ref(id);
        }
        void setStreamed(bool v, EntityRef* e) { if (e->world) e->world->space().setStreamed(e->id, v); }
        void spaceLandColors(const Color& low, const Color& middle, const Color& high, const Color& steep, float lowHeight, float highHeight) { space().setLandColors(low, middle, high, steep, lowHeight, highHeight); }
        void spaceFlatten(float x, float z, float radius) { space().flatten(x, z, radius, space().heightAt(x, z)); }
        void spaceFlattenTo(float x, float z, float radius, float height) { space().flatten(x, z, radius, height); }
        float spaceHeight(float x, float z) { return space().heightAt(x, z); }
        Vec3 spaceNormal(float x, float z) { return space().normalAt(x, z); }
        void handWork(bool done)
        {
            if (!done && (!space().hasLand() || space().streaming()))
            {
                scriptError("there is no land there to change, or no more room for what was done to it");
            }
        }
        void spaceRaise(float x, float z, float radius, float by) { handWork(space().raise(x, z, radius, by)); }
        void spaceSmooth(float x, float z, float radius, float amount) { handWork(space().smooth(x, z, radius, amount)); }
        void spaceLevel(float x, float z, float radius, float height, float amount) { handWork(space().level(x, z, radius, height, amount)); }
        void spacePaint(float x, float z, float radius, const Color& color, float amount) { handWork(space().paint(x, z, radius, color, amount)); }
        void spaceHole(float x, float z, float radius, bool open) { handWork(space().hole(x, z, radius, open)); }
        bool spaceEdges() { return space().edges(); }
        void spaceSetEdges(bool v) { space().setEdges(v); }
        void spaceSkyPicture(const TextureRef* t) { space().setSkyPicture(t ? t->asset : nullptr); }
        std::string spaceLandEdits() { return space().landEdits(); }
        bool spaceRestoreLand(const std::string& text) { return space().setLandEdits(text); }
        float spaceLandSize() { return space().landSize(); }

        void spaceShape(gpu::FieldKind kind, const Vec3& place, const Vec3& size, const Color& color, float soft, gpu::FieldOperation operation, float turn)
        {
            gpu::Field& f = space().field();
            if (f.count < gpu::kFieldShapes && kind <= gpu::FieldKind::Cylinder && operation <= gpu::FieldOperation::Keep)
            {
                gpu::FieldShape& s = f.shapes[f.count++];
                s.kind = kind;
                s.operation = operation;
                s.place = place;
                s.size = size;
                s.soft = soft;
                s.turn = turn * kDegrees;
                s.color = color;
            }
        }
        void spaceClearShapes() { space().field().count = 0; }
        void spaceShapeLook(float shine, float reflection, float repeat, float farthest, u32 steps)
        {
            gpu::Field& f = space().field();
            f.shine = shine;
            f.reflection = reflection;
            f.repeat = repeat;
            f.farthest = farthest;
            f.steps = steps < 8 ? 8 : steps > 160 ? 160 : steps;
        }

        bool spaceRaycast(const Vec3& from, const Vec3& to, Vec3& hit, Vec3& normal, u32 groups, const EntityRef* except)
        {
            u32 id = 0;
            return space().raycast(from, to, groups, except ? except->id : 0, id, hit, normal);
        }
        EntityRef* spacePick(const Vec3& from, const Vec3& to, Vec3& hit, u32 groups, const EntityRef* except)
        {
            u32 id = 0;
            Vec3 normal;
            space().raycast(from, to, groups, except ? except->id : 0, id, hit, normal);
            return services().world->ref(id);
        }
        CScriptArray* spaceWithin(const Vec3& at, float radius, u32 groups)
        {
            CScriptArray* list = CScriptArray::Create(services().entityArray);
            World& w = *services().world;
            w.space().within(at, radius, groups, [&](u32 id)
            {
                EntityRef* e = w.ref(id);
                list->InsertLast(&e);  // adds its own
                e->release();
            });
            return list;
        }
        bool spaceToScreen(const Vec3& place, Vec2& screen) { return space().toScreen(place, screen); }
        Vec3 spaceRay(const Vec2& screen) { return space().rayThrough(screen); }
        Vec3 spaceEye() { return space().cameraPlace(); }
        u32 spaceDrawn() { return space().stats().drawn; }
        u32 spaceHidden() { return space().stats().hidden; }
        u32 spaceDrawnAt(u32 level) { return level < 3 ? space().stats().details[level] : 0; }
        u32 spaceContacts() { return space().stats().contacts; }
        u32 spaceTriangles() { return services().device->stats().triangles; }
        Vec3 lerpVec3(const Vec3& a, const Vec3& b, float t) { return a + (b - a) * t; }
    }

    void bindSpaceTypes(Binder& b)
    {
        b.refType("Model", 0, "A 3D model. Loaded once and shared by everyone who asks for it.");
        b.behaviour(asBEHAVE_ADDREF, "void f()", NX_METHOD(ModelRef, addRef));
        b.behaviour(asBEHAVE_RELEASE, "void f()", NX_METHOD(ModelRef, release));
        b.method("bool get_failed() const property", NX_METHOD(ModelRef, failed), "Whether the file could not be loaded.");
        b.method("bool get_ready() const property", NX_METHOD(ModelRef, ready), "Whether it has arrived. Until then it draws as nothing.");
        b.method("float get_radius() const property", NX_OBJ_LAST(modelRadius), "Of a ball around it.");
        b.method("Vec3 get_low() const property", NX_OBJ_LAST(modelLow), "The least corner of a box around it.");
        b.method("Vec3 get_high() const property", NX_OBJ_LAST(modelHigh), "The greatest.");
        b.method("uint get_triangles() const property", NX_OBJ_LAST(modelTriangles), "How many it has, close up.");
        b.method("uint get_animations() const property", NX_OBJ_LAST(modelAnimations), "How many it has.");
        b.method("string animation(uint index) const", NX_OBJ_LAST(modelAnimation), "The name of one.");

        b.enumType("Solid", "Shapes that are always there, 1 metre across.");
        b.enumValue("SolidNone", static_cast<int>(Solid::None), "Nothing.");
        b.enumValue("SolidBox", static_cast<int>(Solid::Box), "A cube.");
        b.enumValue("SolidSphere", static_cast<int>(Solid::Sphere), "A ball.");
        b.enumValue("SolidCylinder", static_cast<int>(Solid::Cylinder), "Upright.");
        b.enumValue("SolidCone", static_cast<int>(Solid::Cone), "Point up.");
        b.enumValue("SolidTorus", static_cast<int>(Solid::Torus), "A ring, lying flat.");
        b.enumValue("SolidCapsule", static_cast<int>(Solid::Capsule), "A round-ended cylinder, 2 metres tall.");
        b.enumValue("SolidPlane", static_cast<int>(Solid::Plane), "Flat, facing up.");

        b.refType("Effect", 0, "A shader of your own, over the whole picture or on a thing in space. See the guide.");
        b.behaviour(asBEHAVE_ADDREF, "void f()", NX_METHOD(EffectUse, addRef));
        b.behaviour(asBEHAVE_RELEASE, "void f()", NX_METHOD(EffectUse, release));
        b.method("bool get_failed() const property", NX_METHOD(EffectUse, failed), "Whether it could not be loaded, or has a mistake in it, which is in the log.");
        b.method("bool get_ready() const property", NX_METHOD(EffectUse, ready), "Whether it has arrived.");
        b.method("bool get_works() const property", NX_METHOD(EffectUse, works), "Whether this GPU can do it. The original Xbox can't.");
        b.method("void set(uint index, float value)", NX_METHOD(EffectUse, set), "Changes one of its sixteen values: 0 to 3 are values[0].x to values[0].w, and so on.");

        b.refType("Mesh", 0, "Triangles made by a script. Shared by every entity that shows it.");
        b.behaviour(asBEHAVE_FACTORY, "Mesh@ f()", NX_FN(makeMesh));
        b.behaviour(asBEHAVE_ADDREF, "void f()", NX_METHOD(CustomMesh, addRef));
        b.behaviour(asBEHAVE_RELEASE, "void f()", NX_METHOD(CustomMesh, release));
        b.method("uint get_vertices() const property", NX_OBJ_LAST(meshVertices), "How many there are so far.");
        b.method("uint get_triangles() const property", NX_OBJ_LAST(meshTriangles), "How many there are so far.");
        b.method("uint vertex(const Vec3 &in place, const Vec3 &in normal, const Color &in color = White, float u = 0, float v = 0)", NX_OBJ_LAST(meshAdd), "Adds one and returns its number.");
        b.method("void triangle(uint a, uint b, uint c)", NX_OBJ_LAST(meshTriangle), "Joins three vertices, counter-clockwise seen from the front.");
        b.method("void quad(const Vec3 &in a, const Vec3 &in b, const Vec3 &in c, const Vec3 &in d, const Color &in color = White)", NX_OBJ_LAST(meshQuad), "Four corners, counter-clockwise seen from the front.");
        b.method("void box(const Vec3 &in middle, const Vec3 &in size, const Color &in color = White)", NX_OBJ_LAST(meshBox), "Adds a box.");
        b.method("void sphere(const Vec3 &in middle, float radius, const Color &in color = White, uint around = 16)", NX_OBJ_LAST(meshSphere), "Adds a ball.");
        b.method("void cylinder(const Vec3 &in bottom, float height, float bottomRadius, float topRadius, const Color &in color = White, uint around = 12)", NX_OBJ_LAST(meshCylinder), "Adds a cylinder; a radius of 0 makes a cone.");
        b.method("void torus(float ring, float tube, const Color &in color = White, uint around = 20)", NX_OBJ_LAST(meshTorus), "Adds a ring, lying flat.");
        b.method("void plane(float width, float depth, uint squares = 1, const Color &in color = White)", NX_OBJ_LAST(meshPlane), "Adds a flat sheet facing up.");
        b.method("void move(uint firstVertex, const Vec3 &in place, const Vec3 &in angles = Vec3(), const Vec3 &in size = Vec3(1, 1, 1))", NX_OBJ_LAST(meshMove), "Moves everything from a vertex on: resized, then turned, then moved.");
        b.method("void smooth()", NX_OBJ_LAST(meshSmooth), "Works out the normals from the triangles.");
        b.method("void clear()", NX_OBJ_LAST(meshClear), "Takes everything out.");
    }

    void bindSpace(Binder& b)
    {
        b.reopen("Entity");
        b.method("Vec3 get_place() const property", NX_OBJ_LAST(getPlace), "In space: where it is, from its parent if it has one.");
        b.method("void set_place(const Vec3 &in) property", NX_OBJ_LAST(setPlace), "Moves it.");
        b.method("Vec3 get_worldPlace() const property", NX_OBJ_LAST(getWorldPlace), "Where it is in space, whatever its parents.");
        b.method("Vec3 toSpace(const Vec3 &in place) const", NX_OBJ_LAST(entityOutward), "Where a place of its own is in space, as it is turned and sized: its hand, or the end of its gun.");
        b.method("Vec3 get_angles() const property", NX_OBJ_LAST(getAngles), "Degrees it is tipped, turned and rolled: about x, y and z.");
        b.method("void set_angles(const Vec3 &in) property", NX_OBJ_LAST(setAngles), "Turns it.");
        b.method("Vec3 get_size() const property", NX_OBJ_LAST(getSize), "Its size along each axis; 1 is normal.");
        b.method("void set_size(const Vec3 &in) property", NX_OBJ_LAST(setSize), "Resizes it.");
        b.method("Vec3 get_forward() const property", NX_OBJ_LAST(getForward), "Which way it faces: z, turned as it is.");
        b.method("void face(const Vec3 &in direction, float part = 1)", NX_OBJ_LAST(entityFace), "Turns it, staying upright, some or all of the way to face along a direction.");
        b.method("void turn(const Vec3 &in about, float degrees)", NX_OBJ_LAST(entityTurn), "Turns it about an axis.");
        b.method("void model(Model@+ model)", NX_OBJ_LAST(entityModel), "Looks like a model.");
        b.method("void mesh(Mesh@+ mesh, const Color &in color = White)", NX_OBJ_LAST(entityMesh), "Looks like a mesh made by a script.");
        b.method("void shape(Solid solid, const Color &in color = White)", NX_OBJ_LAST(entitySolid), "Looks like a simple shape.");
        b.method("void glow(float size, const Color &in color)", NX_OBJ_LAST(entityGlow), "Looks like a soft ball of light. `light` is what lights other things.");
        b.method("void skin(Texture@+ texture, bool repeat = false)", NX_OBJ_LAST(entitySkin), "Puts an image on it, instead of any of the model's own.");
        b.method("void effect(Effect@+ effect)", NX_OBJ_LAST(entityEffect), "Colours it, and may move its corners, with a shader of your own; null for none. Where there are no shaders it stays as it is.");
        b.method("bool get_impostor() const property", NX_OBJ_LAST(getImpostor), "Whether, further away than `Space::impostors`, it is drawn as a picture of itself.");
        b.method("void set_impostor(bool) property", NX_OBJ_LAST(setImpostor), "Further away than `Space::impostors`, drawn as a picture of itself, made from its model: for trees and the like, of which there are many.");
        b.method("void sprite(Texture@+ sheet, uint angles = 1, uint frames = 1)", NX_OBJ_LAST(entitySprite), "Looks like a picture that stands upright and turns to face the camera, as big as its size. Across the sheet, how it looks from each side: the first from in front, then going round to its left; down it, the frames. null for none.");
        b.method("uint get_spriteFrame() const property", NX_OBJ_LAST(getSpriteFrame), "Which row of its sprite sheet it shows.");
        b.method("void set_spriteFrame(uint) property", NX_OBJ_LAST(setSpriteFrame), "Which row of its sprite sheet it shows, from 0.");
        b.method("float get_sway() const property", NX_OBJ_LAST(getSway), "How far the wind leans it: for each metre of height, at the strongest gust. 0.03 suits a tree, 0.1 grass.");
        b.method("void set_sway(float) property", NX_OBJ_LAST(setSway), "How far the wind leans it: for each metre of height, at the strongest gust. 0.03 suits a tree, 0.1 grass.");
        b.method("float get_shine() const property", NX_OBJ_LAST(getShine), "How much it shows a highlight from the sun.");
        b.method("void set_shine(float) property", NX_OBJ_LAST(setShine), "How much it shows a highlight from the sun, 0 to 1.");
        b.method("float get_glowing() const property", NX_OBJ_LAST(getGlowing), "How bright it is by itself.");
        b.method("void set_glowing(float) property", NX_OBJ_LAST(setGlowing), "1 is as bright as its colour, even in the dark.");
        b.method("bool get_twoSided() const property", NX_OBJ_LAST(getTwoSided), "Whether the backs of its triangles are drawn.");
        b.method("void set_twoSided(bool) property", NX_OBJ_LAST(setTwoSided), "Draws the backs of its triangles too.");
        b.method("bool get_shadow() const property", NX_OBJ_LAST(getShadow), "Whether it casts one.");
        b.method("void set_shadow(bool) property", NX_OBJ_LAST(setShadow), "Whether it casts one.");
        b.method("uint get_detail() const property", NX_OBJ_LAST(getDetail), "The level it was last drawn at: 0 is all of it, 2 the least.");
        b.method("void light(const Color &in color, float range)", NX_OBJ_LAST(entityLight), "Lights what is within so many metres.");
        b.method("bool animate(const string &in name, bool loop = true, float change = 0.25)", NX_OBJ_LAST(entityAnimate), "Plays one of its model's animations, changing to it over some seconds. Asking again for what is playing does nothing. False if it isn't there, or hasn't arrived.");
        b.method("bool get_animated() const property", NX_OBJ_LAST(getAnimated), "Whether an animation that doesn't loop has finished.");
        b.method("float get_animationSpeed() const property", NX_OBJ_LAST(getAnimationSpeed), "How fast its animation plays.");
        b.method("void set_animationSpeed(float) property", NX_OBJ_LAST(setAnimationSpeed), "1 is normal.");

        b.method("Vec3 get_motion() const property", NX_OBJ_LAST(getMotion), "Metres a second.");
        b.method("void set_motion(const Vec3 &in) property", NX_OBJ_LAST(setMotion), "Sets it going.");
        b.method("Vec3 get_turning() const property", NX_OBJ_LAST(getTurning), "Degrees a second about each axis.");
        b.method("void set_turning(const Vec3 &in) property", NX_OBJ_LAST(setTurning), "Sets it turning.");
        b.method("bool get_walker() const property", NX_OBJ_LAST(getWalker), "Whether it is one.");
        b.method("void set_walker(bool) property", NX_OBJ_LAST(setWalker), "Makes it a character: it stays upright, slides along walls, climbs steps and pushes what has mass. It needs a capsule, and feels gravity.");
        b.method("Vec3 get_groundNormal() const property", NX_OBJ_LAST(getGroundNormal), "Of a walker: which way the ground under it faces.");
        b.method("void emit(const string &in particles, float rate)", NX_OBJ_LAST(entityEmit), "Gives off so many particles a second, upwards from it as it is turned. The styles are read by Scene::load.");
        b.method("void push(const Vec3 &in impulse)", NX_OBJ_LAST(entityPush), "Pushes it at its middle.");
        b.method("void push(const Vec3 &in impulse, const Vec3 &in at)", NX_OBJ_LAST(entityPushAt), "Pushes it at a place, which may turn it.");
        b.method("void sphere(float radius, uint group = 0, uint hits = 0)", NX_OBJ_LAST(entitySphere), "Gives it a round collider. mass, wall, friction, bounce, gravity, drag, hits and touching are as on the plane.");
        b.method("void cube(const Vec3 &in size, uint group = 0, uint hits = 0)", NX_OBJ_LAST(entityCube), "The same with a box, which turns with it.");
        b.method("void capsule(float radius, float height, uint group = 0, uint hits = 0)", NX_OBJ_LAST(entityCapsule), "The same with an upright cylinder with round ends, included in its height.");
        b.method("void exact(uint group = 0, uint hits = 0)", NX_OBJ_LAST(entityExact), "The same with every triangle of its model, from both sides, so that what has an inside can be gone into. For walls: what moves is a box instead, as is a model with bones.");
        b.method("Vec3 get_colliderAt() const property", NX_OBJ_LAST(getColliderAt), "Where its collider is, from its place.");
        b.method("void set_colliderAt(const Vec3 &in) property", NX_OBJ_LAST(setColliderAt), "Moves its collider from its place: up by half its height, for something whose place is at its feet.");

        b.method("bool get_streamed() const property", NX_OBJ_LAST(getStreamed), "Whether it goes with its region.");
        b.method("void set_streamed(bool) property", NX_OBJ_LAST(setStreamed), "Destroys it when the region of endless land that it is in goes.");

        b.enumType("FieldShape", "Shapes that can melt into each other.");
        b.enumValue("FieldSphere", static_cast<int>(gpu::FieldKind::Sphere), "size.x is its radius.");
        b.enumValue("FieldBox", static_cast<int>(gpu::FieldKind::Box), "size is half of it, along each axis.");
        b.enumValue("FieldTorus", static_cast<int>(gpu::FieldKind::Torus), "size.x is the radius of the ring, size.y of the tube.");
        b.enumValue("FieldCapsule", static_cast<int>(gpu::FieldKind::Capsule), "size.x is its radius, size.y half the height of its straight part.");
        b.enumValue("FieldPlane", static_cast<int>(gpu::FieldKind::Plane), "Everything below a height.");
        b.enumValue("FieldCylinder", static_cast<int>(gpu::FieldKind::Cylinder), "size.x is its radius, size.y half its height.");
        b.enumType("FieldJoin", "What a shape does to those before it.");
        b.enumValue("JoinAdd", static_cast<int>(gpu::FieldOperation::Join), "Is joined to them.");
        b.enumValue("JoinCut", static_cast<int>(gpu::FieldOperation::Cut), "Is cut out of them.");
        b.enumValue("JoinKeep", static_cast<int>(gpu::FieldOperation::Keep), "Keeps only what is in both.");

        b.section("Space", "The scene in three dimensions: x right, y up, z towards you, in metres. Entities are spawned into it, and it is stepped with the rest of the scene.");
        b.names("Space");
        b.function("Entity@ spawn(const Vec3 &in place)", NX_FN(spaceSpawn), "A new, empty entity in space.");
        b.function("void draw(Canvas@ canvas)", NX_FN(spaceDraw), "Draws everything, from the camera. Call it from draw(), after design(); what is drawn on the canvas after it is in front.");
        b.function("void draw(Canvas@ canvas, float x, float y, float w, float h)", NX_FN(spaceDrawIn), "The same in part of the screen. Move the camera and draw again for a split screen.");
        b.function("void camera(const Vec3 &in from, const Vec3 &in to, float angle = 60)", NX_FN(spaceCamera), "Where it is, what it looks at, and the degrees it sees from top to bottom.");
        b.function("void sun(const Vec3 &in direction, const Color &in color)", NX_FN(spaceSun), "Light from far away; the direction is towards it.");
        b.function("void ambient(const Color &in sky, const Color &in ground)", NX_FN(spaceAmbient), "Light from everywhere: on what faces up, and on what faces down.");
        b.function("void sky(const Color &in top, const Color &in horizon)", NX_FN(spaceSky), "Draws a sky, with the sun in it. Without one, what is on the canvas already is behind.");
        b.function("void fog(const Color &in color, float start, float end)", NX_FN(spaceFog), "Hides what is far away.");
        b.function("void grass(Model@+ tuft, float perSquareMetre = 3, float distance = 30)", NX_FN(spaceGrass), "Grows tufts of a model over the land near the camera, where it is green, flat enough and dry, coloured as the land is there; they bend in the wind. null for none.");
        b.function("bool navigate(const Vec3 &in middle, float size, float cell = 1, float climb = 0.6f, uint groups = 0xFFFD)", NX_FN(spaceNavigate), "Works out where things can walk, in squares over a part of the world as it is now: what is under each square, of the groups given (all but group 1, those that walk, unless told otherwise), not too steep and not under water; a step from one to the next no higher than a climb. Up to 256 squares a side. Again if the world changes.");
        b.function("bool path(const Vec3 &in from, const Vec3 &in to, array<Vec3>@+ way)", NX_FN(spacePath), "The way from one place to another, round what is in the way, made as straight as it can be: the places to go through, the first where it starts. False if the end can't be reached, and then the way as near to it as can be got.");
        b.function("bool walkable(const Vec3 &in at)", NX_FN(spaceWalkable), "Whether something can walk there, as `navigate` worked it out.");
        b.function("Vec3 nearestWalkable(const Vec3 &in at)", NX_FN(spaceNearestWalkable), "The nearest place that can be walked on, or the place itself if there is none.");
        b.function("float get_impostors() property", NX_FN(spaceImpostors), "How far away things with `impostor` become pictures of themselves; 0 for never.");
        b.function("void set_impostors(float) property", NX_FN(spaceSetImpostors), "How far away things with `impostor` become pictures of themselves; 0 for never.");
        b.function("Vec3 get_wind() property", NX_FN(spaceWind), "Which way the wind blows, and how hard: 1 is a breeze. Things with `sway` lean with it.");
        b.function("void set_wind(const Vec3 &in) property", NX_FN(spaceSetWind), "Which way the wind blows, and how hard: 1 is a breeze; nothing still air. Things with `sway` lean with it.");
        b.function("bool get_lines() property", NX_FN(spaceLines), "Whether only the edges of triangles are drawn.");
        b.function("void set_lines(bool) property", NX_FN(spaceSetLines), "Draws only those, to see how things are made.");
        b.function("void shadows(float strength, float distance = 40)", NX_FN(spaceShadows), "How dark they are, 0 to 1, and how far they reach. Simple ones are drawn where the GPU can't do real ones.");
        b.function("void farthest(float distance)", NX_FN(spaceFarthest), "Nothing beyond is drawn.");
        b.function("Vec3 get_gravity() property", NX_FN(spaceGetGravity), "What pulls on those that feel it.");
        b.function("void set_gravity(const Vec3 &in) property", NX_FN(spaceGravity), "Metres a second, each second. It starts as that of the Earth.");
        b.function("void ground(float height)", NX_FN(spaceGround), "Something flat to stand on, everywhere. It isn't drawn.");
        b.function("void water(float height, const Color &in color)", NX_FN(spaceWater), "Draws water, with waves, up to a height.");
        b.function("void land(uint seed, uint squares, float spacing, float height, float roughness = 0.5, bool island = false)", NX_FN(spaceLand), "Makes hills: so many squares each way (a multiple of 32, up to 256), metres to a square, and about how high. An island goes down all round, to below 0.");
        b.function("void endlessLand(uint seed, float spacing, float height, float roughness = 0.5, uint reach = 5)", NX_FN(spaceEndless), "The same without end: regions of 32 squares are made in the background as the camera comes near, up to so many away, and go when it leaves.");
        b.function("bool nextRegion(int &out x, int &out z, bool &out arrived)", NX_FN(spaceNextRegion), "A region that has come or gone, or false when there is no more to tell. Ask each update, to put things in those that arrive; make them streamed and they go by themselves.");
        b.function("float get_regionSize() property", NX_FN(spaceRegionSize), "Metres along a side. Region x, z begins at that many times x and z.");
        b.function("uint get_regions() property", NX_FN(spaceRegions), "How many there are now.");
        b.function("uint get_regionsWaiting() property", NX_FN(spaceRegionsWaiting), "How many are still to come.");
        b.function("int load(const string &in name, const Vec3 &in at = Vec3(), bool streamed = false)", NX_FN(spaceLoad), "Makes the entities listed in a file, around a place, and returns how many. Their models arrive in the background. See the guide.");
        b.function("Entity@ make(Json@+ thing, const Vec3 &in at = Vec3())", NX_FN(spaceMake), "One entity, as such a file describes it.");
        b.function("void landColors(const Color &in low, const Color &in middle, const Color &in high, const Color &in steep, float lowHeight, float highHeight)", NX_FN(spaceLandColors), "Colours them by height and slope.");
        b.function("void flatten(float x, float z, float radius)", NX_FN(spaceFlatten), "Levels the land around a place, to build on.");
        b.function("void flatten(float x, float z, float radius, float height)", NX_FN(spaceFlattenTo), "The same, to a height.");
        b.function("void raise(float x, float z, float radius, float by)", NX_FN(spaceRaise), "Raises the land by hand, most in the middle; lowers it if that is less than 0. Endless land too, and what is done is kept when it goes.");
        b.function("void smooth(float x, float z, float radius, float amount)", NX_FN(spaceSmooth), "Wears it down towards what is round about: from 0, nothing, to 1.");
        b.function("void level(float x, float z, float radius, float height, float amount)", NX_FN(spaceLevel), "Brings it nearer to a height: from 0, nothing, to 1, all the way. Over what flatten() did.");
        b.function("void paint(float x, float z, float radius, const Color &in color, float amount)", NX_FN(spacePaint), "Colours it. A clear colour takes paint away.");
        b.function("void hole(float x, float z, float radius, bool open = true)", NX_FN(spaceHole), "Opens the land, or closes it again, so that there can be something under it.");
        b.function("bool get_edges() property", NX_FN(spaceEdges), "Whether there are walls at the sides of the land.");
        b.function("void set_edges(bool) property", NX_FN(spaceSetEdges), "Puts walls there, that can't be seen: nothing with a collider that moves can leave the land.");
        b.function("void skyPicture(Texture@+ picture)", NX_FN(spaceSkyPicture), "A picture all round, twice as wide as high, instead of the sky's colours. Null for none.");
        b.function("string get_landEdits() property", NX_FN(spaceLandEdits), "All that was raised and painted by hand, as text to keep. Empty if there is nothing.");
        b.function("bool restoreLand(const string &in edits)", NX_FN(spaceRestoreLand), "Puts all that back, instead of what there is. False if it is damaged.");
        b.function("float height(float x, float z)", NX_FN(spaceHeight), "How high the land or the ground is. Far below if there is none.");
        b.function("Vec3 normal(float x, float z)", NX_FN(spaceNormal), "Which way it faces there.");
        b.function("float get_landSize() property", NX_FN(spaceLandSize), "Metres along a side. Its middle is at 0, 0.");
        b.function("void field(FieldShape kind, const Vec3 &in place, const Vec3 &in size, const Color &in color, float soft = 0, FieldJoin how = JoinAdd, float turn = 0)", NX_FN(spaceShape), "Adds one of up to 24 shapes drawn by marching along rays, which melt together where they are soft.");
        b.function("void clearField()", NX_FN(spaceClearShapes), "Takes them all away. To move them, clear and add them again each update.");
        b.function("void fieldLook(float shine, float reflection = 0, float repeat = 0, float farthest = 60, uint steps = 80)", NX_FN(spaceShapeLook), "How they look; repeat makes the whole thing again every so many metres.");
        b.function("void burst(const string &in particles, const Vec3 &in place, uint count, const Vec3 &in aim = Vec3(0, 1, 0))", NX_FN(spaceBurst), "Throws out some particles. A spread of 360 goes every way, 90 is a cone.");
        b.function("bool raycast(const Vec3 &in from, const Vec3 &in to, Vec3 &out hit, Vec3 &out normal, uint groups = 0xFFFF, Entity@+ except = null)", NX_FN(spaceRaycast), "Whether there is anything on a line, where, and which way it faces. The land counts as group 0.");
        b.function("Entity@ pick(const Vec3 &in from, const Vec3 &in to, Vec3 &out hit, uint groups = 0xFFFF, Entity@+ except = null)", NX_FN(spacePick), "The first collider on a line, or null if there is none or the land comes first.");
        b.function("array<Entity@>@ within(const Vec3 &in place, float radius, uint groups = 0xFFFF)", NX_FN(spaceWithin), "Everything whose collider touches a ball.");
        b.function("bool toScreen(const Vec3 &in place, Vec2 &out screen)", NX_FN(spaceToScreen), "Where a place was when last drawn. False if it is behind the camera.");
        b.function("Vec3 ray(const Vec2 &in screen)", NX_FN(spaceRay), "The direction from the camera through a place on the screen.");
        b.function("Vec3 get_eye() property", NX_FN(spaceEye), "Where the camera is.");
        b.function("uint get_drawn() property", NX_FN(spaceDrawn), "How many entities were drawn last.");
        b.function("uint drawnAt(uint level)", NX_FN(spaceDrawnAt), "How many at a level of detail: 0 is all of it, 2 the least.");
        b.function("uint get_hidden() property", NX_FN(spaceHidden), "How many were out of sight or too small.");
        b.function("uint get_triangles() property", NX_FN(spaceTriangles), "How many triangles there were in the last frame.");
        b.function("uint get_contacts() property", NX_FN(spaceContacts), "Where rigid bodies touched in the last step.");
        b.names("");
        b.function("Vec3 lerp(const Vec3 &in a, const Vec3 &in b, float t)", NX_FN(lerpVec3), "From a to b as t goes from 0 to 1.");
    }
}
