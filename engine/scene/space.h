#pragma once

#include "assets/assets.h"
#include "assets/model.h"
#include "audio/audio.h"
#include "base/grow_array.h"
#include "base/json_tree.h"
#include "canvas/canvas.h"

#include <string>

namespace nx
{
    class World;

    // Meshes that every screen can use, made once.
    enum class Solid : u8 { None, Box, Sphere, Cylinder, Cone, Torus, Capsule, Plane, Count };

    class SolidMeshes : NonCopyable
    {
    public:
        ~SolidMeshes() { shutdown(); }
        void init(gpu::Device& device) { m_device = &device; }
        void shutdown();
        // Made when something first needs them. False if they can't be.
        bool make();

        // Coarser when it will be small on the screen: 0 to 2.
        gpu::Mesh get(Solid solid, u32 detail) const { return m_meshes[static_cast<size_t>(solid)][detail]; }
        gpu::Texture soft() const { return m_soft; }
        gpu::Texture grain() const { return m_grain; }  // of the ground, repeating
        gpu::Mesh square() const { return m_square; }  // upright, facing +z, for what faces the camera
        gpu::Mesh dome() const { return m_dome; }
        gpu::Device* device() const { return m_device; }
        // Whose sky the dome is coloured as.
        const void* domeOwner() const { return m_domeOwner; }
        void setDomeOwner(const void* owner) { m_domeOwner = owner; }

    private:
        gpu::Device* m_device = nullptr;
        bool m_made = false;
        const void* m_domeOwner = nullptr;
        gpu::Mesh m_meshes[static_cast<size_t>(Solid::Count)][3];
        gpu::Mesh m_square, m_dome;
        gpu::Texture m_soft, m_grain;
    };

    // A mesh that a script builds. Goes to the GPU when it is first drawn.
    struct CustomMesh
    {
        MeshData data;
        gpu::Device* device = nullptr;
        gpu::Mesh mesh;
        Vec3 middle;
        float radius = 0;
        int refs = 1;
        bool changed = true;

        void addRef() { refs++; }
        void release();
        void upload();
    };

    // A shader and what it is to be told, shared by all that use it together.
    struct EffectUse
    {
        Asset* asset;
        Assets* owner;
        int refs;
        float values[16];

        void addRef() { refs++; }
        void release()
        {
            if (--refs == 0)
            {
                owner->release(asset);
                destroy(this);
            }
        }
        bool failed() const { return asset->failed; }
        bool ready() const { return asset->ready; }
        bool works() const { return static_cast<bool>(asset->effect); }
        void set(u32 index, float value)
        {
            if (index < 16)
            {
                values[index] = value;
            }
        }
    };

    enum class Shape3 : u8 { Sphere, Box, Capsule };

    // Where something is. An entity that has one is in space rather than on the plane.
    struct Spatial
    {
        u32 owner;
        Vec3 place;
        Quat turn;
        Vec3 size;
        bool streamed;
        Mat4 world;
        float made[10];        // the place, turn and size that it was made from
        u32 version;           // goes up when it moves
        u32 parentVersion;
    };

    // What it looks like.
    struct Appearance
    {
        u32 owner;
        Asset* model;          // held
        Asset* texture;        // held; instead of the model's own
        CustomMesh* custom;    // held
        EffectUse* effect;     // held
        Solid solid;
        bool glowBall;         // a soft light that faces the camera
        bool twoSided, casts, repeat;
        gpu::Blend blend;
        gpu::Color color;
        float glow, shine;
        u8 detail;             // the level it was last drawn at
        // Movement of a model that bends.
        int animation, before;
        float time, beforeTime, speed;
        float change, changeFor;  // seconds left of changing from one to the other, and all of them
        bool loop;
        gpu::Mesh bent;
        bool bentReady;
        float bentAt;
    };

    struct Light3
    {
        u32 owner;
        gpu::Color color;
        float range;
    };

    struct Collider3
    {
        u32 owner;
        Shape3 shape;
        u8 group;
        bool wall;
        u16 hits;
        Vec3 half;             // a sphere's radius is x; a capsule's radius is x and its half height, ends and all, y
        Vec3 middle;           // from the entity's place
        // Where that is, kept until it moves or is changed.
        u32 version;
        Vec3 at, reach;
        Quat turn;
        Vec3 halves;
        Vec3 axes[3];
    };

    struct Body3
    {
        u32 owner;
        Vec3 velocity, spin;   // spin in radians a second, about its direction
        float mass, friction, bounce, drag, gravity;
        bool walker;           // an upright capsule that slides along things and climbs small steps
        bool onGround;
        Vec3 ground;           // which way the ground faces
        float still;           // seconds it has hardly moved
        u32 touched;           // how many things, last step
    };

