#pragma once

#include "base/math.h"
#include "base/types.h"

#include <string>
#include <vector>

// Other kinds of 3D file, turned into a .glb, which is the one kind that models are read from. The cook tool
// does it beforehand; a file that wasn't cooked is turned as it is loaded.
namespace nx
{
    // Gets a file that a model names: a path from the folder it is in, never out of it.
    using ReadBeside = bool (*)(const char* name, std::vector<u8>& out, void* user);

    enum class ModelKind : u8 { None, Glb, Gltf, Obj, Stl, Ply, Fbx };

    // By what is in it, whatever it is called, where that can be told; otherwise by its name.
    ModelKind modelKind(const char* name, const u8* data, size_t size);
    // By its name only, for what a folder has.
    ModelKind modelKindByName(const char* name);
    // Whether this build reads them: the original Xbox leaves FBX to the cook tool.
    bool importsFbx();

    // False, with why, if it can't be used.
    bool importModel(const char* name, const u8* data, size_t size, ReadBeside read, void* user, std::vector<u8>& glb, std::string& why);

    // --- For the importers ---

    struct ImportVertex
    {
        float place[3], normal[3], uv[2];
        u8 color[4];
        u8 bones[4];
        u8 weights[4];
    };

    struct ImportMaterial
    {
        float color[4] = { 1, 1, 1, 1 };
        float glow = 0;
        bool twoSided = false, see = false;
        int image = -1;
    };

    struct ImportPart
    {
        int material = -1;
        bool normals = true, colors = false, weights = false;
        std::vector<ImportVertex> vertices;
        std::vector<u32> indices;

        // Each vertex that is the same as one before is that one.
        void corner(const ImportVertex& vertex);
        // Smooth over vertices in the same place.
        void makeNormals();

    private:
        void grow();
        std::vector<u32> m_table;
    };

    struct ImportNode
    {
        std::string name;
        int parent = -1;
        Vec3 place;
        Quat turn;
        Vec3 size = { 1, 1, 1 };
        std::vector<ImportPart> parts;
        bool joint = false;
        Mat4 inverseBind;
    };

    struct ImportChannel
    {
        u32 node;
        u32 path;                 // 0 place, 1 turn, 2 size
        std::vector<float> times, values;
    };

    struct ImportAnimation
    {
        std::string name;
        std::vector<ImportChannel> channels;
    };

    struct ImportScene
    {
        std::vector<ImportMaterial> materials;
        std::vector<std::vector<u8>> images;
        std::vector<ImportNode> nodes;
        std::vector<ImportAnimation> animations;

        // The same picture is kept once. -1 if it isn't there.
        int image(const char* name, ReadBeside read, void* user);
        int image(std::vector<u8>&& bytes, const std::string& key);

    private:
        std::vector<std::string> m_names;
    };

    bool importFbx(const u8* data, size_t size, ReadBeside read, void* user, ImportScene& out, std::string& why);
}
