#pragma once

#include "assets/assets.h"
#include "audio/audio.h"
#include "base/grow_array.h"
#include "base/json_tree.h"
#include "base/math.h"
#include "canvas/canvas.h"
#include "scene/space.h"
#include "script/script_host.h"

#include <string>

namespace nx
{
    class World;

    // What a script holds. There is one per entity, shared by everyone who holds it, and it outlives
    // the entity: then id is 0 and nothing done to it has any effect.
    struct EntityRef
    {
        World* world = nullptr;  // nullptr once the world has gone
        u32 id = 0;
        int refs = 0;
        EntityRef* next = nullptr;      // of those in use, so the world can say goodbye
        EntityRef* previous = nullptr;

        void addRef() { refs++; }
        void release();
    };

    enum class Shape : u8 { Circle, Box };
    enum class Look : u8 { Image, Rect, Disc, Text };

    constexpr u32 kGroups = 16;
    constexpr u32 kAllGroups = 0xFFFF;

    // Components. Each kind is packed together with the entity it belongs to, so a system is a walk
    // along an array.
    struct Visual
    {
        u32 owner;
        Look look;
        gpu::Blend blend;
        bool flipX;
        u16 columns, rows, frame;
        Asset* asset;         // a texture or a font; held
        float width, height;  // of an image 0: its own size
        Color color;
        char* text;           // from memAlloc
    };

    struct Animation
    {
        u32 owner;
        u16 first, last;
        float fps, at;
        bool loop, done;
    };

    struct Body
    {
        u32 owner;
        Vec2 velocity;
        float spin;         // degrees a second
        float drag;         // part of the speed lost each second
        float gravity;      // of the world's
        float bounce;       // part of the speed kept on hitting something solid
        float mass;         // above 0: a rigid body, which pushes and is pushed, turns and stacks
        float friction;     // of a rigid body: 0 ice, 1 rubber
        bool solid;         // stopped by solid tiles and walls, without turning: for characters
        bool onGround, onCeiling;
        int onWall;          // -1 left, 1 right
    };

    struct Collider
    {
        u32 owner;
        Shape shape;
        u8 group;
        bool wall;          // doesn't move; solid and rigid bodies are stopped by it
        bool occluder;      // casts shadows from lights on the plane
        u16 hits;           // groups it wants to know about
        float halfWidth, halfHeight;  // a circle's radius is halfWidth
    };

    struct Lifetime
    {
        u32 owner;
        float left;
    };

    // How particles look. Shared by every emitter that uses it.
    struct ParticleStyle
    {
        char name[32];
        Asset* texture;
        gpu::Blend blend;
        int layer;
        float lifeMin, lifeMax;
        float speedMin, speedMax;
        float angle, spread;     // degrees; the spread is the whole width
        float sizeStart, sizeEnd;
        Color colorStart, colorEnd;
        float gravity, drag;
        float radius;            // of the area they start in
        float fadeIn;            // part of their life
        float spin;              // degrees a second, at most, in space
    };

    struct Particle
    {
        Vec2 position, velocity;
        float age, life;
    };

    struct Emitter
    {
        u32 owner;
        u32 style;
        float rate, owed;
    };

    // A light on the plane: what is within its radius is lit, fading to nothing at the edge, where nothing is in the way.
    struct Light2
    {
        u32 owner;
        Color color;
        float radius;
    };

    enum class Component : u8 { Visual, Animation, Body, Collider, Lifetime, Emitter, Light, Count };

    // A thing that can be made again and again, read from a file.
    struct Prefab
    {
        char name[32];
        u32 first, count;        // its parts: the first is the root
    };

    struct PrefabPart
    {
        i32 parent;              // another part of the prefab, or -1
        char name[24];
        Vec2 position, scale;
        float rotation;
        int layer;
        bool hidden;
        u32 components;          // a bit for each Component
        Visual visual;           // its asset is held by the prefab; its text belongs to the file
        Animation animation;
        Body body;
        Collider collider;
        Lifetime lifetime;
        Emitter emitter;
        u32 burst;               // particles at once
        char behaviours[2][40];
    };

    struct TileMap
    {
        u16* tiles;              // 0 is empty
        u32 width, height;
        float size;
        Asset* texture;
        u32 columns, rows;       // of the sheet
        int layer;
        Color colors[16];        // of tiles drawn without a texture
        u8 solid[64];            // by tile: 1 solid, 2 only from above
        // By tile: a terrain, which joins up with its neighbours. 0 is not one; otherwise one more than the first of its 16
        // frames in the sheet, which are chosen by the sides that meet the same tile: up 1, right 2, down 4, left 8. Drawn
        // in colours, its open sides are edged and its outer corners rounded instead.
        u16 terrain[64];
        Vec2 origin;
    };