    struct Emitter3
    {
        u32 owner;
        u32 style;
        float rate, owed;
    };

    struct Particle3
    {
        Vec3 place, velocity;
        float age, life;
        float turn, spin;   // radians
    };

    struct SpaceStats
    {
        u32 drawn = 0, hidden = 0;  // last draw
        u32 contacts = 0;
        u32 details[3] = {};
    };

    // The three-dimensional side of a World: y up, in metres.
    class Space : NonCopyable
    {
    public:
        Space(World& world, Assets& assets) : m_world(world), m_assets(assets) {}
        ~Space() { clear(); }

        void init(SolidMeshes* solids, Audio* audio = nullptr)
        {
            m_solids = solids;
            m_audio = audio;
        }
        void clear();
        // What was touching what.
        void forget();

        // --- Entities: indices into the world's ---

        Spatial* spatial(u32 id);
        Spatial* getSpatial(u32 id);
        const Spatial* getSpatial(u32 id) const { return const_cast<Space*>(this)->getSpatial(id); }
        Appearance* appearance(u32 id);
        Appearance* getAppearance(u32 id);
        Light3* light(u32 id);
        Collider3* collider(u32 id);
        Collider3* getCollider(u32 id);
        Body3* body(u32 id);
        Body3* getBody(u32 id);
        void setModel(u32 id, Asset* model);
        void setTexture(u32 id, Asset* texture);
        void setCustom(u32 id, CustomMesh* mesh);
        void setSolid(u32 id, Solid solid);
        void setEffect(u32 id, EffectUse* effect);
        // Nothing if it is already playing.
        bool play(u32 id, const char* animation, bool loop, float change);
        bool played(u32 id) const;
        Vec3 worldPlace(u32 id) const;
        void removed(u32 index);
        void transform(u32 index);

        // --- Particles: the world's styles, in metres ---

        // Thrown along a direction, as widely as the style spreads them. False for a style that isn't there.
        bool burst(const char* style, const Vec3& place, u32 count, const Vec3& aim);
        bool emit(u32 id, const char* style, float rate);
        u32 particles() const { return m_particleCount; }
        void clearParticles();

        // --- Sound ---

        // A voice that gets quieter with distance and comes from where it is: from an entity, or a place.
        void follow(Voice voice, u32 entity, const Vec3& place, float volume, float range);
        // How loud and how far to one side it would be.
        void hear(const Vec3& place, float volume, float range, float& gain, float& pan) const;

        // --- Surroundings ---

        void setCamera(const Vec3& from, const Vec3& to, float angle) { m_from = from; m_to = to; m_focusTo[m_focusCount < 3 ? m_focusCount : 3] = to; m_focus[m_focusCount < 3 ? m_focusCount++ : 3] = from; m_angle = angle < 5 ? 5 : angle > 150 ? 150 : angle; }
        void setSun(const Vec3& direction, const gpu::Color& color) { m_sun = direction.normalized(); m_sunColor = color; }
        void setAmbient(const gpu::Color& sky, const gpu::Color& ground) { m_ambientSky = sky; m_ambientGround = ground; }
        void setSky(const gpu::Color& top, const gpu::Color& horizon) { m_skyTop = top; m_skyHorizon = horizon; m_hasSky = true; }
        void setFog(const gpu::Color& color, float start, float end) { m_fog = color; m_fogStart = start; m_fogEnd = end; }
        // Only edges, to see how things are made.
        void setLines(bool lines) { m_lines = lines; }
        bool lines() const { return m_lines; }
        void setShadows(float strength, float distance) { m_shadowStrength = strength; m_shadowDistance = distance; }
        void setFar(float distance) { m_far = distance < 1 ? 1 : distance; }
        void setGravity(const Vec3& gravity) { m_gravity = gravity; }
        Vec3 gravity() const { return m_gravity; }
        void setGround(float height) { m_hasGround = true; m_ground = height; }
        void setWater(float height, const gpu::Color& color) { m_hasWater = true; m_water = height; m_waterColor = color; }

        // --- Land ---

