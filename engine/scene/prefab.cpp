#include "scene/world.h"
#include "base/log.h"

#include <angelscript.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Reading prefabs, particles, maps and whole scenes. A name that means nothing is an error with its
// line, so a slip of the keyboard is found at once.
namespace nx
{
    using Json = JsonTree::Value;

    namespace
    {
        u32 bit(Component c)
        {
            return 1u << static_cast<u32>(c);
        }

        Vec2 pair(Json json, Vec2 otherwise)
        {
            if (json.isNumber())
            {
                return { json.number(), json.number() };
            }
            return json.isArray() ? Vec2(json[0u].number(otherwise.x), json[1u].number(otherwise.y)) : otherwise;
        }

        int hex(char c)
        {
            return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
        }

        bool readBlend(Json json, gpu::Blend& out)
        {
            const char* text = json.text();
            out = strcmp(text, "add") == 0 ? gpu::Blend::Additive : strcmp(text, "none") == 0 ? gpu::Blend::None : gpu::Blend::Alpha;
            return strcmp(text, "add") == 0 || strcmp(text, "none") == 0 || strcmp(text, "alpha") == 0;
        }
    }

    bool World::fail(const char* file, u32 line, const char* format, const char* what)
    {
        char text[160];
        snprintf(text, sizeof(text), format, what);
        snprintf(m_error, sizeof(m_error), "%s:%u: %s", file, static_cast<unsigned>(line), text);
        return false;
    }

    u32 World::groupNamed(const char* name) const
    {
        for (u32 i = 0; i < kGroups; i++)
        {
            if (m_groups[i][0] && strcmp(m_groups[i], name) == 0)
            {
                return i;
            }
        }
        return kNone;
    }

    bool World::readGroups(Json json, u32& mask, const char* file)
    {
        mask = 0;
        const auto one = [&](Json value)
        {
            const u32 group = value.isNumber() ? static_cast<u32>(value.number()) : groupNamed(value.text());
            if (group >= kGroups)
            {
                return fail(file, value.line(), "no group called %s", value.text("that"));
            }
            mask |= 1u << group;
            return true;
        };
        if (!json.isArray())
        {
            return one(json);
        }
        for (Json v = json.first(); v.exists(); v = v.next())
        {
            if (!one(v))
            {
                return false;
            }
        }
        return true;
    }

    bool World::readColor(Json json, Color& color, const char* file)
    {
        if (json.isArray())
        {
            color = { json[0u].number(), json[1u].number(), json[2u].number(), json[3u].number(1) };
            return true;
        }
        const char* text = json.text();
        const size_t length = strlen(text);
        if (text[0] != '#' || (length != 7 && length != 9))
        {
            return fail(file, json.line(), "%s isn't a colour: use \"#rrggbb\", \"#rrggbbaa\" or [r, g, b, a]", text);
        }
        float parts[4] = { 1, 1, 1, 1 };
        for (size_t i = 0; i < (length - 1) / 2; i++)
        {
            const int high = hex(text[1 + i * 2]), low = hex(text[2 + i * 2]);
            if (high < 0 || low < 0)
            {
                return fail(file, json.line(), "%s isn't a colour", text);
            }
            parts[i] = static_cast<float>(high * 16 + low) / 255.0f;
        }
        color = { parts[0], parts[1], parts[2], parts[3] };
        return true;
    }

    // Held for whoever asked. nullptr for a bad name; a missing file is a placeholder, as elsewhere.
    Asset* World::readAsset(Json json, AssetType type, u32 size, const char* file)
    {
        const char* name = json.text();
        char path[192];
        if (strncmp(name, "global:", 7) == 0)
        {
            snprintf(path, sizeof(path), "global/%s", name + 7);
        }
        else if (strncmp(name, "asset:", 6) == 0)
        {
            snprintf(path, sizeof(path), "%s/asset/%s", m_folder, name + 6);
        }
        else
        {
            fail(file, json.line(), "%s: asset names start with asset: or global:", name);
            return nullptr;
        }
        return type == AssetType::Font ? m_assets.font(path, size) : m_assets.texture(path);
    }

    const Prefab* World::prefabNamed(const char* name) const
    {
        // The last of that name, so a later file can replace one.
        for (u32 i = m_prefabs.count(); i-- > 0;)
        {
            if (strcmp(m_prefabs[i].name, name) == 0)
            {
                return &m_prefabs[i];
            }
        }
        return nullptr;
    }