    struct WorldStats
    {
        u32 entities = 0;
        u32 particles = 0;
        u32 pairsTested = 0;   // last step
        u32 contacts = 0;      // of rigid bodies
        u32 touching = 0;
        u32 drawn = 0;         // last draw
    };

    // Everything that lives in one screen: what moves, collides, is drawn and is scripted. Empty until
    // something is spawned. Destroying is put off to the end of the step, so nothing disappears while
    // it is being looked at.
    class World : NonCopyable
    {
    public:
        World(Assets& assets, ScriptHost& host, const char* module, u32 maxEntities, u32 maxParticles);
        ~World();

        // --- Entities ---

        // 0 if there is no room.
        u32 spawn(Vec2 position);
        u32 spawnPrefab(const char* name, Vec2 position);
        void destroyEntity(u32 id);
        bool alive(u32 id) const { return find(id) != nullptr; }
        u32 count() const { return m_live; }
        u32 countGroup(u32 group) const;
        u32 findNamed(const char* name) const;
        // Everything but what was loaded.
        void clear();

        // A reference is added for the caller. nullptr for an entity that isn't there.
        EntityRef* ref(u32 id);

        struct Entity
        {
            u32 generation;
            bool live, dying, visible, shown;
            u16 depth;
            u32 parent, child, next, previous;  // indices; kNone is none
            Vec2 position, scale;
            float rotation;
            float sine, cosine, turned;         // of the rotation they were made from
            float world[6];                     // x' = m0 x + m2 y + m4, as in Canvas
            int layer;
            u32 parts[static_cast<size_t>(Component::Count)];
            u32 parts3[6];                      // in space
            u32 behaviour;                      // first in its list
            EntityRef* ref;
            char name[24];
        };

        // Each that is there, with its id, in the order they were made: for looking at, not for changing.
        template <typename Fn>
        void eachEntity(Fn fn) const
        {
            for (u32 i = 0; i < m_entities.count(); i++)
            {
                if (m_entities[i].live && !m_entities[i].dying)
                {
                    fn(idOf(i), m_entities[i]);
                }
            }
        }

        // nullptr if it has gone or been destroyed. Valid until the next spawn.
        Entity* find(u32 id);
        const Entity* find(u32 id) const { return const_cast<World*>(this)->find(id); }

        void setParent(u32 id, u32 parent);
        u32 parentOf(u32 id) const;
        Vec2 worldPosition(u32 id) const;
        void setLayer(u32 id, int layer);

        // Each makes the component if the entity has none. nullptr for an entity that isn't there.
        Visual* visual(u32 id);
        Animation* animation(u32 id);
        Body* body(u32 id);
        Collider* collider(u32 id);
        Lifetime* lifetime(u32 id);
        // The same without making one.
        Visual* getVisual(u32 id) { return get(m_visuals, Component::Visual, id); }
        Animation* getAnimation(u32 id) { return get(m_animations, Component::Animation, id); }
        Body* getBody(u32 id) { return get(m_bodies, Component::Body, id); }
        Collider* getCollider(u32 id) { return get(m_colliders, Component::Collider, id); }
        Lifetime* getLifetime(u32 id) { return get(m_lifetimes, Component::Lifetime, id); }
        Light2* light(u32 id);
        Light2* getLight(u32 id) { return get(m_lights, Component::Light, id); }
        void removeLight(u32 id);

        // --- Lights on the plane (lights2d.cpp) ---
        // Lighting is on while the ambient light isn't white: everything drawn is darkened to it, and lights add to it.
        void setAmbient(const Color& color) { m_ambient = color; }
        Color ambient() const { return m_ambient; }
        // Layers above this one are drawn after the light, as they are: glows, sparks, what is shown over the world.
        void setLightLayer(int layer) { m_lightLayer = layer; }
        int lightLayer() const { return m_lightLayer; }
        bool lightingOn() const { return m_ambient.r < 0.999f || m_ambient.g < 0.999f || m_ambient.b < 0.999f; }

        // Takes a reference to the asset, letting go of the one before.
        void setLook(u32 id, Look look, Asset* asset);
        void setText(u32 id, const char* text);

        // --- Behaviours: script objects that are told what happens to their entity ---

