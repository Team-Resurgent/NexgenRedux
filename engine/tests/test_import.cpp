#include "assets/import.h"
#include "assets/model.h"
#include "tests/rig.h"
#include "tests/test.h"

#include <map>
#include <math.h>
#include <string.h>
#include <string>

using namespace nx;

namespace
{
    using Files = std::map<std::string, std::string>;

    bool readFrom(const char* name, std::vector<u8>& out, void* user)
    {
        const Files& files = *static_cast<Files*>(user);
        const auto f = files.find(name);
        if (f == files.end())
        {
            return false;
        }
        out.assign(f->second.begin(), f->second.end());
        return true;
    }

    struct Loaded
    {
        ModelData* model;
        gpu::Device* device = gpu::createNullDevice(64, 64);

        Loaded(const char* name, const std::string& file, Files files = {}) { model = loadModel(reinterpret_cast<const u8*>(file.data()), file.size(), name, readFrom, &files); }

        ~Loaded()
        {
            if (model)
            {
                freeModel(model, *device);
            }
            destroy(device);
        }

        u32 triangles() const
        {
            u32 sum = 0;
            for (u32 i = 0; model && i < model->parts.count(); i++)
            {
                sum += model->parts[i].count[0] / 3;
            }
            return sum;
        }
    };

    // Not one of them may be read past, whatever is cut off or changed.
    void damage(const char* name, const std::string& file, const Files& files)
    {
        u32 loaded = 0;
        for (size_t i = 0; i < file.size(); i += 1 + file.size() / 150)
        {
            loaded += Loaded(name, file.substr(0, i), files).model != nullptr;
            std::string bad = file;
            bad[i] = static_cast<char>(bad[i] ^ (1 << (i % 7)));
            loaded += Loaded(name, bad, files).model != nullptr;
            bad[i] = '9';
            loaded += Loaded(name, bad, files).model != nullptr;
        }
        NX_EXPECT(loaded < 450);
    }

    std::string bytes(u32 v, bool big = false)
    {
        std::string out;
        for (u32 i = 0; i < 4; i++)
        {
            out += static_cast<char>(v >> ((big ? 3 - i : i) * 8));
        }
        return out;
    }

    std::string bytes(float f, bool big = false)
    {
        u32 v;
        memcpy(&v, &f, 4);
        return bytes(v, big);
    }

    // Two by two pixels, as a BMP.
    std::string picture()
    {
        std::string out = "BM" + bytes(70u) + bytes(0u) + bytes(54u) + bytes(40u) + bytes(2u) + bytes(2u) + std::string("\1\0\x18\0", 4) + bytes(0u) + bytes(16u) + bytes(0u) + bytes(0u) + bytes(0u) + bytes(0u);
        for (u32 i = 0; i < 2; i++)
        {
            out += std::string("\xFF\0\0\0\xFF\0\0\0", 8);
        }
        return out;
    }
}

NX_TEST(objModels)
{
    const std::string obj =
        "# a box's two sides, and a triangle\r\nmtllib a b.mtl\nv 0 0 0\nv 1 0 0\nv 1 1e0 0\nv 0 +1. 0\nv 0 0 1 1 0 0\nvt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\nvn 0 0 1\n"
        "usemtl red one\nf 1/1/1 2/2/1 3/3/1 4/4/1\nusemtl glass\nf -5//1 -1//1 -2//1\nf 1 2 3 # no normals\nusemtl red one\nf 1/1/1 2/2/1 3/3/1\n";
    const std::string mtl = "newmtl red one\nKd 1 0 0\nmap_Kd -s 1 1 1 tex\\a picture.bmp\nnewmtl glass\nKd 0 1 0\nd 0.5\nKe 1 1 1\n";
    const Files files = { { "a b.mtl", mtl }, { "a picture.bmp", picture() } };
    Loaded l("x/thing.OBJ", obj, files);
    NX_EXPECT(l.model && l.model->parts.count() == 2 && l.triangles() == 5);
    if (l.model)
    {
        const ModelPart &red = l.model->parts[0], &glass = l.model->parts[1];
        NX_EXPECT(red.color.r == 1 && red.color.g == 0 && red.texture == 0 && l.model->textures[0].width == 2 && !red.see && red.glow == 0);
        NX_EXPECT(glass.color.g == 1 && fabsf(glass.color.a - 0.5f) < 0.01f && glass.see && glass.glow == 1 && glass.texture < 0);
        // The same corner is one vertex.
        u32 used = 0;
        for (u32 i = red.first[0]; i < red.first[0] + red.count[0]; i++)
        {
            used = l.model->mesh.indices[i] > used ? l.model->mesh.indices[i] : used;
        }
        NX_EXPECT(used == 3 && fabsf(l.model->mesh.vertices[2].v - 0) < 0.001f);
    }
    // Without what it names, it still shows.
    Loaded alone("thing.obj", obj);
    NX_EXPECT(alone.model && alone.triangles() == 5);
    NX_EXPECT(!Loaded("thing.obj", "v 0 0 0\nf 1 2 3\n").model && !Loaded("thing.obj", "").model && !Loaded("thing.txt", obj).model);
    damage("thing.obj", obj, files);
}