    bool World::readPart(Json json, PrefabPart& p, const char* file)
    {
        if (!json.isObject())
        {
            return fail(file, json.line(), "%s", "expected { }");
        }
        const auto look = [&](Look kind)
        {
            p.components |= bit(Component::Visual);
            p.visual.look = kind;
        };
        u32 fontSize = 24;
        if (json["fontSize"].isNumber())
        {
            fontSize = static_cast<u32>(json["fontSize"].number());
        }
        for (Json v = json.first(); v.exists(); v = v.next())
        {
            const char* key = v.key();
            const auto is = [&](const char* name) { return strcmp(key, name) == 0; };
            if (is("base") || is("prefab") || is("children") || is("fontSize"))
            {
                continue;
            }
            else if (is("name"))
            {
                snprintf(p.name, sizeof(p.name), "%s", v.text());
            }
            else if (is("position"))
            {
                p.position = pair(v, {});
            }
            else if (is("rotation"))
            {
                p.rotation = v.number();
            }
            else if (is("scale"))
            {
                p.scale = pair(v, { 1, 1 });
            }
            else if (is("layer"))
            {
                p.layer = static_cast<int>(v.number());
            }
            else if (is("hidden"))
            {
                p.hidden = v.flag();
            }
            else if (is("image") || is("font"))
            {
                Asset* asset = readAsset(v, is("font") ? AssetType::Font : AssetType::Texture, fontSize, file);
                if (asset == nullptr)
                {
                    return false;
                }
                if (p.visual.asset)
                {
                    m_assets.release(p.visual.asset);
                }
                p.visual.asset = asset;
                look(is("font") ? Look::Text : Look::Image);
            }
            else if (is("text"))
            {
                // The file's own copy: it stays for as long as the world does.
                p.visual.text = const_cast<char*>(v.text());
                look(Look::Text);
            }
            else if (is("rect"))
            {
                const Vec2 size = pair(v, { 16, 16 });
                p.visual.width = size.x;
                p.visual.height = size.y;
                look(Look::Rect);
            }
            else if (is("disc"))
            {
                p.visual.width = p.visual.height = v.number() * 2;
                look(Look::Disc);
            }
            else if (is("size"))
            {
                const Vec2 size = pair(v, {});
                p.visual.width = size.x;
                p.visual.height = size.y;
            }
            else if (is("sheet"))
            {
                const Vec2 sheet = pair(v, { 1, 1 });
                p.visual.columns = static_cast<u16>(sheet.x < 1 ? 1 : sheet.x);
                p.visual.rows = static_cast<u16>(sheet.y < 1 ? 1 : sheet.y);
            }
            else if (is("frame"))
            {
                p.visual.frame = static_cast<u16>(v.number());
            }
            else if (is("flip"))
            {
                p.visual.flipX = v.flag();
            }
            else if (is("blend"))
            {
                if (!readBlend(v, p.visual.blend))
                {
                    return fail(file, v.line(), "%s isn't a blend: use alpha, add or none", v.text());
                }
            }
            else if (is("color"))
            {
                if (!readColor(v, p.visual.color, file))
                {
                    return false;
                }
            }
            else if (is("play"))
            {
                p.components |= bit(Component::Animation);
                p.animation.first = static_cast<u16>(v[0u].number());
                p.animation.last = static_cast<u16>(v[1u].number());
                p.animation.fps = v[2u].number(10);
                p.animation.last = p.animation.last < p.animation.first ? p.animation.first : p.animation.last;
            }
            else if (is("loop"))
            {
                p.animation.loop = v.flag(true);
            }
            else if (is("velocity") || is("spin") || is("drag") || is("gravity") || is("bounce") || is("solid") || is("mass") || is("friction"))
            {
                p.components |= bit(Component::Body);
                Body& b = p.body;
                b.velocity = is("velocity") ? pair(v, {}) : b.velocity;
                b.spin = is("spin") ? v.number() : b.spin;
                b.drag = is("drag") ? v.number() : b.drag;
                b.gravity = is("gravity") ? v.number() : b.gravity;
                b.bounce = is("bounce") ? v.number() : b.bounce;
                b.solid = is("solid") ? v.flag() : b.solid;
                b.mass = is("mass") ? v.number() : b.mass;
                b.friction = is("friction") ? v.number() : b.friction;
            }
            else if (is("circle") || is("box"))
            {
                p.components |= bit(Component::Collider);
                const Vec2 size = pair(v, { 16, 16 });
                p.collider.shape = is("circle") ? Shape::Circle : Shape::Box;
                p.collider.halfWidth = is("circle") ? v.number() : size.x * 0.5f;
                p.collider.halfHeight = is("circle") ? v.number() : size.y * 0.5f;
            }
            else if (is("group") || is("hits"))
            {
                u32 mask = 0;
                if (!readGroups(v, mask, file))
                {
                    return false;
                }
                if (is("hits"))
                {
                    p.collider.hits = static_cast<u16>(mask);
                }
                else
                {
                    u32 group = 0;
                    while (!(mask & (1u << group)) && group + 1 < kGroups)
                    {
                        group++;
                    }
                    p.collider.group = static_cast<u8>(group);
                }
            }
            else if (is("wall"))
            {
                p.collider.wall = v.flag();
            }
            else if (is("lifetime"))
            {
                p.components |= bit(Component::Lifetime);
                p.lifetime.left = v.number();
            }
            else if (is("emitter"))
            {
                p.emitter.style = styleNamed(v.text());
                if (p.emitter.style == kNone)
                {
                    return fail(file, v.line(), "no particles called %s", v.text());
                }
                p.components |= bit(Component::Emitter);
            }
            else if (is("rate"))
            {
                p.emitter.rate = v.number();
            }
            else if (is("burst"))
            {
                p.burst = static_cast<u32>(v.number());
            }
            else if (is("behaviour"))
            {
                for (u32 i = 0; i < 2; i++)
                {
                    const Json name = v.isArray() ? v[i] : i == 0 ? v : Json();
                    if (name.isString())
                    {
                        snprintf(p.behaviours[i], sizeof(p.behaviours[i]), "%s", name.text());
                    }
                }
            }
            else
            {
                return fail(file, v.line(), "what is \"%s\"?", key);
            }
        }
        return true;
    }