        // Adds a reference to the object. False if it can't be one.
        bool addBehaviour(u32 id, asIScriptObject* object);
        // The first, or nullptr. No reference is added.
        asIScriptObject* behaviour(u32 id) const;

        // --- Files ---

        // What has been thrown out goes at once; emitters go on.
        void clearParticles();
        // Prefabs, particles, groups, maps and entities. On failure, takeError() says why.
        bool load(const char* path, const char* folder);
        // Why the last load or spawn failed, forgetting it.
        std::string takeError()
        {
            const std::string text = m_error;
            m_error[0] = 0;
            return text;
        }
        // Files that have been loaded, for noticing changes.
        bool changed() const;
        u32 groupNamed(const char* name) const;

        // --- Particles ---

        u32 styleNamed(const char* name) const;  // 0xFFFFFFFF if there is none
        const ParticleStyle& style(u32 index) const { return m_styles[index]; }

        bool burst(const char* style, Vec2 position, u32 count);

        // --- Space ---

        void setGravity(Vec2 gravity) { m_gravity = gravity; }
        Vec2 gravity() const { return m_gravity; }
        u32 nearest(Vec2 position, float radius, u32 groups, u32 except) const;
        // Calls back with each entity whose collider touches a circle.
        template <typename Fn>
        void within(Vec2 position, float radius, u32 groups, Fn fn) const
        {
            for (const Collider& c : m_colliders)
            {
                if ((groups & (1u << c.group)) && m_entities[c.owner].live && !m_entities[c.owner].dying && touchesCircle(c, position, radius))
                {
                    fn(idOf(c.owner));
                }
            }
        }
        u32 raycast(Vec2 from, Vec2 to, u32 groups, Vec2& hit) const;
        bool touching(u32 a, u32 b) const;
        // What the map that was loaded last has at a place: 0 outside.
        u32 tileAt(Vec2 position) const;
        // A way between two places around the solid tiles of the maps, on the grid of the last: the corners of it, ending
        // at `to`. Diagonally too, if allowed, but never across a corner. False if there is none, or no map.
        bool findPath(Vec2 from, Vec2 to, bool diagonal, GrowArray<Vec2>& way);
        // Whether a cell of the last map's grid is blocked by a solid tile in any map.
        bool blocked(int x, int y) const;
        bool blockedAt(Vec2 place) const;
        void setTileAt(Vec2 position, u32 tile);
        // Makes a tile of the last map a terrain, from a frame of its sheet; -1 makes it a tile like any other again.
        void setTerrain(u32 tile, int firstFrame);
        // Which sides of a terrain's tile meet more of it, up 1, right 2, down 4, left 8; and the frame it is drawn with, or
        // -1 where there is no tile. Of the last map.
        static u32 terrainJoins(const TileMap& map, u32 x, u32 y);
        int tileFrame(Vec2 position) const;
        Vec2 mapSize() const;

        // --- Rigid bodies ---

        // Pins two together at a place; the second may be 0, for the world. Softer ones give.
        bool join(u32 a, u32 b, Vec2 at, float softness);
        // At a place, or at the middle.
        void push(u32 id, Vec2 impulse, const Vec2* at);

        // --- Every step, and every frame ---

        // False if a script failed; the host has the error.
        bool update(float dt);

        void setCamera(Vec2 position, float zoom, float rotation);
        Vec2 toWorld(Vec2 screen) const;
        Vec2 toScreen(Vec2 world) const;
        // Entities in layers from first to last.
        void draw(Canvas& canvas, int first, int last);

        // Everything in three dimensions.
        Space& space() { return m_space; }

        const WorldStats& stats();

    private:
        friend struct EntityRef;
        friend class Space;

        static constexpr u32 kNone = 0xFFFFFFFF;
        static constexpr u32 kIndexBits = 16;
        static constexpr u32 kMaxClasses = 48;
        static constexpr u32 kMaxFiles = 4;
        static constexpr u32 kMaxMaps = 4;
        static constexpr u32 kFreeRefs = 64;

        // What a class of behaviour can be told.
        struct Class
        {
            asITypeInfo* type;
            asIScriptFunction* start;
            asIScriptFunction* update;
            asIScriptFunction* onHit;
            asIScriptFunction* onLeave;
            asIScriptFunction* onGone;
            int entityProperty;
        };

        struct Behaviour
        {
            asIScriptObject* object;  // nullptr: this slot is free
            u32 owner;                // index
            u32 ownerId;
            u32 next;                 // of the same entity, or the next free slot
            u16 type;
            bool started;
        };

