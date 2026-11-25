#include "assets/model.h"
#include "assets/decode.h"
#include "base/log.h"

#include "third_party/cgltf/cgltf.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

namespace nx
{
    namespace
    {
        void* parserAlloc(void*, cgltf_size size)
        {
            return memAlloc(size, MemTag::General);
        }

        void parserFree(void*, void* memory)
        {
            memFree(memory);
        }

        struct Parsed : NonCopyable
        {
            cgltf_data* data = nullptr;
            ~Parsed() { cgltf_free(data); }
        };

        Vec3 vec3(const float (&v)[3])
        {
            return { v[0], v[1], v[2] };
        }

        Mat4 worldOf(const cgltf_node* node)
        {
            Mat4 m;
            cgltf_node_transform_world(node, m.m);
            return m;
        }

        // A node's own transform, taken apart if it is a matrix.
        void localOf(const cgltf_node& node, ModelBone& bone)
        {
            bone.place = node.has_translation ? vec3(node.translation) : Vec3();
            bone.turn = node.has_rotation ? Quat(node.rotation[0], node.rotation[1], node.rotation[2], node.rotation[3]).normalized() : Quat();
            bone.size = node.has_scale ? vec3(node.scale) : Vec3(1, 1, 1);
            if (node.has_matrix)
            {
                const float* m = node.matrix;
                bone.place = { m[12], m[13], m[14] };
                const Vec3 x(m[0], m[1], m[2]), y(m[4], m[5], m[6]), z(m[8], m[9], m[10]);
                bone.size = { x.length(), y.length(), z.length() };
                const Vec3 a = x.normalized(), b = y.normalized(), c = z.normalized();
                const float trace = a.x + b.y + c.z;
                Quat q;
                if (trace > 0)
                {
                    const float s = sqrtf(trace + 1) * 2;
                    q = { (b.z - c.y) / s, (c.x - a.z) / s, (a.y - b.x) / s, s / 4 };
                }
                else if (a.x > b.y && a.x > c.z)
                {
                    const float s = sqrtf(1 + a.x - b.y - c.z) * 2;
                    q = { s / 4, (b.x + a.y) / s, (c.x + a.z) / s, (b.z - c.y) / s };
                }
                else if (b.y > c.z)
                {
                    const float s = sqrtf(1 + b.y - a.x - c.z) * 2;
                    q = { (b.x + a.y) / s, s / 4, (c.y + b.z) / s, (c.x - a.z) / s };
                }
                else
                {
                    const float s = sqrtf(1 + c.z - a.x - b.y) * 2;
                    q = { (c.x + a.z) / s, (c.y + b.z) / s, s / 4, (a.y - b.x) / s };
                }
                bone.turn = q.normalized();
            }
        }

        bool fail(const char* name, const char* why)
        {
            NX_ERROR("%s: %s", name, why);
            return false;
        }

        // Each pixel of the four it was, in the same memory.
        void halve(u8* pixels, u32& width, u32& height)
        {
            const u32 w = width / 2, h = height / 2;
            for (u32 y = 0; y < h; y++)
            {
                for (u32 x = 0; x < w; x++)
                {
                    const u8* a = pixels + (static_cast<size_t>(y) * 2 * width + static_cast<size_t>(x) * 2) * 4;
                    const u8* b = a + static_cast<size_t>(width) * 4;
                    u8* out = pixels + (static_cast<size_t>(y) * w + x) * 4;
                    for (u32 k = 0; k < 4; k++)
                    {
                        out[k] = static_cast<u8>((a[k] + a[k + 4] + b[k] + b[k + 4] + 2) / 4);
                    }
                }
            }
            width = w;
            height = h;
        }

        int readTexture(ModelData& model, const cgltf_texture_view& view, const cgltf_image** images, u32& imageCount, const char* name)
        {
            const cgltf_image* image = view.texture ? view.texture->image : nullptr;
            if (image == nullptr || image->buffer_view == nullptr)
            {
                return -1;
            }
            for (u32 i = 0; i < imageCount; i++)
            {
                if (images[i] == image)
                {
                    return static_cast<int>(i);
                }
            }
            const u8* data = cgltf_buffer_view_data(image->buffer_view);
            u32 width = 0, height = 0;
            ModelTexture t = {};
            if (imageCount < 16 && data && imageSize(data, image->buffer_view->size, width, height) && width <= 4096 && height <= 4096)
            {
                t.pixels = decodeImage(data, image->buffer_view->size, width, height);
                // Those made for other engines are often larger than is drawn here.
                while (t.pixels && !gpu::validTextureSize(width, height) && width > 1 && height > 1)
                {
                    halve(t.pixels, width, height);
                }
                t.width = width;
                t.height = height;
            }
            if (t.pixels == nullptr)
            {
                fail(name, "a picture in it can't be read");
                return -1;
            }
            images[imageCount++] = image;
            model.textures.push(t);
            return static_cast<int>(model.textures.count() - 1);
        }