NX_TEST(stlModels)
{
    std::string ascii = "solid it\nfacet normal 0 0 1\nouter loop\nvertex 0 0 5\nvertex 1 0 5\nvertex 0 2 5\nendloop\nendfacet\nendsolid\n";
    Loaded a("a.stl", ascii);
    NX_EXPECT(a.model && a.triangles() == 1 && a.model->mesh.vertices[2].y == 5 && a.model->mesh.vertices[2].z == -2 && a.model->parts[0].texture < 0);
    std::string binary(80, 's');
    binary.replace(0, 5, "solid");
    binary += bytes(2u);
    for (u32 i = 0; i < 2; i++)
    {
        const float f[12] = { 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, i ? 0.0f : 1.0f };
        for (const float v : f)
        {
            binary += bytes(v);
        }
        binary += std::string(2, 0);
    }
    Loaded b("b.stl", binary);
    NX_EXPECT(b.model && b.triangles() == 2);
    damage("a.stl", ascii, {});
    damage("b.stl", binary, {});
}

NX_TEST(plyModels)
{
    const std::string head = "element vertex 4\nproperty float x\nproperty float y\nproperty float z\nproperty uchar red\nproperty uchar green\nproperty uchar blue\nelement face 1\nproperty list uchar int vertex_indices\nend_header\n";
    const std::string ascii = "ply\nformat ascii 1.0\ncomment of no matter\n" + head + "0 0 0 255 0 0\n1 0 0 0 255 0\n1 1 0 0 0 255\n0 1 0 9 9 9\n4 0 1 2 3\n";
    Loaded a("a.ply", ascii);
    NX_EXPECT(a.model && a.triangles() == 2 && a.model->mesh.vertices[0].r == 255 && fabsf(a.model->mesh.vertices[0].nz - 1) < 0.01f);
    for (const bool big : { false, true })
    {
        std::string binary = std::string("ply\nformat binary_") + (big ? "big" : "little") + "_endian 1.0\n" + head;
        const float places[4][3] = { { 0, 0, 0 }, { 1, 0, 0 }, { 1, 1, 0 }, { 0, 1, 0 } };
        for (const auto& p : places)
        {
            binary += bytes(p[0], big) + bytes(p[1], big) + bytes(p[2], big) + std::string("\1\2\3");
        }
        binary += "\4";
        for (u32 i = 0; i < 4; i++)
        {
            binary += bytes(i, big);
        }
        Loaded b("b.ply", binary);
        NX_EXPECT(b.model && b.triangles() == 2 && b.model->mesh.vertices[2].x == 1 && b.model->mesh.vertices[2].y == 1);
        damage("b.ply", binary, {});
    }
    damage("a.ply", ascii, {});
}

NX_TEST(gltfModelsInSeveralFiles)
{
    std::string bin;
    for (const float v : { 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f })
    {
        bin += bytes(v);
    }
    const auto text = [](const char* uri, const char* image)
    {
        return std::string("{\"asset\":{\"version\":\"2.0\"},\"buffers\":[{\"byteLength\":36,\"uri\":\"") + uri + "\"}],\"bufferViews\":[{\"buffer\":0,\"byteLength\":36}],"
            "\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\",\"min\":[0,0,0],\"max\":[1,1,0]}],\"images\":[{\"uri\":\"" + image + "\"}],\"textures\":[{\"source\":0}],"
            "\"materials\":[{\"pbrMetallicRoughness\":{\"baseColorTexture\":{\"index\":0}}}],\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0},\"material\":0}]}],\"nodes\":[{\"mesh\":0}],\"scenes\":[{\"nodes\":[0]}]}";
    };
    const Files files = { { "a file.bin", bin }, { "tex/p.bmp", picture() } };
    const std::string gltf = text("a%20file.bin", "tex/p.bmp");
    Loaded a("a.gltf", gltf, files);
    NX_EXPECT(a.model && a.triangles() == 1 && a.model->parts[0].texture == 0 && a.model->mesh.vertices[1].x == 1);
    Loaded noPicture("a.gltf", gltf, { { "a file.bin", bin } });
    NX_EXPECT(noPicture.model && noPicture.model->parts[0].texture < 0);
    NX_EXPECT(!Loaded("a.gltf", gltf).model && !Loaded("a.gltf", text("../a file.bin", ""), { { "../a file.bin", bin } }).model && !Loaded("a.gltf", text("C:/a.bin", ""), { { "C:/a.bin", bin } }).model);
    damage("a.gltf", gltf, files);
}