        // A rigid body as the solver sees it: in world space, in radians.
        struct Rigid
        {
            u32 id;
            u32 body;       // index, or kNone for something that doesn't move
            u32 entity;
            Shape shape;
            Vec2 position, velocity;
            float angle, turning, c, s;
            float inverseMass, inverseInertia;
            float friction, bounce;
            float halfWidth, halfHeight;
            float minX, maxX, minY, maxY;
        };

        struct Contact
        {
            Vec2 position, normal;   // from a to b
            Vec2 fromA, fromB;
            float separation;
            float normalImpulse, tangentImpulse;
            float normalMass, tangentMass, bias;
            u32 feature;             // so a contact can be recognised next step
        };

        struct Touch
        {
            u64 key;
            u32 a, b;                // in m_rigid
            u32 count;
            float friction;
            Contact contacts[2];
        };

        struct Joint
        {
            u32 a, b;                // ids; b may be 0
            Vec2 localA, localB;     // b's is in the world if there is no b
            float softness;
            Vec2 impulse;
            u32 rigidA, rigidB;      // this step
            Vec2 fromA, fromB, bias;
            float m[4];              // the inverse of how much they resist
            bool active;
        };

        struct Box
        {
            float minX, maxX, minY, maxY;
        };

        struct Event
        {
            u32 target, other;  // ids
            bool began;
        };

        struct DrawItem
        {
            u64 key;
            u32 index;
            u8 kind;  // 0 visual, 1 particles, 2 map
        };

        struct Extent
        {
            float minX, maxX, minY, maxY;
            u32 collider;
        };

        u32 idOf(u32 index) const { return (m_entities[index].generation << kIndexBits) | index; }

        template <typename T>
        T* get(GrowArray<T>& parts, Component kind, u32 id)
        {
            const Entity* e = find(id);
            return e && e->parts[static_cast<size_t>(kind)] != kNone ? &parts[e->parts[static_cast<size_t>(kind)]] : nullptr;
        }
        template <typename T>
        T* getOrAdd(GrowArray<T>& parts, Component kind, u32 id, const T& fresh)
        {
            Entity* e = find(id);
            if (e == nullptr)
            {
                return nullptr;
            }
            u32& at = e->parts[static_cast<size_t>(kind)];
            if (at == kNone)
            {
                if (!parts.push(fresh))
                {
                    return nullptr;
                }
                at = parts.count() - 1;
                parts[at].owner = id & ((1u << kIndexBits) - 1);
            }
            return &parts[at];
        }
        template <typename T>
        void removePart(GrowArray<T>& parts, Component kind, u32 index);

        u32 spawnPart(const PrefabPart& part, Vec2 position);
        void unlink(u32 index);
        void markDying(u32 index);
        bool removeDead();
        void remove(u32 index);
        void freeVisual(Visual& visual);
        EntityRef* makeRef();

        int classOf(asITypeInfo* type);
        bool startBehaviours();
        bool updateBehaviours(float dt);
        bool tell(u32 entity, asIScriptFunction* Class::*method, u32 other);

        void move(float dt);
        void moveSolid(u32 bodyIndex, Vec2 by);
        template <typename Fn>
        void solids(const Box& box, u32 self, Fn fn) const;
        bool obstacle(const Box& box, u32 axis, bool positive, float bottomBefore, u32 self, float& edge) const;
        // physics.cpp
        void physics(float dt);
        void addRigid(const Collider& collider, u32 body);
        static u32 touch(const Rigid& a, const Rigid& b, Contact (&contacts)[2]);
        static u32 touchBoxes(const Rigid& a, const Rigid& b, Contact (&contacts)[2]);
        static u32 touchCircleBox(const Rigid& circle, const Rigid& box, Contact& contact);
        void prepareJoint(Joint& joint, float dt);
        void solveJoint(Joint& joint);
        void prepareTouch(Touch& touch, float dt);
        void solveTouch(Touch& touch);
        void buildWalls(const TileMap& map);
        void animate(float dt);
        void age(float dt);
        void emit(float dt);
        void moveParticles(float dt);
        void updateTransforms();
        void transform(u32 index);
        bool collide();
        bool wantsToKnow(u32 id, u32 other);

        void halfExtent(const Collider& c, float& x, float& y) const;
        bool touchesCircle(const Collider& c, Vec2 position, float radius) const;
        bool overlap(const Collider& a, const Collider& b) const;