        bool readPrimitive(ModelData& model, const cgltf_data& data, const cgltf_node& node, const cgltf_primitive& primitive, const cgltf_image** images, u32& imageCount, bool animated, const char* name)
        {
            if (primitive.type != cgltf_primitive_type_triangles)
            {
                return true;  // points and lines aren't drawn
            }
            const cgltf_accessor* places = nullptr;
            const cgltf_accessor* normals = nullptr;
            const cgltf_accessor* uvs = nullptr;
            const cgltf_accessor* colors = nullptr;
            const cgltf_accessor* joints = nullptr;
            const cgltf_accessor* weights = nullptr;
            for (cgltf_size i = 0; i < primitive.attributes_count; i++)
            {
                const cgltf_attribute& a = primitive.attributes[i];
                const bool first = a.index == 0;
                places = a.type == cgltf_attribute_type_position ? a.data : places;
                normals = a.type == cgltf_attribute_type_normal ? a.data : normals;
                uvs = a.type == cgltf_attribute_type_texcoord && first ? a.data : uvs;
                colors = a.type == cgltf_attribute_type_color && first ? a.data : colors;
                joints = a.type == cgltf_attribute_type_joints && first ? a.data : joints;
                weights = a.type == cgltf_attribute_type_weights && first ? a.data : weights;
            }
            if (places == nullptr || places->count == 0)
            {
                return true;
            }
            const u32 first = model.mesh.vertices.count();
            if (first + places->count > 65536)
            {
                return fail(name, "more than 65536 vertices");
            }
            ModelPart part = {};
            part.color = { 1, 1, 1, 1 };
            part.texture = -1;
            if (const cgltf_material* m = primitive.material)
            {
                if (m->has_pbr_metallic_roughness)
                {
                    const float* c = m->pbr_metallic_roughness.base_color_factor;
                    part.color = { c[0], c[1], c[2], c[3] };
                    part.texture = readTexture(model, m->pbr_metallic_roughness.base_color_texture, images, imageCount, name);
                }
                part.twoSided = m->double_sided;
                part.see = m->alpha_mode == cgltf_alpha_mode_blend;
                part.glow = m->emissive_factor[0] + m->emissive_factor[1] + m->emissive_factor[2] > 0.1f ? 1.0f : 0.0f;
            }

            // Without animations, everything is moved to where it belongs now. With them, a node that has
            // no skin is a bone that its vertices follow.
            const Mat4 world = animated ? Mat4() : worldOf(&node);
            const bool skinned = animated && node.skin && joints && weights;
            for (cgltf_size i = 0; i < places->count; i++)
            {
                float p[3] = {}, n[3] = { 0, 1, 0 }, uv[2] = {}, c[4] = { 1, 1, 1, 1 };
                cgltf_accessor_read_float(places, i, p, 3);
                if (normals)
                {
                    cgltf_accessor_read_float(normals, i, n, 3);
                }
                if (uvs)
                {
                    cgltf_accessor_read_float(uvs, i, uv, 2);
                }
                if (colors)
                {
                    cgltf_accessor_read_float(colors, i, c, 4);
                }
                model.mesh.vertex(world.transformPoint({ p[0], p[1], p[2] }), world.transformDirection({ n[0], n[1], n[2] }).normalized(), { c[0], c[1], c[2], c[3] }, uv[0], uv[1]);
                if (animated)
                {
                    ModelWeight w = {};
                    w.bones[0] = static_cast<u8>(cgltf_node_index(&data, &node));
                    w.weights[0] = 255;
                    if (skinned)
                    {
                        cgltf_uint j[4] = {};
                        float f[4] = {};
                        cgltf_accessor_read_uint(joints, i, j, 4);
                        cgltf_accessor_read_float(weights, i, f, 4);
                        const float sum = f[0] + f[1] + f[2] + f[3];
                        int left = 255;
                        for (u32 k = 0; k < 4; k++)
                        {
                            const bool known = j[k] < node.skin->joints_count;
                            w.bones[k] = known ? static_cast<u8>(cgltf_node_index(&data, node.skin->joints[j[k]])) : 0;
                            const int value = sum > 0 && known ? static_cast<int>(f[k] / sum * 255 + 0.5f) : 0;
                            w.weights[k] = static_cast<u8>(value > left ? left : value < 0 ? 0 : value);
                            left -= w.weights[k];
                        }
                        w.weights[0] = static_cast<u8>(w.weights[0] + left);
                    }
                    model.weights.push(w);
                }
            }
            part.first[0] = model.mesh.indices.count();
            const cgltf_size count = primitive.indices ? primitive.indices->count : places->count;
            for (cgltf_size i = 0; i + 2 < count; i += 3)
            {
                u32 t[3];
                for (u32 k = 0; k < 3; k++)
                {
                    const cgltf_size index = primitive.indices ? cgltf_accessor_read_index(primitive.indices, i + k) : i + k;
                    if (index >= places->count)
                    {
                        return fail(name, "a triangle uses a vertex that isn't there");
                    }
                    t[k] = first + static_cast<u32>(index);
                }
                model.mesh.triangle(t[0], t[1], t[2]);
            }
            part.count[0] = model.mesh.indices.count() - part.first[0];
            if (part.count[0])
            {
                model.parts.push(part);
            }
            return true;
        }