    bool World::addParts(Json json, i32 parent, u32 depth, const char* file)
    {
        if (depth > 8)
        {
            return fail(file, json.line(), "%s", "children are nested too deeply");
        }
        PrefabPart p = {};
        p.scale = { 1, 1 };
        p.visual.blend = gpu::Blend::Alpha;
        p.visual.columns = p.visual.rows = 1;
        p.visual.color = { 1, 1, 1, 1 };
        p.animation.loop = true;
        p.body.friction = 0.4f;
        p.parent = parent;
        const u32 self = m_parts.count();
        const Json base = json["base"];
        u32 extra = 0;
        const Prefab* from = base.isString() ? prefabNamed(base.text()) : nullptr;
        if (base.isString() && from == nullptr)
        {
            return fail(file, base.line(), "no prefab called %s (it has to come first)", base.text());
        }
        if (from)
        {
            p = m_parts[from->first];
            p.parent = parent;
            extra = from->count - 1;
            if (p.visual.asset)
            {
                m_assets.addRef(p.visual.asset);
            }
        }
        const bool ok = readPart(json, p, file);
        if (!m_parts.push(p) || !ok)
        {
            return ok ? fail(file, json.line(), "%s", "no memory") : false;
        }
        for (u32 i = 0; i < extra; i++)
        {
            PrefabPart child = m_parts[from->first + 1 + i];
            child.parent = child.parent - static_cast<i32>(from->first) + static_cast<i32>(self);
            if (child.visual.asset)
            {
                m_assets.addRef(child.visual.asset);
            }
            m_parts.push(child);
        }
        for (Json c = json["children"].first(); c.exists(); c = c.next())
        {
            if (!addParts(c, static_cast<i32>(self), depth + 1, file))
            {
                return false;
            }
        }
        return true;
    }

    bool World::readPrefab(Json json, const char* name, const char* file)
    {
        Prefab prefab = {};
        snprintf(prefab.name, sizeof(prefab.name), "%s", name);
        prefab.first = m_parts.count();
        if (!addParts(json, -1, 0, file))
        {
            return false;
        }
        prefab.count = m_parts.count() - prefab.first;
        return m_prefabs.push(prefab);
    }