        void buildDrawList();
        void drawVisual(Canvas& canvas, const Visual& v, const float (&view)[4]);
        void drawParticles(Canvas& canvas, u32 style);
        void drawMap(Canvas& canvas, const TileMap& map, const float (&view)[4]);
        void drawTerrainTile(Canvas& canvas, const TileMap& map, u32 x, u32 y, const Color& color, u32 joins);

        // prefab.cpp
        bool fail(const char* file, u32 line, const char* format, const char* what);
        bool readPart(JsonTree::Value json, PrefabPart& part, const char* file);
        bool readPrefab(JsonTree::Value json, const char* name, const char* file);
        bool addParts(JsonTree::Value json, i32 parent, u32 depth, const char* file);
        bool readStyle(JsonTree::Value json, const char* file);
        bool readMap(JsonTree::Value json, const char* file);
        bool readColor(JsonTree::Value json, Color& color, const char* file);
        bool readGroups(JsonTree::Value json, u32& mask, const char* file);
        Asset* readAsset(JsonTree::Value json, AssetType type, u32 size, const char* file);
        const Prefab* prefabNamed(const char* name) const;
        u32 random();
        float random(float low, float high);

        Assets& m_assets;
        ScriptHost& m_host;
        char m_module[32];
        u32 m_maxEntities, m_maxParticles;
        char m_error[256] = {};

        GrowArray<Entity> m_entities;
        u32 m_freeEntity = kNone;
        u32 m_live = 0;
        u32 m_dying = 0;
        GrowArray<u32> m_order;      // live entities, parents before children
        bool m_orderDirty = true;

        GrowArray<Visual> m_visuals;
        GrowArray<Animation> m_animations;
        GrowArray<Body> m_bodies;
        GrowArray<Collider> m_colliders;
        GrowArray<Lifetime> m_lifetimes;
        GrowArray<Emitter> m_emitters;
        GrowArray<Light2> m_lights;
        Color m_ambient = { 1, 1, 1, 1 };
        int m_lightLayer = 32767;
        Asset* m_lightMap = nullptr;            // a picture the size of the screen, made when first wanted
        GrowArray<Vec2> m_lightEdges;           // pairs: what is in the way of a light, found again each frame
        GrowArray<float> m_lightAngles;
        void drawLights(Canvas& canvas, const float (&view)[4]);
        void lightEdges(const float (&box)[4]);
        void releaseLightMap();
        GrowArray<Behaviour> m_behaviours;
        u32 m_freeBehaviour = kNone;
        u32 m_unstarted = 0;
        Class m_classes[kMaxClasses] = {};
        u32 m_classCount = 0;

        GrowArray<Prefab> m_prefabs;
        GrowArray<PrefabPart> m_parts;
        GrowArray<ParticleStyle> m_styles;
        GrowArray<Particle> m_particles[16];  // one for each style
        u32 m_particleCount = 0;
        TileMap m_maps[kMaxMaps] = {};
        u32 m_mapCount = 0;
        GrowArray<u32> m_pathCost, m_pathFrom, m_pathOpen, m_pathCells;   // reused by findPath
        char m_groups[kGroups][20] = {};
        char m_folder[96] = {};
        struct
        {
            JsonTree* json;
            char path[160];
            u64 time;
        } m_files[kMaxFiles] = {};
        u32 m_fileCount = 0;

        GrowArray<Rigid> m_rigid;
        GrowArray<u32> m_rigidOrder;
        GrowArray<Touch> m_touches, m_touchesBefore;
        GrowArray<Joint> m_joints;
        GrowArray<Box> m_walls;      // runs of solid tiles, for rigid bodies
        GrowArray<Extent> m_extents;
        GrowArray<u64> m_pairs, m_pairsBefore;
        GrowArray<Event> m_events;
        GrowArray<DrawItem> m_drawList;
        bool m_drawDirty = true;
    public:
        bool m_showShapes = false;   // colliders drawn over everything: grey walls, green bodies, blue characters, yellow areas
    private:

        Vec2 m_gravity;
        bool m_hasCamera = false;
        Vec2 m_cameraAt;
        float m_cameraZoom = 1, m_cameraTurn = 0;
        float m_viewWidth = 1280, m_viewHeight = 720;
        u32 m_random = 88172645u;
        EntityRef* m_refs = nullptr;
        EntityRef* m_freeRefs[kFreeRefs] = {};
        u32 m_freeRefCount = 0;
        WorldStats m_stats;
        Space m_space;
    };
}