        bool readAnimations(ModelData& model, const cgltf_data& data)
        {
            for (cgltf_size i = 0; i < data.animations_count; i++)
            {
                const cgltf_animation& from = data.animations[i];
                ModelAnimation a = {};
                snprintf(a.name, sizeof(a.name), "%s", from.name ? from.name : "");
                a.first = model.channels.count();
                for (cgltf_size k = 0; k < from.channels_count; k++)
                {
                    const cgltf_animation_channel& channel = from.channels[k];
                    const cgltf_accessor* times = channel.sampler ? channel.sampler->input : nullptr;
                    const cgltf_accessor* values = channel.sampler ? channel.sampler->output : nullptr;
                    const bool known = channel.target_path == cgltf_animation_path_type_translation || channel.target_path == cgltf_animation_path_type_rotation || channel.target_path == cgltf_animation_path_type_scale;
                    if (channel.target_node == nullptr || !known || times == nullptr || values == nullptr || times->count == 0)
                    {
                        continue;
                    }
                    const bool cubic = channel.sampler->interpolation == cgltf_interpolation_type_cubic_spline;
                    if (values->count < times->count * (cubic ? 3 : 1))
                    {
                        continue;
                    }
                    ModelChannel c = {};
                    c.bone = static_cast<u32>(cgltf_node_index(&data, channel.target_node));
                    c.path = channel.target_path == cgltf_animation_path_type_translation ? ModelPath::Place : channel.target_path == cgltf_animation_path_type_rotation ? ModelPath::Turn : ModelPath::Size;
                    c.stepped = channel.sampler->interpolation == cgltf_interpolation_type_step;
                    c.count = static_cast<u32>(times->count);
                    const u32 parts = c.path == ModelPath::Turn ? 4 : 3;
                    c.times = static_cast<float*>(memAlloc(c.count * sizeof(float), MemTag::General));
                    c.values = static_cast<float*>(memAlloc(c.count * parts * sizeof(float), MemTag::General));
                    if (!c.times || !c.values || !model.channels.push(c))
                    {
                        memFree(c.times);
                        memFree(c.values);
                        return false;
                    }
                    for (u32 t = 0; t < c.count; t++)
                    {
                        cgltf_accessor_read_float(times, t, &c.times[t], 1);
                        // A spline's own value, between its two slopes.
                        cgltf_accessor_read_float(values, cubic ? t * 3 + 1 : t, &c.values[t * parts], parts);
                    }
                    a.length = fmaxf(a.length, c.times[c.count - 1]);
                }
                a.count = model.channels.count() - a.first;
                model.animations.push(a);
            }
            return true;
        }