    bool World::readStyle(Json json, const char* file)
    {
        if (m_styles.count() == sizeof(m_particles) / sizeof(m_particles[0]))
        {
            return fail(file, json.line(), "%s", "too many kinds of particles");
        }
        ParticleStyle s = {};
        snprintf(s.name, sizeof(s.name), "%s", json.key());
        s.blend = gpu::Blend::Additive;
        s.lifeMin = s.lifeMax = 1;
        s.spread = 360;
        s.sizeStart = s.sizeEnd = 8;
        s.colorStart = { 1, 1, 1, 1 };
        s.colorEnd = { 1, 1, 1, 0 };
        for (Json v = json.first(); v.exists(); v = v.next())
        {
            const char* key = v.key();
            const auto is = [&](const char* name) { return strcmp(key, name) == 0; };
            const Vec2 two = pair(v, {});
            if (is("image"))
            {
                s.texture = readAsset(v, AssetType::Texture, 0, file);
                if (s.texture == nullptr)
                {
                    return false;
                }
            }
            else if (is("blend"))
            {
                if (!readBlend(v, s.blend))
                {
                    return fail(file, v.line(), "%s isn't a blend: use alpha, add or none", v.text());
                }
            }
            else if (is("color") || is("colorEnd"))
            {
                Color& color = is("color") ? s.colorStart : s.colorEnd;
                if (!readColor(v, color, file))
                {
                    return false;
                }
                if (is("color") && !json["colorEnd"].exists())
                {
                    s.colorEnd = { color.r, color.g, color.b, 0 };
                }
            }
            else if (is("life"))
            {
                s.lifeMin = two.x > 0.01f ? two.x : 0.01f;
                s.lifeMax = two.y > s.lifeMin ? two.y : s.lifeMin;
            }
            else if (is("speed"))
            {
                s.speedMin = two.x;
                s.speedMax = two.y;
            }
            else if (is("size"))
            {
                s.sizeStart = two.x;
                s.sizeEnd = two.y;
            }
            else if (is("layer"))
            {
                s.layer = static_cast<int>(v.number());
            }
            else if (is("angle"))
            {
                s.angle = v.number();
            }
            else if (is("spread"))
            {
                s.spread = v.number();
            }
            else if (is("gravity"))
            {
                s.gravity = v.number();
            }
            else if (is("drag"))
            {
                s.drag = v.number();
            }
            else if (is("radius"))
            {
                s.radius = v.number();
            }
            else
            {
                if (s.texture)
                {
                    m_assets.release(s.texture);
                }
                return fail(file, v.line(), "what is \"%s\"?", key);
            }
        }
        m_drawDirty = true;
        return m_styles.push(s);
    }

    bool World::readMap(Json json, const char* file)
    {
        const Json rows = json["rows"];
        if (!rows.isArray() || rows.count() == 0 || m_mapCount == kMaxMaps)
        {
            return fail(file, json.line(), "%s", "a map needs rows, and there can be four maps");
        }
        TileMap map = {};
        map.size = json["tile"].number(32);
        map.size = map.size < 1 ? 1 : map.size;
        map.layer = static_cast<int>(json["layer"].number(-10));
        map.origin = pair(json["position"], {});
        const Vec2 sheet = pair(json["sheet"], { 1, 1 });
        map.columns = sheet.x < 1 ? 1 : static_cast<u32>(sheet.x);
        map.rows = sheet.y < 1 ? 1 : static_cast<u32>(sheet.y);
        map.height = rows.count();
        for (Json r = rows.first(); r.exists(); r = r.next())
        {
            const u32 width = static_cast<u32>(strlen(r.text()));
            map.width = width > map.width ? width : map.width;
        }
        if (map.width == 0 || map.width > 4096 || map.height > 4096)
        {
            return fail(file, rows.line(), "%s", "a map can be up to 4096 tiles each way");
        }
        for (u32 i = 0; i < 16; i++)
        {
            map.colors[i] = { 0.5f, 0.5f, 0.5f, 1 };
        }
        for (Json c = json["colors"].first(); c.exists(); c = c.next())
        {
            const int tile = atoi(c.key());
            if (tile < 1 || tile > 15 || !readColor(c, map.colors[tile], file))
            {
                return m_error[0] ? false : fail(file, c.line(), "%s: colours are for tiles 1 to 15", c.key());
            }
        }
        for (u32 kind = 1; kind <= 2; kind++)
        {
            for (Json s = json[kind == 1 ? "solid" : "platforms"].first(); s.exists(); s = s.next())
            {
                const float tile = s.number();
                if (tile >= 1 && tile < 64)
                {
                    map.solid[static_cast<u32>(tile)] = static_cast<u8>(kind);
                }
            }
        }
        if (json["image"].exists())
        {
            map.texture = readAsset(json["image"], AssetType::Texture, 0, file);
            if (map.texture == nullptr)
            {
                return false;
            }
        }
        const size_t bytes = static_cast<size_t>(map.width) * map.height * sizeof(u16);
        map.tiles = static_cast<u16*>(memAlloc(bytes, MemTag::Scene));
        if (map.tiles == nullptr)
        {
            if (map.texture)
            {
                m_assets.release(map.texture);
            }
            return fail(file, json.line(), "%s", "no memory for the map");
        }
        memset(map.tiles, 0, bytes);
        m_maps[m_mapCount++] = map;  // from here the world frees it
        m_drawDirty = true;

        const Json legend = json["legend"];
        u32 y = 0;
        for (Json r = rows.first(); r.exists(); r = r.next(), y++)
        {
            const char* text = r.text();
            for (u32 x = 0; text[x]; x++)
            {
                const char name[2] = { text[x], 0 };
                const Json meaning = legend[name];
                if (meaning.isNumber())
                {
                    map.tiles[static_cast<size_t>(y) * map.width + x] = static_cast<u16>(meaning.number());
                }
                else if (meaning.isString())
                {
                    const Vec2 at = map.origin + Vec2(static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f) * map.size;
                    if (spawnPrefab(meaning.text(), at) == 0)
                    {
                        return m_error[0] ? false : fail(file, meaning.line(), "cannot spawn %s", meaning.text());
                    }
                }
                else if (text[x] != ' ' && text[x] != '.')
                {
                    return fail(file, r.line(), "the legend has no %s", name);
                }
            }
        }
        buildWalls(m_maps[m_mapCount - 1]);
        return true;
    }

