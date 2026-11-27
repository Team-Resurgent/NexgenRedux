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
        u32 modelBones(const ModelRef* m) { return m->asset->model ? m->asset->model->bones.count() : 0; }
        std::string modelBone(u32 index, const ModelRef* m) { return index < modelBones(m) ? m->asset->model->bones[index].name : ""; }

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
                // See-through, if its colour is.
                a->blend = color.a < 1 ? gpu::Blend::Alpha : gpu::Blend::None;
                a->casts = color.a >= 1;
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
        // In space it lights what is within so many metres; on the plane, so many units round it.
        void entityLight(const Color& color, float range, EntityRef* e)
        {
            if (e->world == nullptr)
            {
                return;
            }
            if (e->world->space().getSpatial(e->id) == nullptr)
            {
                if (range <= 0)
                {
                    e->world->removeLight(e->id);
                }
                else if (Light2* l = e->world->light(e->id))
                {
                    l->color = color;
                    l->radius = range;
                }
                return;
            }
            if (Light3* l = e->world->space().light(e->id))
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
        bool entityAttachBone(EntityRef* parent, const std::string& bone, EntityRef* e)
        {
            return e->world && (parent == nullptr || parent->world == e->world) && e->world->space().attachToBone(e->id, parent ? parent->id : 0, bone.c_str());
        }
        u32 entityBones(EntityRef* e) { return e->world ? e->world->space().boneCount(e->id) : 0; }
        std::string entityBone(u32 index, EntityRef* e) { return e->world ? e->world->space().boneName(e->id, index) : ""; }
        bool getAnimated(const EntityRef* e) { return e->world == nullptr || e->world->space().played(e->id); }
        bool entityBlend(CScriptArray* names, CScriptArray* at, float change, EntityRef* e)
        {
            if (e->world == nullptr || names == nullptr || at == nullptr || names->GetSize() != at->GetSize() || names->GetSize() > Moves::kBlends)
            {
                return false;
            }
            const char* list[Moves::kBlends];
            float points[Moves::kBlends];
            for (u32 i = 0; i < names->GetSize(); i++)
            {
                list[i] = static_cast<const std::string*>(names->At(i))->c_str();
                points[i] = *static_cast<const float*>(at->At(i));
            }
            return e->world->space().blend(e->id, list, points, names->GetSize(), change);
        }
        Moves* movesOf(const EntityRef* e) { return e->world ? e->world->space().moves(e->id) : nullptr; }
        float getBlendBy(const EntityRef* e) { const Appearance* a = someLook(e); return a && a->moves ? a->moves->blendBy : 0; }
        void setBlendBy(float v, EntityRef* e) { if (Moves* m = movesOf(e)) m->blendBy = v; }
        bool getRootMotion(const EntityRef* e) { const Appearance* a = someLook(e); return a && a->moves && a->moves->root; }
        void setRootMotion(bool v, EntityRef* e) { if (Moves* m = movesOf(e)) m->root = v; }
        bool entityAnimateLayer(const std::string& name, const std::string& bone, bool loop, float change, EntityRef* e)
        {
            return e->world && e->world->space().playLayer(e->id, name.c_str(), bone.c_str(), loop, change);
        }
        void entityStopLayer(float change, EntityRef* e) { if (e->world) e->world->space().stopLayer(e->id, change); }
        bool getLayerPlayed(const EntityRef* e) { return e->world == nullptr || e->world->space().layerPlayed(e->id); }
        bool entityLookAt(const std::string& bone, const Vec3& place, float amount, EntityRef* e)
        {
            return e->world && e->world->space().lookAt(e->id, bone.c_str(), place, amount);
        }
        bool entityRagdoll(bool limp, const Vec3& push, float change, EntityRef* e)
        {
            if (e->world == nullptr)
            {
                return false;
            }
            const Body3* body = e->world->space().getBody(e->id);
            return e->world->space().ragdoll(e->id, limp, body && body->mass > 0 ? body->mass : 60, push, change);
        }
        bool getRagdolled(const EntityRef* e) { return e->world && e->world->space().ragdolled(e->id); }
        void entityLookAway(EntityRef* e)
        {
            const Appearance* a = someLook(e);
            if (a && a->moves)
            {
                a->moves->lookWanted = 0;
            }
        }
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
        EntityRef* spaceDecal(const TextureRef* picture, const Vec3& place, const Vec3& facing, float size, float degrees, const Color& color)
        {
            World& w = *services().world;
            const u32 id = w.space().decal(picture ? picture->asset : nullptr, place, facing, size, degrees, color);
            return id ? w.ref(id) : nullptr;
        }
        u32 spaceDecalLimit() { return space().decalLimit(); }
        void spaceSetDecalLimit(u32 v) { space().setDecalLimit(v); }
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
        void spaceWaterWaves(float height, float length, float speed, float choppy) { space().setWaterWaves(height, length, speed, choppy); }
        void spaceWaterColors(const Color& shallow, const Color& deep) { space().setWaterColors(shallow, deep); }
        void spaceWaterFoam(float shore, float crests) { space().setWaterFoam(shore, crests); }
        u32 spaceMist(const Vec3& middle, const Vec3& size, const Color& color, float thickness, float swirl, float grain) { return space().addMist(middle, size, color, thickness, swirl, grain); }
        bool spaceMoveMist(u32 id, const Vec3& middle) { return space().moveMist(id, middle); }
        void spaceRemoveMist(u32 id) { space().removeMist(id); }
        void spaceClearMists() { space().clearMists(); }
        bool spaceVoxels(const Vec3& corner, u32 across, u32 high, u32 deep, float size) { return space().makeVoxels(corner, across, high, deep, size); }
        void spaceClearVoxels() { space().clearVoxels(); }
        u32 spaceVoxel(int x, int y, int z) { return space().voxel(x, y, z); }
        bool spaceSetVoxel(int x, int y, int z, u32 kind) { return space().setVoxel(x, y, z, kind); }
        u32 spaceFillVoxels(int x0, int y0, int z0, int x1, int y1, int z1, u32 kind) { return space().fillVoxels(x0, y0, z0, x1, y1, z1, kind); }
        u32 spaceVoxelBall(const Vec3& middle, float radius, u32 kind) { return space().voxelBall(middle, radius, kind); }
        void spaceVoxelHills(u32 seed, float height, float roughness, int sea, float caves) { space().voxelHills(seed, height, roughness, sea, caves); }
        void spaceVoxelColor(u32 kind, const Color& top, const Color& side) { space().setVoxelColor(kind, top, side); }
        int spaceVoxelTop(int x, int z) { return space().voxelTop(x, z); }
        Vec3 spaceVoxelCell(const Vec3& at) { return space().voxelCell(at); }
        Vec3 spaceVoxelPlace(int x, int y, int z) { return space().voxelPlace(x, y, z); }
        bool spaceVoxelRay(const Vec3& from, const Vec3& to, Vec3& cell, Vec3& face, Vec3& hit) { return space().voxelRay(from, to, cell, face, hit); }
        u32 spaceVoxelChunks() { return space().voxelChunks(); }
        u32 spaceVoxelsWaiting() { return space().voxelsWaiting(); }
        void spaceWaterStyle(const std::string& name)
        {
            Space& s = space();
            s.setWaterCartoon(name == "cartoon");
            if (name == "calm") { s.setWaterWaves(0.05f, 6, 0.6f, 0); s.setWaterFoam(0.5f, 0); }
            else if (name == "sea") { s.setWaterWaves(0.18f, 9, 1, 0.2f); s.setWaterFoam(1, 0); }
            else if (name == "choppy") { s.setWaterWaves(0.45f, 10, 1.2f, 0.75f); s.setWaterFoam(1, 0.6f); }
            else if (name == "storm") { s.setWaterWaves(1.3f, 18, 1.3f, 0.9f); s.setWaterFoam(1, 1); }
            else if (name == "cartoon") { s.setWaterWaves(0.12f, 8, 0.8f, 0.3f); s.setWaterFoam(1.2f, 0); }
            else scriptError("water styles are calm, sea, choppy, storm and cartoon");
        }
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
        float clampFloat(float v, float low, float high) { return v < low ? low : v > high ? high : v; }
        void spaceOcclusion(float strength, float radius)
        {
            space().look().occlusion = clampFloat(strength, 0, 1);
            space().look().occlusionRadius = radius > 0.05f ? radius : 0.05f;
        }
        void spaceReflections(float strength) { space().look().reflections = clampFloat(strength, 0, 1); }
        void spaceFocus(float distance, float depth, float blur, float falloff, float bokeh)
        {
            gpu::SpaceLook& l = space().look();
            l.focus = distance > 0 ? distance : 0;
            l.focusDepth = depth > 0 ? depth : 0;
            l.focusBlur = clampFloat(blur, 0, 40);
            l.focusFalloff = falloff > 0.01f ? falloff : 0.01f;
            l.bokeh = clampFloat(bokeh, 0, 4);
        }
        void spaceExposure(float brightness, float range, float speed)
        {
            space().look().exposure = clampFloat(brightness, 0, 1);
            space().look().exposureRange = range > 1 ? range : 1;
            space().setExposureSpeed(speed);
        }
        bool spaceSeesDepth() { return space().seesDepth(); }
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

        u32 jointId(const EntityRef* e) { return e && e->world ? e->id : 0; }
        bool spaceJoin(const EntityRef* a, const EntityRef* b, const Vec3& at) { return jointId(a) && space().pin(a->id, jointId(b), at); }
        bool spaceHinge(const EntityRef* a, const EntityRef* b, const Vec3& at, const Vec3& axis) { return jointId(a) && space().hinge(a->id, jointId(b), at, axis); }
        bool spaceRope(const EntityRef* a, const EntityRef* b, const Vec3& atA, const Vec3& atB, float length) { return jointId(a) && space().rope(a->id, jointId(b), atA, atB, length); }
        bool spaceSpring(const EntityRef* a, const EntityRef* b, const Vec3& atA, const Vec3& atB, float stiffness, float damping, float length)
        {
            return jointId(a) && space().spring(a->id, jointId(b), atA, atB, stiffness, damping, length);
        }
        void spaceUnjoin(const EntityRef* e) { if (jointId(e)) space().unjoin(e->id); }
        u32 spaceJoints() { return space().joints(); }

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
        b.method("uint get_bones() const property", NX_OBJ_LAST(modelBones), "How many bones it bends by, which things can be attached to.");
        b.method("string bone(uint index) const", NX_OBJ_LAST(modelBone), "The name of one, such as \"hand.r\".");

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
        b.method("void light(const Color &in color, float range)", NX_OBJ_LAST(entityLight), "Lights what is near it: in space, what is within so many metres; on the plane, so many units round it, fading to nothing, with shadows behind solid tiles and occluders, while Scene::ambient isn't white. A range of 0 puts out a light on the plane.");
        b.method("bool attach(Entity@+ parent, const string &in bone)", NX_OBJ_LAST(entityAttachBone), "Moves with one of the parent's model's bones, by name, as a hand holds a sword or a head wears a hat: its place, turn and size are then from that bone, and it follows every animation. False if either isn't in space.");
        b.method("uint get_bones() const property", NX_OBJ_LAST(entityBones), "How many bones its model has, once it has come.");
        b.method("string bone(uint index) const", NX_OBJ_LAST(entityBone), "The name of one.");
        b.method("bool animate(const string &in name, bool loop = true, float change = 0.25)", NX_OBJ_LAST(entityAnimate), "Plays one of its model's animations, changing to it over some seconds. Asking again for what is playing does nothing. False if it isn't there, or hasn't arrived.");
        b.method("bool get_animated() const property", NX_OBJ_LAST(getAnimated), "Whether an animation that doesn't loop has finished.");
        b.method("float get_animationSpeed() const property", NX_OBJ_LAST(getAnimationSpeed), "How fast its animation plays.");
        b.method("void set_animationSpeed(float) property", NX_OBJ_LAST(setAnimationSpeed), "1 is normal.");
        b.method("bool animateBlend(array<string>@+ names, array<float>@+ at, float change = 0.25)", NX_OBJ_LAST(entityBlend), "Plays several of its model's animations at once, mixed by `blendBy`: each is at a point along a line, as idle at 0, walk at 1.5 and run at 4 for its speed, and between two points it is some of each, their steps kept together. Changes to it over some seconds; asking again only changes the points. Up to 8. False if any isn't there, or the model hasn't arrived.");
        b.method("float get_blendBy() const property", NX_OBJ_LAST(getBlendBy), "Where along the line of `animateBlend` it is.");
        b.method("void set_blendBy(float) property", NX_OBJ_LAST(setBlendBy), "Where along the line of `animateBlend` it is: its speed, say.");
        b.method("bool animateLayer(const string &in name, const string &in bone, bool loop = true, float change = 0.25)", NX_OBJ_LAST(entityAnimateLayer), "Plays an animation over one bone and those that hang from it, on top of the rest, as waving while walking: \"chest\" for the top half. Comes in over some seconds. Asking again for what is playing does nothing. False if either isn't there.");
        b.method("void stopLayer(float change = 0.25)", NX_OBJ_LAST(entityStopLayer), "Lets the layer go over some seconds.");
        b.method("bool get_layerPlayed() const property", NX_OBJ_LAST(getLayerPlayed), "Whether there is no layer, or one that doesn't loop has finished, or it is going.");
        b.method("bool get_rootMotion() const property", NX_OBJ_LAST(getRootMotion), "Whether its animations move it.");
        b.method("void set_rootMotion(bool) property", NX_OBJ_LAST(setRootMotion), "Where its animations carry the model's highest bone along the ground, the whole thing goes instead, so its feet don't slide: a walker so moved still stops at walls.");
        b.method("bool lookAt(const string &in bone, const Vec3 &in place, float amount = 1)", NX_OBJ_LAST(entityLookAt), "Turns a bone of its model, as a head, toward a place, up to 75 degrees, over whatever its animation does; amount is how much. It turns there smoothly, and can be asked every update as the place moves. False if the bone isn't there.");
        b.method("void lookAway()", NX_OBJ_LAST(entityLookAway), "Turns the bone back to what its animation does.");
        b.method("bool ragdoll(bool limp, const Vec3 &in push = Vec3(), float change = 0.6f)", NX_OBJ_LAST(entityRagdoll), "Goes limp: a body for each bone of its model that moves some of its skin, pinned where the bones meet and swinging only so far, falls as it is pushed, sent off at a speed as well as how it was moving; its own collider isn't there meanwhile, and what hits a body hits it. `push` on it then pushes the body nearest the place. false gets it up again, into whatever it plays, over some seconds. False if its model doesn't bend.");
        b.method("bool get_ragdolled() const property", NX_OBJ_LAST(getRagdolled), "Whether it is limp.");

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
        b.function("void occlusion(float strength = 1, float radius = 0.6)", NX_FN(spaceOcclusion), "Darkens corners, creases and what is under things, as light reaches them less: ambient occlusion, from how far away each pixel is, within so many metres. 0 for none. Where `depthEffects`.");
        b.function("void reflections(float strength = 0.6)", NX_FN(spaceReflections), "Shows what is around in what faces up, wet floors and still water, where it is on the screen: screen-space reflections. 0 for none. Where `depthEffects`.");
        b.function("void focus(float distance, float depth = 1, float blur = 10, float falloff = 6, float bokeh = 1)", NX_FN(spaceFocus), "Depth of field: sharp at so many metres and depth either side, blurred further from it, most so falloff metres on, by blur pixels across (at 720 high); the brightest spread into round bokeh, by bokeh. A distance of 0 for none. Where `depthEffects`.");
        b.function("void autoExposure(float brightness = 0.4, float range = 2.5, float speed = 2)", NX_FN(spaceExposure), "Makes the view brighter or darker, gradually, toward an average brightness (0 to 1), by as much as range times either way, as eyes adjust going in and out of the dark. 0 for none. Where `depthEffects`.");
        b.function("bool get_depthEffects() property", NX_FN(spaceSeesDepth), "Whether this GPU can do occlusion, reflections, focus and autoExposure, and depthAt in picture effects: everywhere but the original Xbox.");
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
        b.function("void water(float height, const Color &in color)", NX_FN(spaceWater), "Draws water, with waves, up to a height: clearer in the shallows, darker where it is deep, the sky in it at a low angle, and foam where it meets the land.");
        b.function("uint mist(const Vec3 &in middle, const Vec3 &in size, const Color &in color = Color(0.85f, 0.88f, 0.92f, 0.6f), float thickness = 1, float swirl = 1, float grain = 6)", NX_FN(spaceMist), "Mist that can be walked into, in a box: billows of noise in three dimensions, drifting with the wind, soft at the box's edges. The colour's alpha is how much it hides, thickness more of the same, swirl how fast it moves, grain how many metres its billows are. Orange and swift, it is smoke or fire. Gives an id.");
        b.function("bool moveMist(uint id, const Vec3 &in middle)", NX_FN(spaceMoveMist), "Moves a mist to a new middle.");
        b.function("void removeMist(uint id)", NX_FN(spaceRemoveMist), "Takes a mist away.");
        b.function("void clearMists()", NX_FN(spaceClearMists), "Takes all the mists away.");
        b.function("bool voxels(const Vec3 &in corner, uint across, uint high, uint deep, float size = 1)", NX_FN(spaceVoxels), "A world of blocks from a corner: so many across, high and deep (up to 1024, 256 and 1024), each so many metres. Empty until filled. Things stand on the blocks and bump into them, and lines meet them, as they do the land: group 0.");
        b.function("void clearVoxels()", NX_FN(spaceClearVoxels), "Takes all the blocks away.");
        b.function("uint voxel(int x, int y, int z)", NX_FN(spaceVoxel), "The kind of block there: 0 is none. Outside the world, 0.");
        b.function("bool setVoxel(int x, int y, int z, uint kind)", NX_FN(spaceSetVoxel), "Puts a block there, or takes it away with 0. Whether anything changed. It is seen at once.");
        b.function("uint fillVoxels(int x0, int y0, int z0, int x1, int y1, int z1, uint kind)", NX_FN(spaceFillVoxels), "Fills a box of blocks, corners included, or empties it with 0. How many changed.");
        b.function("uint voxelBall(const Vec3 &in middle, float radius, uint kind = 0)", NX_FN(spaceVoxelBall), "Digs a round hole, in metres, or with a kind builds a ball. How many blocks changed.");
        b.function("void voxelHills(uint seed = 1, float height = 20, float roughness = 0.5f, int sea = 8, float caves = 0.5f)", NX_FN(spaceVoxelHills), "Fills the world with hills from a number: grass and earth over stone, sand at the sea's height and below, snow on the tops, and as many hollows under them as caves asks, from 0 to 1.");
        b.function("void voxelColor(uint kind, const Color &in top, const Color &in side)", NX_FN(spaceVoxelColor), "The colours of a kind of block, on top and on its sides. 1 grass, 2 earth, 3 stone, 4 sand, 5 wood, 6 leaves, 7 brick, 8 snow, 9 planks, 10 dark stone, 11 gold and 12 ice to begin with.");
        b.function("int voxelTop(int x, int z)", NX_FN(spaceVoxelTop), "The highest block there, or -1.");
        b.function("Vec3 voxelCell(const Vec3 &in place)", NX_FN(spaceVoxelCell), "Which block a place is in, in whole numbers.");
        b.function("Vec3 voxelPlace(int x, int y, int z)", NX_FN(spaceVoxelPlace), "The middle of a block, in metres.");
        b.function("bool voxelRay(const Vec3 &in from, const Vec3 &in to, Vec3 &out cell, Vec3 &out face, Vec3 &out hit)", NX_FN(spaceVoxelRay), "The first block on a line, which way the face it meets looks, and where: cell plus face is where a new block would go.");
        b.function("uint voxelChunks()", NX_FN(spaceVoxelChunks), "How many chunks of 16 blocks each way have anything in them.");
        b.function("uint voxelsWaiting()", NX_FN(spaceVoxelsWaiting), "How many chunks are still to be made into triangles: a few are, each frame.");
        b.function("void waterStyle(const string &in style)", NX_FN(spaceWaterStyle), "How the water moves and looks: calm, sea, choppy, storm or cartoon.");
        b.function("void waterWaves(float height = 0.18f, float length = 9, float speed = 1, float choppy = 0.2f)", NX_FN(spaceWaterWaves), "How high the waves are, how long from one to the next, how fast as waves that long go, and how sharp their crests: 0 round, 1 pointed.");
        b.function("void waterColors(const Color &in shallow, const Color &in deep)", NX_FN(spaceWaterColors), "Its colour where it is shallow and where it is deep; alpha is how much it hides.");
        b.function("void waterFoam(float shore = 1, float crests = 0)", NX_FN(spaceWaterFoam), "How much foam where it meets the land, and on the tops of waves.");
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
        b.function("bool join(Entity@+ a, Entity@+ b, const Vec3 &in at)", NX_FN(spaceJoin), "Pins two bodies together at a place, free to turn about it; null for b pins a to the world there. Both need colliders, and a mass to move.");
        b.function("bool hinge(Entity@+ a, Entity@+ b, const Vec3 &in at, const Vec3 &in axis)", NX_FN(spaceHinge), "The same, turning only about an axis through that place, as a door, a lid or a wheel does.");
        b.function("bool rope(Entity@+ a, Entity@+ b, const Vec3 &in atA, const Vec3 &in atB, float length = -1)", NX_FN(spaceRope), "A place on each kept no further apart than a length, as a rope or a chain does: it pulls but never pushes. A length below 0 is as far apart as they are now.");
        b.function("bool spring(Entity@+ a, Entity@+ b, const Vec3 &in atA, const Vec3 &in atB, float stiffness = 60, float damping = 3, float length = -1)", NX_FN(spaceSpring), "A place on each pulled towards a length and pushed away from it: stiffness in newtons a metre, damping slows the bouncing. A length below 0 is as far apart as they are now.");
        b.function("void unjoin(Entity@+ e)", NX_FN(spaceUnjoin), "Lets go of every joint it is in. They also go when it does.");
        b.function("uint get_joints() property", NX_FN(spaceJoints), "How many there are.");
        b.function("bool raycast(const Vec3 &in from, const Vec3 &in to, Vec3 &out hit, Vec3 &out normal, uint groups = 0xFFFF, Entity@+ except = null)", NX_FN(spaceRaycast), "Whether there is anything on a line, where, and which way it faces. The land counts as group 0.");
        b.function("Entity@ pick(const Vec3 &in from, const Vec3 &in to, Vec3 &out hit, uint groups = 0xFFFF, Entity@+ except = null)", NX_FN(spacePick), "The first collider on a line, or null if there is none or the land comes first.");
        b.function("Entity@ decal(Texture@+ picture, const Vec3 &in place, const Vec3 &in facing, float size, float degrees = 0, const Color &in color = White)", NX_FN(spaceDecal), "Lays a picture over whatever is at a place, as a sticker would be: a mark, a hole, a splash, a footprint. Facing is the way the surface there faces (a ray's normal), size how big across, degrees how it is turned. It wraps over the models and meshes there that don't bend, the land and the ground, and stays where it is put, as an entity of its own that can be coloured, faded or destroyed. null if there is nothing there. On every GPU.");
        b.function("uint get_decals() property", NX_FN(spaceDecalLimit), "How many decals there can be; past it, the oldest goes. 64 to begin with.");
        b.function("void set_decals(uint) property", NX_FN(spaceSetDecalLimit), "How many decals there can be; past it, the oldest goes. 0 for no limit.");
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