        // Simpler levels of what doesn't bend, part by part so that each keeps its material.
        void addDetails(ModelData& model)
        {
            const u32 partCount = model.parts.count();
            for (u32 level = 1; level < kModelDetails; level++)
            {
                for (u32 i = 0; i < partCount; i++)
                {
                    ModelPart& part = model.parts[i];
                    part.first[level] = part.first[level - 1];
                    part.count[level] = part.count[level - 1];
                    if (model.weights.count() || part.count[0] < 120)
                    {
                        continue;
                    }
                    MeshData simple;
                    simplify(model.mesh.vertices.begin(), model.mesh.vertices.count(), model.mesh.indices.begin() + part.first[0], part.count[0], level == 1 ? 14 : 6, simple);
                    // Only the vertices it uses.
                    if (simple.indices.count() == 0 || simple.indices.count() >= part.count[level - 1] * 3 / 4)
                    {
                        continue;
                    }
                    GrowArray<int, MemTag::General> becomes;
                    if (!becomes.reserve(simple.vertices.count() ? simple.vertices.count() : 1))
                    {
                        continue;
                    }
                    for (u32 v = 0; v < simple.vertices.count(); v++)
                    {
                        becomes.push(-1);
                    }
                    const u32 first = model.mesh.indices.count();
                    bool ok = true;
                    for (u32 k = 0; k < simple.indices.count() && ok; k++)
                    {
                        int& b = becomes[simple.indices[k]];
                        if (b < 0)
                        {
                            b = static_cast<int>(model.mesh.vertices.count());
                            ok = b < 65536 && model.mesh.vertices.push(simple.vertices[simple.indices[k]]);
                        }
                        ok = ok && model.mesh.indices.push(static_cast<u16>(b));
                    }
                    if (ok)
                    {
                        part.first[level] = first;
                        part.count[level] = simple.indices.count();
                    }
                }
            }
        }

        bool readAll(ModelData& model, const cgltf_data& data, const char* name)
        {
            const bool animated = data.animations_count > 0 || data.skins_count > 0;
            if (animated)
            {
                if (data.nodes_count > kModelBones)
                {
                    return fail(name, "an animated model can have up to 128 nodes");
                }
                for (cgltf_size i = 0; i < data.nodes_count; i++)
                {
                    const cgltf_node& node = data.nodes[i];
                    ModelBone b = {};
                    b.parent = node.parent ? static_cast<int>(cgltf_node_index(&data, node.parent)) : -1;
                    localOf(node, b);
                    b.inverseBind = Mat4();
                    snprintf(b.name, sizeof(b.name), "%s", node.name ? node.name : "");
                    model.bones.push(b);
                }
                for (cgltf_size i = 0; i < data.skins_count; i++)
                {
                    const cgltf_skin& skin = data.skins[i];
                    for (cgltf_size j = 0; j < skin.joints_count; j++)
                    {
                        Mat4 m;
                        if (skin.inverse_bind_matrices)
                        {
                            cgltf_accessor_read_float(skin.inverse_bind_matrices, j, m.m, 16);
                        }
                        model.bones[static_cast<u32>(cgltf_node_index(&data, skin.joints[j]))].inverseBind = m;
                    }
                }
                if (!readAnimations(model, data))
                {
                    return fail(name, "no memory");
                }
            }
            const cgltf_image* images[16] = {};
            u32 imageCount = 0;
            bool missingNormals = false;
            for (cgltf_size i = 0; i < data.nodes_count; i++)
            {
                const cgltf_node& node = data.nodes[i];
                for (cgltf_size p = 0; node.mesh && p < node.mesh->primitives_count; p++)
                {
                    const cgltf_primitive& primitive = node.mesh->primitives[p];
                    if (!readPrimitive(model, data, node, primitive, images, imageCount, animated, name))
                    {
                        return false;
                    }
                    bool has = false;
                    for (cgltf_size a = 0; a < primitive.attributes_count; a++)
                    {
                        has = has || primitive.attributes[a].type == cgltf_attribute_type_normal;
                    }
                    missingNormals = missingNormals || !has;
                }
            }
            if (model.parts.count() == 0)
            {
                return fail(name, "there are no triangles in it");
            }
            if (missingNormals)
            {
                model.mesh.computeNormals();
            }
            const auto box = [&](const MeshData& mesh, u32 count)
            {
                model.low = { 1e9f, 1e9f, 1e9f };
                model.high = { -1e9f, -1e9f, -1e9f };
                for (u32 i = 0; i < count; i++)
                {
                    const gpu::MeshVertex& v = mesh.vertices[i];
                    model.low = { fminf(model.low.x, v.x), fminf(model.low.y, v.y), fminf(model.low.z, v.z) };
                    model.high = { fmaxf(model.high.x, v.x), fmaxf(model.high.y, v.y), fmaxf(model.high.z, v.z) };
                }
            };
            box(model.mesh, model.mesh.vertices.count());
            addDetails(model);

            // Where it is when nothing has moved it.
            model.mesh.bounds(model.middle, model.radius);
            if (animated)
            {
                Mat4 bones[kModelBones];
                model.pose(-1, 0, bones);
                MeshData moved;
                if (moved.vertices.reserve(model.weights.count()))
                {
                    moved.vertices.resize(model.weights.count());
                    model.bend(bones, moved.vertices.begin());
                    moved.bounds(model.middle, model.radius);
                    box(moved, moved.vertices.count());
                    model.radius *= 1.3f;  // arms and legs reach further than they rest
                }
            }
            return true;
        }
    }