    bool World::load(const char* path, const char* folder)
    {
        m_error[0] = 0;
        snprintf(m_folder, sizeof(m_folder), "%s", folder);
        if (m_fileCount == kMaxFiles)
        {
            return fail(path, 0, "%s", "too many scene files");
        }
        FileData file;
        Platform& platform = m_assets.platform();
        auto& record = m_files[m_fileCount];
        record.time = platform.fileTime(path);
        if (!platform.readFile(path, file, MemTag::Scene))
        {
            return fail(path, 0, "%s", "cannot be read");
        }
        JsonTree* json = nx::create<JsonTree>(MemTag::Scene);
        if (json == nullptr)
        {
            return fail(path, 0, "%s", "no memory");
        }
        record.json = json;
        snprintf(record.path, sizeof(record.path), "%s", path);
        m_fileCount++;
        if (!json->parse(reinterpret_cast<const char*>(file.data), file.size))
        {
            return fail(path, json->errorLine(), "%s", json->error());
        }
        const Json root = json->root();
        if (!root.isObject())
        {
            return fail(path, 1, "%s", "expected { }");
        }
        static const char* const known[] = { "groups", "gravity", "particles", "prefabs", "map", "maps", "entities" };
        for (Json v = root.first(); v.exists(); v = v.next())
        {
            bool found = false;
            for (const char* name : known)
            {
                found = found || strcmp(name, v.key()) == 0;
            }
            if (!found)
            {
                return fail(path, v.line(), "what is \"%s\"?", v.key());
            }
        }
        // In the order they are needed, whatever order they are written in.
        for (Json g = root["groups"].first(); g.exists(); g = g.next())
        {
            u32 free = 0;
            while (free < kGroups && m_groups[free][0] && strcmp(m_groups[free], g.text()) != 0)
            {
                free++;
            }
            if (free == kGroups)
            {
                return fail(path, g.line(), "%s", "there can be 16 groups");
            }
            snprintf(m_groups[free], sizeof(m_groups[free]), "%s", g.text());
        }
        if (root["gravity"].exists())
        {
            const Json g = root["gravity"];
            m_gravity = g.isNumber() ? Vec2(0, g.number()) : pair(g, {});
        }
        for (Json s = root["particles"].first(); s.exists(); s = s.next())
        {
            if (!readStyle(s, path))
            {
                return false;
            }
        }
        for (Json p = root["prefabs"].first(); p.exists(); p = p.next())
        {
            if (!readPrefab(p, p.key(), path))
            {
                return false;
            }
        }
        if (root["map"].exists() && !readMap(root["map"], path))
        {
            return false;
        }
        for (Json m = root["maps"].first(); m.exists(); m = m.next())
        {
            if (!readMap(m, path))
            {
                return false;
            }
        }
        for (Json e = root["entities"].first(); e.exists(); e = e.next())
        {
            if (e["prefab"].isString())
            {
                const Vec2 at = pair(e["position"], {});
                const u32 id = spawnPrefab(e["prefab"].text(), at);
                Entity* entity = find(id);
                if (entity == nullptr)
                {
                    return m_error[0] ? false : fail(path, e.line(), "cannot spawn %s", e["prefab"].text());
                }
                entity->rotation = e["rotation"].number(entity->rotation);
                entity->scale = pair(e["scale"], entity->scale);
                if (e["name"].isString())
                {
                    snprintf(entity->name, sizeof(entity->name), "%s", e["name"].text());
                }
            }
            else
            {
                const u32 before = m_prefabs.count();
                if (!readPrefab(e, "", path) || spawnPrefab("", pair(e["position"], {})) == 0)
                {
                    return m_error[0] ? false : fail(path, e.line(), "%s", "cannot spawn an entity");
                }
                m_prefabs.resize(before);
            }
        }
        return true;
    }