NX_TEST(fbxModels)
{
    if (!importsFbx())
    {
        return;
    }
    const std::string fbx =
        "; FBX 7.4.0 project file\nFBXHeaderExtension:  {\n\tFBXHeaderVersion: 1003\n\tFBXVersion: 7400\n}\nGlobalSettings:  {\n\tVersion: 1000\n\tProperties70:  {\n"
        "\t\tP: \"UpAxis\", \"int\", \"Integer\", \"\",2\n\t\tP: \"UpAxisSign\", \"int\", \"Integer\", \"\",1\n\t\tP: \"FrontAxis\", \"int\", \"Integer\", \"\",1\n\t\tP: \"FrontAxisSign\", \"int\", \"Integer\", \"\",-1\n"
        "\t\tP: \"CoordAxis\", \"int\", \"Integer\", \"\",0\n\t\tP: \"CoordAxisSign\", \"int\", \"Integer\", \"\",1\n\t\tP: \"UnitScaleFactor\", \"double\", \"Number\", \"\",1\n\t}\n}\n"
        "Objects:  {\n\tGeometry: 1, \"Geometry::g\", \"Mesh\" {\n\t\tVertices: *12 {\n\t\t\ta: 0,0,0,100,0,0,100,0,200,0,0,200\n\t\t}\n\t\tPolygonVertexIndex: *4 {\n\t\t\ta: 0,1,2,-4\n\t\t}\n"
        "\t\tLayerElementMaterial: 0 {\n\t\t\tVersion: 101\n\t\t\tName: \"\"\n\t\t\tMappingInformationType: \"AllSame\"\n\t\t\tReferenceInformationType: \"IndexToDirect\"\n\t\t\tMaterials: *1 {\n\t\t\t\ta: 0\n\t\t\t}\n\t\t}\n"
        "\t\tLayer: 0 {\n\t\t\tVersion: 100\n\t\t\tLayerElement:  {\n\t\t\t\tType: \"LayerElementMaterial\"\n\t\t\t\tTypedIndex: 0\n\t\t\t}\n\t\t}\n\t}\n"
        "\tModel: 2, \"Model::it\", \"Mesh\" {\n\t\tVersion: 232\n\t\tProperties70:  {\n\t\t\tP: \"Lcl Translation\", \"Lcl Translation\", \"\", \"A\",100,0,0\n\t\t}\n\t}\n"
        "\tMaterial: 3, \"Material::red\", \"\" {\n\t\tVersion: 102\n\t\tShadingModel: \"lambert\"\n\t\tProperties70:  {\n\t\t\tP: \"DiffuseColor\", \"Color\", \"\", \"A\",1,0,0\n\t\t}\n\t}\n}\n"
        "Connections:  {\n\tC: \"OO\",2,0\n\tC: \"OO\",1,2\n\tC: \"OO\",3,2\n}\n";
    Loaded l("a.fbx", fbx);
    NX_EXPECT(l.model && l.triangles() == 2);
    if (l.model)
    {
        // In metres, with y up, and where its node is.
        float left = 1e9f, right = -1e9f, low = 1e9f, high = -1e9f;
        for (const gpu::MeshVertex& v : l.model->mesh.vertices)
        {
            left = fminf(left, v.x);
            right = fmaxf(right, v.x);
            low = fminf(low, v.y);
            high = fmaxf(high, v.y);
        }
        NX_EXPECT(fabsf(left - 1) < 0.01f && fabsf(right - 2) < 0.01f && fabsf(low) < 0.01f && fabsf(high - 2) < 0.01f);
        NX_EXPECT(l.model->parts[0].color.r > 0.9f && l.model->parts[0].color.g < 0.1f && l.model->bones.count() == 0);
    }
    damage("a.fbx", fbx, {});
}