    ModelData::~ModelData()
    {
        for (ModelChannel& c : channels)
        {
            memFree(c.times);
            memFree(c.values);
        }
        for (ModelTexture& t : textures)
        {
            freeImage(t.pixels);
        }
    }

    ModelData* loadModel(const u8* file, size_t size, const char* name, ReadBeside read, void* user)
    {
        std::vector<u8> made;
        if (modelKind(name, file, size) != ModelKind::Glb)
        {
            std::string why;
            if (!importModel(name, file, size, read, user, made, why))
            {
                fail(name, why.c_str());
                return nullptr;
            }
            file = made.data();
            size = made.size();
        }
        cgltf_options options = {};
        options.memory.alloc_func = parserAlloc;
        options.memory.free_func = parserFree;
        Parsed parsed;
        if (cgltf_parse(&options, file, size, &parsed.data) != cgltf_result_success)
        {
            fail(name, "not a glTF model");
            return nullptr;
        }
        // Only what is in the file itself: nothing is read from wherever it might point.
        for (cgltf_size i = 0; i < parsed.data->buffers_count; i++)
        {
            const char* uri = parsed.data->buffers[i].uri;
            if (uri && strncmp(uri, "data:", 5) != 0)
            {
                fail(name, "it needs another file; save it as .glb");
                return nullptr;
            }
        }
        if (cgltf_load_buffers(&options, parsed.data, nullptr) != cgltf_result_success || cgltf_validate(parsed.data) != cgltf_result_success)
        {
            fail(name, "the model is damaged");
            return nullptr;
        }
        ModelData* model = create<ModelData>(MemTag::General);
        if (model && !readAll(*model, *parsed.data, name))
        {
            destroy(model);
            return nullptr;
        }
        return model;
    }

    bool uploadModel(ModelData& model, gpu::Device& device)
    {
        // One that bends is drawn from a copy, so its own vertices needn't be on the GPU.
        if (model.weights.count() == 0)
        {
            model.gpuMesh = device.createMesh(model.mesh.vertices.begin(), model.mesh.vertices.count(), model.mesh.indices.begin(), model.mesh.indices.count(), false);
            if (!model.gpuMesh)
            {
                return false;
            }
        }
        model.bytes = model.mesh.vertices.count() * sizeof(gpu::MeshVertex) + model.mesh.indices.count() * sizeof(u16);
        for (ModelTexture& t : model.textures)
        {
            gpu::TextureData data = gpu::rgbaTexture(t.pixels, t.width, t.height);
            data.mips = true;
            t.texture = device.createTexture(data);
            model.bytes += device.textureInfo(t.texture).bytes;
            freeImage(t.pixels);
            t.pixels = nullptr;
        }
        return true;
    }

    void freeModel(ModelData* model, gpu::Device& device)
    {
        if (model)
        {
            device.destroyMesh(model->gpuMesh);
            for (ModelTexture& t : model->textures)
            {
                device.destroyTexture(t.texture);
            }
            destroy(model);
        }
    }

    // --- Movement ---