        // Hills from a number: squares each way (a multiple of 32, up to 256), metres to a square. An island
        // goes down all round, to below 0.
        bool makeLand(u32 seed, u32 squares, float spacing, float height, float roughness, bool island);
        // The same without end: regions of 32 squares are made as the camera comes near, and go as it
        // leaves. Reach is how many regions away.
        bool makeEndlessLand(u32 seed, float spacing, float height, float roughness, u32 reach);
        // What has come or gone since it was last asked. False when there is no more.
        bool nextRegion(int& x, int& z, bool& arrived);
        float regionSize() const { return static_cast<float>(kChunk) * m_landSpacing; }
        u32 regions() const { return m_chunks.count(); }
        u32 regionsWaiting() const { return m_regionsWaiting; }
        bool streaming() const { return m_landOn && m_landSquares == 0; }
        // Something that goes when the region it is in does.
        void setStreamed(u32 id, bool streamed);
        // Entities from a file, at a place. How many, or -1 with the world's error set.
        int load(const char* path, const char* folder, const Vec3& at, bool streamed);
        // The same, of what has been read already.
        int load(const char* path, const FileData& file, const char* folder, const Vec3& at, bool streamed);
        // One thing, as a file has them. 0 if there is something wrong with it.
        u32 make(JsonTree::Value json, const char* folder, const Vec3& at);
        // Levels the land towards a height around a place.
        void flatten(float x, float z, float radius, float level);
        // By hand, where the land has an end: more in the middle of the radius than at its edge. False where
        // there is none. Over what flatten() did.
        bool raise(float x, float z, float radius, float by);
        bool smooth(float x, float z, float radius, float amount);
        bool level(float x, float z, float radius, float height, float amount);
        // A colour that is clear takes paint away.
        bool paint(float x, float z, float radius, const gpu::Color& color, float amount);
        // All of that, as text. Empty if there is none. It only fits land of the same number of squares.
        std::string landEdits() const;
        bool setLandEdits(const std::string& text);
        void setLandColors(const gpu::Color& low, const gpu::Color& middle, const gpu::Color& high, const gpu::Color& steep, float lowHeight, float highHeight);
        bool hasLand() const { return m_landOn; }
        // Of the land or the ground; far below if there is neither.
        float heightAt(float x, float z) const;
        Vec3 normalAt(float x, float z) const;
        float landSize() const { return static_cast<float>(m_landSquares) * m_landSpacing; }

        // --- Shapes by their distance ---

        gpu::Field& field() { return m_field; }
        // Their distance, as the GPU works it out.
        static float fieldDistance(const gpu::Field& field, const Vec3& at, gpu::Color* color);
        struct FieldTurns
        {
            float cosine[gpu::kFieldShapes], sine[gpu::kFieldShapes];
        };
        static void turnField(const gpu::Field& field, FieldTurns& turns);
        static float fieldDistance(const gpu::Field& field, const FieldTurns& turns, const Vec3& at, gpu::Color* color);
        // The same as triangles, for a GPU that can't, and for tests. Cells along each side of a cube.
        static void fieldMesh(const gpu::Field& field, const Vec3& middle, float half, u32 cells, MeshData& out);

        // --- Questions ---

        // The land counts as group 0, and has no id.
        bool raycast(const Vec3& from, const Vec3& to, u32 groups, u32 except, u32& id, Vec3& hit, Vec3& normal) const;
        u32 countGroup(u32 group) const;
        void push(u32 id, const Vec3& impulse, const Vec3* at);
        template <typename Fn>
        void within(const Vec3& place, float radius, u32 groups, Fn fn) const
        {
            for (const Collider3& c : m_colliders)
            {
                if ((groups & (1u << c.group)) && visibleCollider(c) && touchesSphere(c, place, radius))
                {
                    fn(idOf(c.owner));
                }
            }
        }
        bool touching(u32 a, u32 b) const;
        // In design units. False if it is behind the camera.
        bool toScreen(const Vec3& place, Vec2& out) const;
        // From the camera through a place on the screen.
        Vec3 rayThrough(const Vec2& screen) const;
        Vec3 cameraPlace() const { return m_from; }

        // --- Every step, and every frame ---

        void update(float dt);
        // Adds pairs of ids of what touches and wants to know.
        void collectPairs(GrowArray<u64>& pairs);
        // Into part of the canvas, in design units; none for all of it.
        void draw(Canvas& canvas, float x, float y, float w, float h);
        const SpaceStats& stats() const { return m_stats; }

    private:
        static constexpr u32 kNone = 0xFFFFFFFF;
        static constexpr u32 kChunk = 32;      // squares along a side
        static constexpr u32 kRow = kChunk + 3;
        static constexpr u32 kChunkDetails = 3;

        // A region of the land.
        struct Chunk
        {
            int x, z;
            float* heights;        // (kChunk + 3) squared: one beyond each edge, for the slopes
            gpu::Mesh mesh;
            float low, high;
            bool changed;
        };

