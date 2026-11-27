#pragma once

#include "assets/geometry.h"
#include "assets/import.h"

namespace nx
{
    constexpr u32 kModelDetails = 3;   // levels, the first the whole model
    constexpr u32 kModelBones = 128;

    // Some triangles of one material.
    struct ModelPart
    {
        u32 first[kModelDetails], count[kModelDetails];
        gpu::Color color;
        int texture;        // in the model, or -1
        bool twoSided;
        bool indoors;       // of a material whose name begins so: the sun doesn't reach it
        bool see;           // through: drawn after everything solid
        float glow;
    };

    struct ModelBone
    {
        int parent;
        Vec3 place;
        Quat turn;
        Vec3 size;
        Mat4 inverseBind;
        char name[24];
    };

    enum class ModelPath : u8 { Place, Turn, Size };

    // How one bone changes over time.
    struct ModelChannel
    {
        u32 bone;
        ModelPath path;
        bool stepped;
        u32 count;
        float* times;
        float* values;      // 3 or 4 for each time
    };

    struct ModelAnimation
    {
        char name[24];
        float length;
        u32 first, count;   // channels
        int root;           // the highest bone it moves from place to place, or -1
    };

    // Where one bone is from its parent.
    struct BonePose
    {
        Vec3 place;
        Quat turn;
        Vec3 size;
    };

    // Which bones move a vertex, and how much.
    struct ModelWeight
    {
        u8 bones[4];
        u8 weights[4];      // adding up to 255
    };

    struct ModelTexture
    {
        u8* pixels;         // until it is on the GPU
        u32 width, height;
        gpu::Texture texture;
    };

    // A model read from a glTF file. Never changed after loading, and shared by all who show it.
    struct ModelData : NonCopyable
    {
        ~ModelData();

        MeshData mesh;                       // all the levels, one after another
        GrowArray<ModelPart, MemTag::General> parts;
        GrowArray<ModelBone, MemTag::General> bones;
        GrowArray<ModelChannel, MemTag::General> channels;
        GrowArray<ModelAnimation, MemTag::General> animations;
        GrowArray<ModelWeight, MemTag::General> weights;  // empty: it doesn't bend
        GrowArray<ModelTexture, MemTag::General> textures;
        Vec3 middle;
        float radius = 0;
        Vec3 low, high;                      // a box around it, at rest
        gpu::Mesh gpuMesh;
        size_t bytes = 0;

        // Which triangles of the first level are in which part of the box, made when it is first needed. False if
        // there is no room.
        bool makeCells() const;
        void cellsOf(const Vec3& least, const Vec3& most, int (&from)[3], int (&to)[3]) const;
        mutable GrowArray<u32, MemTag::General> cellStarts;
        mutable GrowArray<u16, MemTag::General> cellTriangles;
        mutable u32 cellCount[3] = {};
        mutable float cellSize[3] = {};

        int findAnimation(const char* name) const;
        // The bones at a time, ready for the vertices. Room for kModelBones.
        void pose(int animation, float time, Mat4* out) const;
        // Between two animations: how much of the second.
        void pose(int from, float fromTime, int to, float toTime, float part, Mat4* out) const;
        // The pieces of those. Every bone at a time of an animation, or at rest for -1; room for kModelBones.
        void sample(int animation, float time, BonePose* out) const;
        // Moves `out` part of the way to `to`: where `only` isn't null, just the bones it marks.
        void mix(BonePose* out, const BonePose* to, float part, const bool* only = nullptr) const;
        // Where each bone is in the model's own space.
        void place(const BonePose* bones, Mat4* out) const;
        // From those, what the vertices are given.
        void finish(const BonePose* bones, Mat4* out) const;
        // Every vertex of the first level, moved by the bones.
        void bend(const Mat4* bones, gpu::MeshVertex* out) const;
    };

    // A .glb, or another kind that import.h turns into one, with the files it names read by `read`. nullptr,
    // with the reason logged, if it can't be used.
    // On a worker thread: nothing of it touches the GPU.
    ModelData* loadModel(const u8* file, size_t size, const char* name, ReadBeside read = nullptr, void* user = nullptr);

    // Moves it to the GPU. False if there is no room.
    bool uploadModel(ModelData& model, gpu::Device& device);
    void freeModel(ModelData* model, gpu::Device& device);
}