    int ModelData::findAnimation(const char* name) const
    {
        for (u32 i = 0; i < animations.count(); i++)
        {
            if (strcmp(animations[i].name, name) == 0)
            {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    namespace
    {
        struct Local
        {
            Vec3 place;
            Quat turn;
            Vec3 size;
        };

        void sample(const ModelData& model, int animation, float time, Local* out)
        {
            for (u32 i = 0; i < model.bones.count(); i++)
            {
                out[i] = { model.bones[i].place, model.bones[i].turn, model.bones[i].size };
            }
            if (animation < 0 || static_cast<u32>(animation) >= model.animations.count())
            {
                return;
            }
            const ModelAnimation& a = model.animations[static_cast<u32>(animation)];
            for (u32 i = 0; i < a.count; i++)
            {
                const ModelChannel& c = model.channels[a.first + i];
                if (c.bone >= model.bones.count())
                {
                    continue;
                }
                u32 low = 0, high = c.count - 1;
                while (low < high)
                {
                    const u32 middle = (low + high + 1) / 2;
                    if (c.times[middle] <= time)
                    {
                        low = middle;
                    }
                    else
                    {
                        high = middle - 1;
                    }
                }
                const u32 next = low + 1 < c.count ? low + 1 : low;
                const float span = c.times[next] - c.times[low];
                float part = span > 0 && !c.stepped ? (time - c.times[low]) / span : 0;
                part = part < 0 ? 0 : part > 1 ? 1 : part;
                if (c.path == ModelPath::Turn)
                {
                    const float* x = c.values + low * 4;
                    const float* y = c.values + next * 4;
                    out[c.bone].turn = Quat::slerp({ x[0], x[1], x[2], x[3] }, { y[0], y[1], y[2], y[3] }, part);
                }
                else
                {
                    const float* x = c.values + low * 3;
                    const float* y = c.values + next * 3;
                    const Vec3 v(x[0] + (y[0] - x[0]) * part, x[1] + (y[1] - x[1]) * part, x[2] + (y[2] - x[2]) * part);
                    (c.path == ModelPath::Place ? out[c.bone].place : out[c.bone].size) = v;
                }
            }
        }

        void chain(const ModelData& model, const Local* locals, Mat4* out)
        {
            // A parent may come after its child in the file, so each is worked out when first needed.
            bool done[kModelBones] = {};
            Mat4 worlds[kModelBones];
            for (u32 i = 0; i < model.bones.count(); i++)
            {
                u32 waiting[kModelBones];
                u32 count = 0;
                for (int at = static_cast<int>(i); at >= 0 && !done[at] && count < kModelBones; at = model.bones[static_cast<u32>(at)].parent)
                {
                    waiting[count++] = static_cast<u32>(at);
                }
                while (count)
                {
                    const u32 b = waiting[--count];
                    const Mat4 local = compose(locals[b].place, locals[b].turn, locals[b].size);
                    const int parent = model.bones[b].parent;
                    worlds[b] = parent >= 0 ? worlds[parent] * local : local;
                    done[b] = true;
                }
            }
            for (u32 i = 0; i < model.bones.count(); i++)
            {
                out[i] = worlds[i] * model.bones[i].inverseBind;
            }
        }
    }

    void ModelData::pose(int animation, float time, Mat4* out) const
    {
        Local locals[kModelBones];
        sample(*this, animation, time, locals);
        chain(*this, locals, out);
    }

    void ModelData::pose(int from, float fromTime, int to, float toTime, float part, Mat4* out) const
    {
        Local a[kModelBones], b[kModelBones];
        sample(*this, from, fromTime, a);
        sample(*this, to, toTime, b);
        for (u32 i = 0; i < bones.count(); i++)
        {
            a[i].place = a[i].place + (b[i].place - a[i].place) * part;
            a[i].turn = Quat::slerp(a[i].turn, b[i].turn, part);
            a[i].size = a[i].size + (b[i].size - a[i].size) * part;
        }
        chain(*this, a, out);
    }

    void ModelData::bend(const Mat4* matrices, gpu::MeshVertex* out) const
    {
        for (u32 i = 0; i < weights.count(); i++)
        {
            const gpu::MeshVertex& v = mesh.vertices[i];
            const ModelWeight& w = weights[i];
            gpu::MeshVertex& o = out[i];
            o = v;
            const Vec3 p(v.x, v.y, v.z), n(v.nx, v.ny, v.nz);
            if (w.weights[0] == 255)
            {
                const Mat4& m = matrices[w.bones[0]];
                const Vec3 a = m.transformPoint(p), b = m.transformDirection(n);
                o.x = a.x;
                o.y = a.y;
                o.z = a.z;
                o.nx = b.x;
                o.ny = b.y;
                o.nz = b.z;
                continue;
            }
            Vec3 a, b;
            for (u32 k = 0; k < 4; k++)
            {
                if (w.weights[k])
                {
                    const float part = static_cast<float>(w.weights[k]) / 255.0f;
                    const Mat4& m = matrices[w.bones[k]];
                    a = a + m.transformPoint(p) * part;
                    b = b + m.transformDirection(n) * part;
                }
            }
            o.x = a.x;
            o.y = a.y;
            o.z = a.z;
            o.nx = b.x;
            o.ny = b.y;
            o.nz = b.z;
        }
    }
}