        struct Flat
        {
            float x, z, radius, level;
        };

        struct RegionEvent
        {
            int x, z;
            bool arrived;
        };

        // Something heard from a place.
        struct Heard
        {
            Voice voice;
            u32 entity;
            Vec3 place;
            float volume, range;
            float gain, pan;   // as last told
        };

        struct Frustum
        {
            float planes[6][4];
            bool sees(const Vec3& middle, float radius) const;
        };

        struct Item
        {
            float key;
            u32 index;
        };

        // A body as the solver sees it.
        struct Rigid
        {
            u32 entity;        // index, or kNone
            u32 body;          // or kNone for something that doesn't move
            Shape3 shape;
            Vec3 place, velocity, spin;
            Quat turn;
            Vec3 half;
            Vec3 axes[3];
            float inverseMass;
            Vec3 inverseInertia;  // along its own axes
            float friction, bounce;
            Vec3 low, high;
            bool onGround, still;
            u32 touching;
        };

        struct Box3
        {
            Vec3 low, high;
        };

        struct Contact
        {
            Vec3 place, normal;   // from a to b
            float depth;
            float normalImpulse, tangentImpulse[2];
            bool edge;
            // This step's.
            Vec3 fromA, fromB, tangents[2];
            float normalMass, tangentMass[2], bias;
        };

        struct Touch
        {
            u64 key;
            u32 a, b;             // b may be kLand
            u32 count;
            float friction, bounce;
            Contact contacts[4];
        };

        static constexpr u32 kLand = 0xFFFFFFFE;

        template <typename T>
        T* get(GrowArray<T>& parts, u32 kind, u32 id);
        template <typename T>
        T* getOrAdd(GrowArray<T>& parts, u32 kind, u32 id, const T& fresh);
        template <typename T>
        void removePart(GrowArray<T>& parts, u32 kind, u32 index);
        void freeAppearance(Appearance& a);

        bool touchesSphere(const Collider3& c, const Vec3& place, float radius) const;

        // space.cpp
        void animate(float dt);
        bool bend(Appearance& a);
        void drawAppearance(const Appearance& a, u32 detail, const gpu::View& view, bool shadow, bool see);
        void pickLights(const gpu::View& view, const Vec3& middle, float radius, gpu::MeshDraw& draw) const;
        void boundsOf(const Appearance& a, const Mat4& world, Vec3& middle, float& radius) const;
        void drawBlobs(const gpu::View& view, const Frustum& frustum);
        void emitParticles(float dt);
        void moveParticles(float dt);
        void drawParticles(const gpu::View& view);
        void listen();

        // land.cpp
        void buildChunk(Chunk& chunk);
        float generate(int x, int z) const;
        float corner(int x, int z) const;
        bool addChunk(int x, int z);
        void fillChunk(Chunk& chunk);
        void removeChunk(u32 index);
        const Chunk* chunkAt(int x, int z) const;
        void stream();
        void startLand(u32 seed, float spacing, float height, float roughness);
        bool readThing(JsonTree::Value json, const Vec3& at, bool streamed, const char* file, u32& id);
        void drawLand(const gpu::View& view, const Frustum& frustum, bool shadow);
        void drawWater(const gpu::View& view);
        gpu::Color landColor(float height, const Vec3& normal, float x, float z) const;

        // sculpt.cpp
        struct Brush
        {
            float x, z, radius;   // in squares
            int fromX, fromZ, toX, toZ;
        };
        bool brush(float x, float z, float radius, Brush& b);
        bool makeSculpt();
        template <typename F>
        bool change(float x, float z, float radius, const F& by);
        void moveCorners(int fromX, int fromZ, int toX, int toZ, const float* moved);
        float sculpted(int x, int z) const;
        const u8* painted(int x, int z) const;

        // physics3.cpp
        void physics(float dt);
        void addRigid(const Collider3& c, u32 body);
        void walk(float dt);
        void walkOne(u32 body, float dt);
        static u32 touchPair(const Rigid& a, const Rigid& b, Contact (&contacts)[4]);
        static u32 touchBall(const Vec3& at, float radius, const Rigid& box, Contact& contact);
        static u32 touchBalls(const Vec3& a, float ra, const Vec3& b, float rb, Contact& contact);
        void makeRigid(const Collider3& c, Rigid& r) const;
        void refresh(const Collider3& c) const;
        static void shapeRigid(Rigid& r);
        u32 pushOut(Rigid& r, Body3* b);
        bool visibleCollider(const Collider3& c) const;
        u32 idOf(u32 index) const;
        static u32 touchBoxes(const Rigid& a, const Rigid& b, Contact (&contacts)[4]);
        u32 touchLand(const Rigid& a, Contact (&contacts)[4]) const;
        static Vec3 inertia(const Rigid& r, const Vec3& v);
        void solve(float dt);
        bool overlap(const Collider3& a, const Collider3& b) const;
        void place(const Collider3& c, Vec3& middle, Quat& turn, Vec3& half) const;