    bool World::changed() const
    {
        for (u32 i = 0; i < m_fileCount; i++)
        {
            const u64 time = m_assets.platform().fileTime(m_files[i].path);
            if (time && time != m_files[i].time)
            {
                return true;
            }
        }
        return false;
    }

    u32 World::spawnPart(const PrefabPart& part, Vec2 position)
    {
        const PrefabPart p = part;  // spawning may read more files
        const u32 id = spawn(position);
        Entity* e = find(id);
        if (e == nullptr)
        {
            return 0;
        }
        e->scale = p.scale;
        e->rotation = p.rotation;
        e->layer = p.layer;
        e->visible = !p.hidden;
        memcpy(e->name, p.name, sizeof(e->name));
        const auto has = [&](Component c) { return (p.components & bit(c)) != 0; };
        if (has(Component::Visual))
        {
            if (Visual* v = visual(id))
            {
                const u32 owner = v->owner;
                *v = p.visual;
                v->owner = owner;
                v->text = nullptr;
                if (v->asset)
                {
                    m_assets.addRef(v->asset);
                }
                if (p.visual.text)
                {
                    setText(id, p.visual.text);
                }
            }
        }
        const auto put = [](auto* to, const auto& from)
        {
            if (to)
            {
                const u32 owner = to->owner;
                *to = from;
                to->owner = owner;
            }
        };
        if (has(Component::Animation))
        {
            put(animation(id), p.animation);
        }
        if (has(Component::Body))
        {
            put(body(id), p.body);
        }
        if (has(Component::Collider))
        {
            put(collider(id), p.collider);
        }
        if (has(Component::Lifetime))
        {
            put(lifetime(id), p.lifetime);
        }
        if (has(Component::Emitter))
        {
            put(getOrAdd(m_emitters, Component::Emitter, id, p.emitter), p.emitter);
            if (p.burst)
            {
                burst(m_styles[p.emitter.style].name, position, p.burst);
            }
        }
        transform(id & ((1u << kIndexBits) - 1));
        return id;
    }

    u32 World::spawnPrefab(const char* name, Vec2 position)
    {
        const Prefab* found = prefabNamed(name);
        if (found == nullptr)
        {
            snprintf(m_error, sizeof(m_error), "no prefab called %s", name);
            return 0;
        }
        const Prefab prefab = *found;
        u32 ids[64];
        if (prefab.count > 64)
        {
            snprintf(m_error, sizeof(m_error), "%s has more than 64 parts", name);
            return 0;
        }
        for (u32 i = 0; i < prefab.count; i++)
        {
            const PrefabPart& part = m_parts[prefab.first + i];
            ids[i] = spawnPart(part, i == 0 ? position : part.position);
            if (ids[i] == 0)
            {
                destroyEntity(ids[0]);
                snprintf(m_error, sizeof(m_error), "too many entities");
                return 0;
            }
            if (i)
            {
                setParent(ids[i], ids[part.parent - static_cast<i32>(prefab.first)]);
            }
        }
        for (u32 i = 0; i < prefab.count; i++)
        {
            for (u32 k = 0; k < 2; k++)
            {
                char className[40];
                memcpy(className, m_parts[prefab.first + i].behaviours[k], sizeof(className));
                if (className[0] == 0)
                {
                    continue;
                }
                asITypeInfo* type = m_host.findType(m_module, className);
                asIScriptObject* object = type ? m_host.create(type) : nullptr;
                const bool added = object && addBehaviour(ids[i], object);
                if (object)
                {
                    m_host.release(object);
                }
                if (!added)
                {
                    destroyEntity(ids[0]);
                    snprintf(m_error, sizeof(m_error), "%s: no behaviour class called %s", name, className);
                    return 0;
                }
            }
        }
        return ids[0];
    }
}
