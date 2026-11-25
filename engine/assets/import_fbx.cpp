#if defined(NX_FBX)

#include "assets/import.h"
#include "base/memory.h"

#include "third_party/ufbx/ufbx.h"

#include <math.h>
#include <string.h>

namespace nx
{
    namespace
    {
        void* take(void*, size_t size) { return memAlloc(size, MemTag::General); }
        void give(void*, void* memory, size_t) { memFree(memory); }

        ufbx_allocator_opts ours()
        {
            ufbx_allocator_opts o;
            memset(&o, 0, sizeof(o));
            o.allocator.alloc_fn = take;
            o.allocator.free_fn = give;
            o.memory_limit = 1024u * 1024 * 1024;
            return o;
        }

        float f(ufbx_real v) { return v > -1e30 && v < 1e30 ? static_cast<float>(v) : 0.0f; }

        u8 byte(ufbx_real v) { return static_cast<u8>((v < 0 ? 0 : v > 1 ? 1 : v) * 255 + 0.5); }

        std::string text(const ufbx_string& s) { return std::string(s.data, s.length); }

        Mat4 matrix(const ufbx_matrix& m)
        {
            Mat4 out;
            const ufbx_vec3* columns[4] = { &m.cols[0], &m.cols[1], &m.cols[2], &m.cols[3] };
            for (u32 c = 0; c < 4; c++)
            {
                out.m[c * 4 + 0] = f(columns[c]->x);
                out.m[c * 4 + 1] = f(columns[c]->y);
                out.m[c * 4 + 2] = f(columns[c]->z);
                out.m[c * 4 + 3] = c == 3 ? 1.0f : 0.0f;
            }
            return out;
        }

        int readMaterial(const ufbx_material* m, ReadBeside read, void* user, ImportScene& out)
        {
            ImportMaterial to;
            const ufbx_material_map& base = m->pbr.base_color.has_value || m->pbr.base_color.texture ? m->pbr.base_color : m->fbx.diffuse_color;
            to.color[0] = f(base.value_vec3.x);
            to.color[1] = f(base.value_vec3.y);
            to.color[2] = f(base.value_vec3.z);
            const ufbx_texture* t = base.texture_enabled || base.texture ? base.texture : nullptr;
            if (t && t->file_textures.count)
            {
                t = t->file_textures.data[0];
                if (t->content.size)
                {
                    const u8* bytes = static_cast<const u8*>(t->content.data);
                    to.image = out.image(std::vector<u8>(bytes, bytes + t->content.size), text(t->filename) + "!" + std::to_string(t->file_index));
                }
                for (const ufbx_string* name : { &t->relative_filename, &t->filename, &t->absolute_filename })
                {
                    if (to.image < 0 && name->length)
                    {
                        to.image = out.image(text(*name).c_str(), read, user);
                    }
                }
                if (to.image >= 0)
                {
                    to.color[0] = to.color[1] = to.color[2] = 1;
                }
            }
            // "Opacity" means several things in what makes these, and is often left at 0.
            const float opacity = m->pbr.opacity.has_value ? f(m->pbr.opacity.value_vec3.x) : 1;
            if (opacity > 0.01f && opacity < 0.99f)
            {
                to.color[3] = opacity;
                to.see = true;
            }
            const ufbx_vec3 e = m->pbr.emission_color.value_vec3;
            to.glow = m->pbr.emission_factor.value_real > 0.05 && e.x + e.y + e.z > 0.1 && !m->pbr.emission_color.texture ? 1.0f : 0.0f;
            to.twoSided = m->features.double_sided.enabled;
            out.materials.push_back(to);
            return static_cast<int>(out.materials.size()) - 1;
        }

        struct Scenes
        {
            ufbx_scene* scene = nullptr;
            ~Scenes() { ufbx_free_scene(scene); }
        };
    }