        World& m_world;
        Assets& m_assets;
        SolidMeshes* m_solids = nullptr;

        GrowArray<Spatial> m_spatials;
        GrowArray<Appearance> m_appearances;
        GrowArray<Light3> m_lights;
        GrowArray<Collider3> m_colliders;
        GrowArray<Body3> m_bodies;
        GrowArray<Emitter3> m_emitters;
        GrowArray<Particle3> m_particles[16];
        u32 m_particleCount = 0;
        gpu::Mesh m_particleMeshes[6];      // used in turn, as each can be filled once a frame
        u32 m_particleMesh = 0;
        MeshData m_particleData;
        GrowArray<Heard> m_heard;
        Audio* m_audio = nullptr;

        Vec3 m_from = { 0, 2, 6 }, m_to;
        float m_angle = 60, m_far = 400;
        Vec3 m_sun = { 0.4f, 0.8f, 0.45f };
        gpu::Color m_sunColor = { 0.9f, 0.86f, 0.78f, 1 };
        gpu::Color m_ambientSky = { 0.38f, 0.44f, 0.55f, 1 }, m_ambientGround = { 0.2f, 0.19f, 0.17f, 1 };
        gpu::Color m_skyTop = { 0.25f, 0.5f, 0.9f, 1 }, m_skyHorizon = { 0.75f, 0.85f, 0.95f, 1 };
        gpu::Color m_fog = { 0.75f, 0.85f, 0.95f, 1 };
        float m_fogStart = 1e6f, m_fogEnd = 2e6f;
        float m_shadowStrength = 0.75f, m_shadowDistance = 40;
        bool m_lines = false, m_hasSky = false, m_hasGround = false, m_hasWater = false;
        float m_ground = 0, m_water = 0;
        gpu::Color m_waterColor = { 0.15f, 0.35f, 0.5f, 0.75f };
        Vec3 m_gravity = { 0, -9.8f, 0 };
        float m_time = 0;

        bool m_landOn = false, m_island = false, m_started = false;
        u32 m_landSquares = 0;       // 0: endless
        u32 m_landSeed = 0, m_reach = 0, m_regionsWaiting = 0;
        float m_landSpacing = 1, m_landHeight = 0, m_landRough = 0;
        GrowArray<Chunk> m_chunks;
        u16 m_table[32 * 32] = {};   // of regions, by the last bits of where they are; 0 is none
        GrowArray<Flat> m_flats;
        GrowArray<float> m_sculpt;   // by corner; empty until something is changed by hand
        GrowArray<u8> m_paint;       // colour and how much of it, likewise
        GrowArray<RegionEvent> m_regionEvents;
        u32 m_regionNext = 0;
        Vec3 m_focus[4], m_focusTo[4];
        Vec3 m_ears[4], m_earSides[4];   // where it is heard from, and which way is right
        u32 m_earCount = 0;
        u32 m_focusCount = 0;
        gpu::Color m_landColors[4] = { { 0.76f, 0.7f, 0.5f, 1 }, { 0.33f, 0.5f, 0.22f, 1 }, { 0.9f, 0.92f, 0.95f, 1 }, { 0.42f, 0.38f, 0.34f, 1 } };
        float m_landLow = 0, m_landHigh = 30;
        gpu::Mesh m_waterMesh;
        MeshData m_waterData;

        gpu::Field m_field;
        gpu::Mesh m_fieldMeshes[4];   // those that the GPU may still be drawing, and the latest
        u32 m_fieldLatest = 0;
        MeshData m_fieldData;
        float m_fieldMade = -1;

        GrowArray<Rigid> m_rigid;
        GrowArray<u32> m_rigidOrder, m_hitters;
        GrowArray<Box3> m_boxes;
        gpu::Color m_domeColors[3] = {};
        Vec3 m_domeSun;
        GrowArray<Touch> m_touches, m_touchesBefore;

        GrowArray<Item> m_items;
        gpu::View m_lastView;
        Vec2 m_lastAt, m_lastSize;
        SpaceStats m_stats;
        u32 m_version = 0;

        friend class World;
    };
}