    bool importFbx(const u8* data, size_t size, ReadBeside read, void* user, ImportScene& out, std::string& why)
    {
        ufbx_load_opts opts;
        memset(&opts, 0, sizeof(opts));
        opts.temp_allocator = ours();
        opts.result_allocator = ours();
        opts.target_axes = ufbx_axes_right_handed_y_up;
        opts.target_unit_meters = 1;
        opts.space_conversion = UFBX_SPACE_CONVERSION_MODIFY_GEOMETRY;
        opts.geometry_transform_handling = UFBX_GEOMETRY_TRANSFORM_HANDLING_MODIFY_GEOMETRY;
        opts.inherit_mode_handling = UFBX_INHERIT_MODE_HANDLING_COMPENSATE;
        opts.generate_missing_normals = true;
        opts.normalize_normals = true;
        opts.file_format = UFBX_FILE_FORMAT_FBX;
        ufbx_error error;
        Scenes held;
        held.scene = ufbx_load_memory(data, size, &opts, &error);
        const ufbx_scene* scene = held.scene;
        if (scene == nullptr)
        {
            why = "not an FBX file that can be read: " + text(error.description);
            return false;
        }

        // Which nodes matter: those with triangles, bones of skins, those that move, and what they hang from.
        std::vector<bool> keep(scene->nodes.count, false), joint(scene->nodes.count, false);
        bool skins = false;
        const auto keepUp = [&](const ufbx_node* n)
        {
            for (; n && !keep[n->typed_id]; n = n->parent)
            {
                keep[n->typed_id] = true;
            }
        };
        for (size_t i = 0; i < scene->nodes.count; i++)
        {
            const ufbx_node* n = scene->nodes.data[i];
            if (n->mesh == nullptr || n->mesh->num_triangles == 0 || !n->visible)
            {
                continue;
            }
            keepUp(n);
            if (n->mesh->skin_deformers.count)
            {
                const ufbx_skin_deformer* skin = n->mesh->skin_deformers.data[0];
                for (size_t k = 0; k < skin->clusters.count; k++)
                {
                    if (const ufbx_node* bone = skin->clusters.data[k]->bone_node)
                    {
                        keepUp(bone);
                        joint[bone->typed_id] = true;
                        skins = true;
                    }
                }
            }
        }

        std::vector<ufbx_baked_anim*> baked;
        std::vector<std::string> names;
        struct Free
        {
            std::vector<ufbx_baked_anim*>& all;
            ~Free()
            {
                for (ufbx_baked_anim* b : all)
                {
                    ufbx_free_baked_anim(b);
                }
            }
        } frees = { baked };
        for (size_t i = 0; i < scene->anim_stacks.count && i < 64; i++)
        {
            ufbx_bake_opts bake;
            memset(&bake, 0, sizeof(bake));
            bake.temp_allocator = ours();
            bake.result_allocator = ours();
            bake.trim_start_time = true;
            bake.key_reduction_enabled = true;
            bake.key_reduction_rotation = true;
            ufbx_baked_anim* b = ufbx_bake_anim(scene, scene->anim_stacks.data[i]->anim, &bake, &error);
            if (b == nullptr)
            {
                continue;
            }
            baked.push_back(b);
            names.push_back(text(scene->anim_stacks.data[i]->name));
        }

        std::vector<int> number(scene->nodes.count, -1);
        for (size_t i = 0; i < scene->nodes.count; i++)
        {
            // Parents come first.
            const ufbx_node* n = scene->nodes.data[i];
            if (!keep[n->typed_id])
            {
                continue;
            }
            ImportNode to;
            to.name = text(n->name);
            to.parent = n->parent ? number[n->parent->typed_id] : -1;
            const ufbx_transform& t = n->local_transform;
            to.place = { f(t.translation.x), f(t.translation.y), f(t.translation.z) };
            to.turn = { f(t.rotation.x), f(t.rotation.y), f(t.rotation.z), f(t.rotation.w) };
            to.size = { f(t.scale.x), f(t.scale.y), f(t.scale.z) };
            to.joint = joint[n->typed_id];
            number[n->typed_id] = static_cast<int>(out.nodes.size());
            out.nodes.push_back(static_cast<ImportNode&&>(to));
        }
        if (skins && out.nodes.size() > 255)
        {
            why = "it has more than 255 bones";
            return false;
        }

        std::vector<int> materials(scene->materials.count, -1);
        std::vector<bool> bound(out.nodes.size(), false);
        for (size_t i = 0; i < scene->nodes.count; i++)
        {
            const ufbx_node* n = scene->nodes.data[i];
            const ufbx_mesh* mesh = n->mesh;
            if (number[n->typed_id] < 0 || mesh == nullptr || mesh->num_triangles == 0 || !n->visible)
            {
                continue;
            }
            // All the meshes that bones move are put in the one place where the bones were bound to them, as a
            // bone has only one binding here.
            const ufbx_skin_deformer* skin = mesh->skin_deformers.count ? mesh->skin_deformers.data[0] : nullptr;
            ufbx_matrix toBind = ufbx_identity_matrix;
            bool haveBind = false;
            for (size_t k = 0; skin && k < skin->clusters.count; k++)
            {
                const ufbx_skin_cluster* c = skin->clusters.data[k];
                if (c->bone_node == nullptr)
                {
                    continue;
                }
                if (!haveBind)
                {
                    toBind = ufbx_matrix_mul(&c->bind_to_world, &c->geometry_to_bone);
                    haveBind = true;
                }
                const size_t bone = static_cast<size_t>(number[c->bone_node->typed_id]);
                if (!bound[bone])
                {
                    bound[bone] = true;
                    const ufbx_matrix back = ufbx_matrix_invert(&c->bind_to_world);
                    out.nodes[bone].inverseBind = matrix(back);
                }
            }
            skin = haveBind ? skin : nullptr;
            const ufbx_matrix forNormals = ufbx_matrix_for_normals(&toBind);
            std::vector<u32> corners(mesh->max_face_triangles * 3);
            for (size_t p = 0; p < mesh->material_parts.count; p++)
            {
                const ufbx_mesh_part& from = mesh->material_parts.data[p];
                if (from.num_triangles == 0)
                {
                    continue;
                }
                ImportPart part;
                const ufbx_material* m = from.index < n->materials.count ? n->materials.data[from.index] : nullptr;
                if (m)
                {
                    int& known = materials[m->typed_id];
                    known = known < 0 ? readMaterial(m, read, user, out) : known;
                    part.material = known;
                }
                part.colors = mesh->vertex_color.exists;
                part.weights = skin != nullptr;
                for (size_t k = 0; k < from.face_indices.count; k++)
                {
                    const u32 count = ufbx_triangulate_face(corners.data(), corners.size(), mesh, mesh->faces.data[from.face_indices.data[k]]);
                    for (u32 t = 0; t < count * 3; t++)
                    {
                        const u32 at = corners[t];
                        ImportVertex v;
                        memset(&v, 0, sizeof(v));
                        v.color[0] = v.color[1] = v.color[2] = v.color[3] = 255;
                        ufbx_vec3 place = ufbx_get_vertex_vec3(&mesh->vertex_position, at);
                        ufbx_vec3 normal = mesh->vertex_normal.exists ? ufbx_get_vertex_vec3(&mesh->vertex_normal, at) : ufbx_vec3{ { { 0, 1, 0 } } };
                        if (skin)
                        {
                            place = ufbx_transform_position(&toBind, place);
                            normal = ufbx_transform_direction(&forNormals, normal);
                            const double length = sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
                            normal.x = length > 0 ? normal.x / length : 0;
                            normal.y = length > 0 ? normal.y / length : 1;
                            normal.z = length > 0 ? normal.z / length : 0;
                        }
                        v.place[0] = f(place.x);
                        v.place[1] = f(place.y);
                        v.place[2] = f(place.z);
                        v.normal[0] = f(normal.x);
                        v.normal[1] = f(normal.y);
                        v.normal[2] = f(normal.z);
                        if (mesh->vertex_uv.exists)
                        {
                            const ufbx_vec2 uv = ufbx_get_vertex_vec2(&mesh->vertex_uv, at);
                            v.uv[0] = f(uv.x);
                            v.uv[1] = 1 - f(uv.y);
                        }
                        if (mesh->vertex_color.exists)
                        {
                            const ufbx_vec4 c = ufbx_get_vertex_vec4(&mesh->vertex_color, at);
                            v.color[0] = byte(c.x);
                            v.color[1] = byte(c.y);
                            v.color[2] = byte(c.z);
                            v.color[3] = byte(c.w);
                        }
                        if (skin)
                        {
                            // The four that weigh most come first.
                            const u32 vertex = mesh->vertex_indices.data[at];
                            const ufbx_skin_vertex sv = vertex < skin->vertices.count ? skin->vertices.data[vertex] : ufbx_skin_vertex();
                            double sum = 0;
                            u32 used = 0;
                            double weight[4] = {};
                            for (u32 w = 0; w < sv.num_weights && used < 4; w++)
                            {
                                const ufbx_skin_weight& sw = skin->weights.data[sv.weight_begin + w];
                                const ufbx_node* bone = skin->clusters.data[sw.cluster_index]->bone_node;
                                if (bone && sw.weight > 0)
                                {
                                    v.bones[used] = static_cast<u8>(number[bone->typed_id]);
                                    weight[used++] = sw.weight;
                                    sum += sw.weight;
                                }
                            }
                            int left = 255;
                            for (u32 w = 0; w < used; w++)
                            {
                                int share = static_cast<int>(weight[w] / sum * 255 + 0.5);
                                share = share > left ? left : share;
                                v.weights[w] = static_cast<u8>(share);
                                left -= share;
                            }
                            if (used == 0)
                            {
                                // One that nothing moves stays with the first bone.
                                v.bones[0] = static_cast<u8>(number[skin->clusters.data[0]->bone_node ? skin->clusters.data[0]->bone_node->typed_id : n->typed_id]);
                            }
                            v.weights[0] = static_cast<u8>(v.weights[0] + left);
                        }
                        part.corner(v);
                    }
                }
                out.nodes[static_cast<size_t>(number[n->typed_id])].parts.push_back(static_cast<ImportPart&&>(part));
            }
        }

        for (size_t i = 0; i < baked.size(); i++)
        {
            ImportAnimation a;
            a.name = names[i];
            bool moves = false;
            for (size_t k = 0; k < baked[i]->nodes.count; k++)
            {
                const ufbx_baked_node& n = baked[i]->nodes.data[k];
                if (n.typed_id >= number.size() || number[n.typed_id] < 0)
                {
                    continue;
                }
                const u32 node = static_cast<u32>(number[n.typed_id]);
                moves = moves || !n.constant_translation || !n.constant_rotation || !n.constant_scale;
                const auto three = [&](const ufbx_baked_vec3_list& keys, u32 path, bool still)
                {
                    ImportChannel c = { node, path, {}, {} };
                    for (size_t t = 0; t < keys.count && (t == 0 || !still); t++)
                    {
                        c.times.push_back(static_cast<float>(keys.data[t].time));
                        c.values.push_back(f(keys.data[t].value.x));
                        c.values.push_back(f(keys.data[t].value.y));
                        c.values.push_back(f(keys.data[t].value.z));
                    }
                    if (!c.times.empty())
                    {
                        a.channels.push_back(c);
                    }
                };
                three(n.translation_keys, 0, n.constant_translation);
                three(n.scale_keys, 2, n.constant_scale);
                ImportChannel c = { node, 1, {}, {} };
                for (size_t t = 0; t < n.rotation_keys.count && (t == 0 || !n.constant_rotation); t++)
                {
                    const ufbx_quat& q = n.rotation_keys.data[t].value;
                    c.times.push_back(static_cast<float>(n.rotation_keys.data[t].time));
                    for (const ufbx_real part : { q.x, q.y, q.z, q.w })
                    {
                        c.values.push_back(f(part));
                    }
                }
                if (!c.times.empty())
                {
                    a.channels.push_back(c);
                }
            }
            if (moves && !a.channels.empty())
            {
                out.animations.push_back(a);
            }
        }
        return true;
    }
}

#endif
