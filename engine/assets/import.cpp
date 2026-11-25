#include "assets/import.h"

#include "base/endian.h"
#include "base/json_value.h"
#include "base/memory.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace nx
{
    namespace
    {
        constexpr size_t kMost = 256 * 1024 * 1024;   // bytes of geometry: more is a mistake

        char lowerOf(char c)
        {
            return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
        }

        bool endsWith(const char* text, const char* with)
        {
            const size_t a = strlen(text), b = strlen(with);
            for (size_t i = 0; i < b; i++)
            {
                if (a < b || lowerOf(text[a - b + i]) != with[i])
                {
                    return false;
                }
            }
            return true;
        }

        bool starts(const u8* data, size_t size, const char* with)
        {
            const size_t length = strlen(with);
            return size >= length && memcmp(data, with, length) == 0;
        }

        // Files may name others by paths on the machine they were made on.
        bool readNamed(std::string name, ReadBeside read, void* user, std::vector<u8>& out)
        {
            for (char& c : name)
            {
                c = c == '\\' ? '/' : c;
            }
            while (name.compare(0, 2, "./") == 0)
            {
                name.erase(0, 2);
            }
            const bool inside = !name.empty() && name[0] != '/' && name.find(':') == std::string::npos && name.find("..") == std::string::npos;
            if (read == nullptr || name.empty())
            {
                return false;
            }
            if (inside && read(name.c_str(), out, user))
            {
                return true;
            }
            const size_t slash = name.find_last_of("/:");
            const std::string last = slash == std::string::npos ? name : name.substr(slash + 1);
            return last != name && !last.empty() && last != ".." && read(last.c_str(), out, user);
        }

        // --- Text ---

        // Words and numbers, a line at a time, whatever the machine's language.
        struct Text
        {
            const char* at;
            const char* end;

            bool more() const { return at < end; }

            void spaces()
            {
                while (at < end && (*at == ' ' || *at == '\t' || *at == '\r'))
                {
                    at++;
                }
            }

            bool lineEnds()
            {
                spaces();
                return at >= end || *at == '\n' || *at == '#';
            }

            void nextLine()
            {
                while (at < end && *at != '\n')
                {
                    at++;
                }
                at += at < end;
            }

            std::string word()
            {
                spaces();
                const char* from = at;
                while (at < end && *at != ' ' && *at != '\t' && *at != '\r' && *at != '\n')
                {
                    at++;
                }
                return std::string(from, at);
            }

            std::string rest()
            {
                spaces();
                const char* from = at;
                while (at < end && *at != '\n' && *at != '\r')
                {
                    at++;
                }
                const char* to = at;
                while (to > from && (to[-1] == ' ' || to[-1] == '\t'))
                {
                    to--;
                }
                return std::string(from, to);
            }

            // Any word, on this line or a later one. Empty at the end.
            std::string anyWord()
            {
                while (at < end && (*at == ' ' || *at == '\t' || *at == '\r' || *at == '\n'))
                {
                    at++;
                }
                return word();
            }
        };

        float numberOf(const std::string& word)
        {
            const char* p = word.c_str();
            bool minus = false;
            if (*p == '-' || *p == '+')
            {
                minus = *p++ == '-';
            }
            double v = 0;
            while (*p >= '0' && *p <= '9')
            {
                v = v * 10 + (*p++ - '0');
            }
            if (*p == '.')
            {
                double part = 0.1;
                for (p++; *p >= '0' && *p <= '9'; p++)
                {
                    v += (*p - '0') * part;
                    part *= 0.1;
                }
            }
            if (*p == 'e' || *p == 'E')
            {
                p++;
                const bool less = *p == '-';
                p += *p == '-' || *p == '+';
                int power = 0;
                while (*p >= '0' && *p <= '9' && power < 100)
                {
                    power = power * 10 + (*p++ - '0');
                }
                v *= pow(10.0, less ? -power : power);
            }
            v = minus ? -v : v;
            return v > -1e30 && v < 1e30 ? static_cast<float>(v) : 0.0f;
        }

        u8 colorOf(float v)
        {
            return static_cast<u8>((v < 0 ? 0 : v > 1 ? 1 : v) * 255 + 0.5f);
        }

        ImportVertex plain()
        {
            ImportVertex v;
            memset(&v, 0, sizeof(v));
            v.color[0] = v.color[1] = v.color[2] = v.color[3] = 255;
            return v;
        }

        // --- OBJ ---

        struct ObjMaterial
        {
            std::string name;
            int index;
        };

        void readMtl(const std::vector<u8>& file, ReadBeside read, void* user, ImportScene& scene, std::vector<ObjMaterial>& named)
        {
            Text t = { reinterpret_cast<const char*>(file.data()), reinterpret_cast<const char*>(file.data()) + file.size() };
            int now = -1;
            for (; t.more(); t.nextLine())
            {
                const std::string word = t.word();
                if (word == "newmtl")
                {
                    now = static_cast<int>(scene.materials.size());
                    scene.materials.push_back({});
                    named.push_back({ t.rest(), now });
                }
                if (now < 0)
                {
                    continue;
                }
                ImportMaterial& m = scene.materials[static_cast<size_t>(now)];
                if (word == "Kd")
                {
                    for (u32 i = 0; i < 3; i++)
                    {
                        m.color[i] = numberOf(t.word());
                    }
                }
                else if (word == "Ke")
                {
                    const float sum = numberOf(t.word()) + numberOf(t.word()) + numberOf(t.word());
                    m.glow = sum > 0.1f ? 1.0f : 0.0f;
                }
                else if (word == "d" || word == "Tr")
                {
                    const float v = numberOf(t.word());
                    m.color[3] = word == "d" ? v : 1 - v;
                    m.see = m.color[3] < 0.99f;
                }
                else if (word == "map_Kd")
                {
                    // Options come first, and the name may have spaces in it.
                    std::string name = t.rest();
                    while (!name.empty() && name[0] == '-')
                    {
                        size_t space = name.find(' ');
                        const std::string option = name.substr(0, space);
                        u32 values = option == "-o" || option == "-s" || option == "-t" ? 3 : option == "-mm" ? 2 : 1;
                        for (; values && space != std::string::npos; values--)
                        {
                            space = name.find(' ', space + 1);
                        }
                        name = space == std::string::npos ? "" : name.substr(space + 1);
                    }
                    m.image = scene.image(name.c_str(), read, user);
                    // Those that have a picture often have a colour that means nothing.
                    if (m.image >= 0 && m.color[0] + m.color[1] + m.color[2] < 0.01f)
                    {
                        m.color[0] = m.color[1] = m.color[2] = 1;
                    }
                }
            }
        }

        bool importObj(const u8* data, size_t size, ReadBeside read, void* user, ImportScene& scene, std::string& why)
        {
            std::vector<float> places, colors, uvs, normals;
            std::vector<ObjMaterial> named;
            std::vector<ImportPart> parts;
            std::vector<ImportVertex> face;
            int material = -1;
            size_t part = static_cast<size_t>(-1);
            Text t = { reinterpret_cast<const char*>(data), reinterpret_cast<const char*>(data) + size };
            for (; t.more(); t.nextLine())
            {
                const std::string word = t.word();
                if (word == "v")
                {
                    for (u32 i = 0; i < 3; i++)
                    {
                        places.push_back(numberOf(t.word()));
                    }
                    float c[3];
                    u32 given = 0;
                    while (given < 3 && !t.lineEnds())
                    {
                        c[given++] = numberOf(t.word());
                    }
                    if (given == 3)
                    {
                        colors.resize(places.size() - 3, 1.0f);
                        colors.insert(colors.end(), c, c + 3);
                    }
                }
                else if (word == "vt")
                {
                    uvs.push_back(numberOf(t.word()));
                    uvs.push_back(1 - numberOf(t.word()));
                }
                else if (word == "vn")
                {
                    for (u32 i = 0; i < 3; i++)
                    {
                        normals.push_back(numberOf(t.word()));
                    }
                }
                else if (word == "mtllib")
                {
                    std::vector<u8> file;
                    if (readNamed(t.rest(), read, user, file))
                    {
                        readMtl(file, read, user, scene, named);
                    }
                }
                else if (word == "usemtl")
                {
                    const std::string name = t.rest();
                    material = -1;
                    for (const ObjMaterial& m : named)
                    {
                        material = m.name == name ? m.index : material;
                    }
                    part = static_cast<size_t>(-1);
                }
                else if (word == "f")
                {
                    face.clear();
                    bool all = true;
                    while (!t.lineEnds())
                    {
                        const std::string corner = t.word();
                        long index[3] = {};
                        const char* p = corner.c_str();
                        for (u32 i = 0; i < 3 && *p; i++)
                        {
                            index[i] = strtol(p, nullptr, 10);
                            while (*p && *p != '/')
                            {
                                p++;
                            }
                            p += *p == '/';
                        }
                        // From 1, or back from the last.
                        const auto find = [](long i, size_t count) { return i > 0 ? static_cast<size_t>(i) - 1 : i < 0 && static_cast<size_t>(-i) <= count ? count - static_cast<size_t>(-i) : static_cast<size_t>(-1); };
                        const size_t v = find(index[0], places.size() / 3), uv = find(index[1], uvs.size() / 2), n = find(index[2], normals.size() / 3);
                        if (v >= places.size() / 3)
                        {
                            why = "a face uses a vertex that isn't there";
                            return false;
                        }
                        ImportVertex vertex = plain();
                        memcpy(vertex.place, &places[v * 3], sizeof(vertex.place));
                        if (v * 3 + 2 < colors.size())
                        {
                            for (u32 i = 0; i < 3; i++)
                            {
                                vertex.color[i] = colorOf(colors[v * 3 + i]);
                            }
                        }
                        if (uv < uvs.size() / 2)
                        {
                            memcpy(vertex.uv, &uvs[uv * 2], sizeof(vertex.uv));
                        }
                        all = all && n < normals.size() / 3;
                        if (n < normals.size() / 3)
                        {
                            memcpy(vertex.normal, &normals[n * 3], sizeof(vertex.normal));
                        }
                        face.push_back(vertex);
                    }
                    for (size_t i = 0; part == static_cast<size_t>(-1) && i < parts.size(); i++)
                    {
                        part = parts[i].material == material ? i : part;
                    }
                    if (part == static_cast<size_t>(-1))
                    {
                        part = parts.size();
                        parts.push_back({});
                        parts[part].material = material;
                    }
                    ImportPart& to = parts[part];
                    to.normals = to.normals && all;
                    to.colors = to.colors || !colors.empty();
                    for (size_t i = 2; i < face.size(); i++)
                    {
                        to.corner(face[0]);
                        to.corner(face[i - 1]);
                        to.corner(face[i]);
                    }
                }
                if ((places.size() + uvs.size() + normals.size()) * sizeof(float) > kMost)
                {
                    why = "it is too big";
                    return false;
                }
            }
            scene.nodes.push_back({});
            for (ImportPart& p : parts)
            {
                if (!p.indices.empty())
                {
                    scene.nodes[0].parts.push_back(static_cast<ImportPart&&>(p));
                }
            }
            return true;
        }

        // --- STL ---

        bool importStl(const u8* data, size_t size, ImportScene& scene, std::string& why)
        {
            ImportPart part;
            const auto add = [&](const float (&corners)[3][3])
            {
                // They stand on z; here y is up.
                Vec3 p[3];
                for (u32 i = 0; i < 3; i++)
                {
                    p[i] = { corners[i][0], corners[i][2], -corners[i][1] };
                }
                const Vec3 across = (p[1] - p[0]).cross(p[2] - p[0]);
                if (!(across.dot(across) > 0))
                {
                    return;
                }
                const Vec3 n = across.normalized();
                for (const Vec3& at : p)
                {
                    ImportVertex v = plain();
                    v.place[0] = at.x;
                    v.place[1] = at.y;
                    v.place[2] = at.z;
                    v.normal[0] = n.x;
                    v.normal[1] = n.y;
                    v.normal[2] = n.z;
                    part.corner(v);
                }
            };
            const u64 triangles = size >= 84 ? readU32(data + 80) : 0;
            if (size >= 84 && 84 + triangles * 50 <= size && (triangles || !starts(data, size, "solid")))
            {
                for (u64 i = 0; i < triangles; i++)
                {
                    float corners[3][3];
                    memcpy(corners, data + 84 + i * 50 + 12, sizeof(corners));
                    bool fine = true;
                    for (const auto& c : corners)
                    {
                        fine = fine && c[0] > -1e30f && c[0] < 1e30f && c[1] > -1e30f && c[1] < 1e30f && c[2] > -1e30f && c[2] < 1e30f;
                    }
                    if (fine)
                    {
                        add(corners);
                    }
                }
            }
            else
            {
                float corners[3][3];
                u32 count = 0;
                Text t = { reinterpret_cast<const char*>(data), reinterpret_cast<const char*>(data) + size };
                for (; t.more(); t.nextLine())
                {
                    const std::string word = t.word();
                    if (word == "outer")
                    {
                        count = 0;
                    }
                    else if (word == "vertex" && count < 3)
                    {
                        for (float& c : corners[count])
                        {
                            c = numberOf(t.word());
                        }
                        if (++count == 3)
                        {
                            add(corners);
                        }
                    }
                }
            }
            if (part.indices.empty())
            {
                why = "there are no triangles in it";
                return false;
            }
            scene.nodes.push_back({});
            scene.nodes[0].parts.push_back(static_cast<ImportPart&&>(part));
            return true;
        }

        // --- PLY ---

        struct PlyProperty
        {
            std::string name;
            int type, listType;   // sizes, less than 0 if signed; 14 and 18 are floats
        };

        int plyType(const std::string& name)
        {
            static const struct { const char* name; int type; } kTypes[] = { { "char", -1 }, { "int8", -1 }, { "uchar", 1 }, { "uint8", 1 }, { "short", -2 }, { "int16", -2 }, { "ushort", 2 }, { "uint16", 2 },
                { "int", -4 }, { "int32", -4 }, { "uint", 4 }, { "uint32", 4 }, { "float", 14 }, { "float32", 14 }, { "double", 18 }, { "float64", 18 } };
            for (const auto& t : kTypes)
            {
                if (name == t.name)
                {
                    return t.type;
                }
            }
            return 0;
        }

        struct PlyReader
        {
            Text text;
            bool ascii, big;
            bool ok = true;

            double value(int type)
            {
                if (ascii)
                {
                    const std::string word = text.anyWord();
                    ok = ok && !word.empty();
                    return static_cast<double>(numberOf(word));
                }
                const u32 bytes = type >= 14 ? static_cast<u32>(type - 10) : static_cast<u32>(abs(type));
                if (static_cast<size_t>(text.end - text.at) < bytes)
                {
                    ok = false;
                    return 0;
                }
                u64 raw = 0;
                for (u32 i = 0; i < bytes; i++)
                {
                    raw |= static_cast<u64>(static_cast<u8>(text.at[big ? bytes - 1 - i : i])) << (i * 8);
                }
                text.at += bytes;
                if (type == 14)
                {
                    const u32 half = static_cast<u32>(raw);
                    float f;
                    memcpy(&f, &half, 4);
                    return f > -1e30f && f < 1e30f ? static_cast<double>(f) : 0;
                }
                if (type == 18)
                {
                    double d;
                    memcpy(&d, &raw, 8);
                    return d > -1e30 && d < 1e30 ? d : 0;
                }
                if (type < 0 && raw >> (bytes * 8 - 1))
                {
                    return static_cast<double>(static_cast<i64>(raw) - (static_cast<i64>(1) << (bytes * 8)));
                }
                return static_cast<double>(raw);
            }
        };

        bool importPly(const u8* data, size_t size, ImportScene& scene, std::string& why)
        {
            Text t = { reinterpret_cast<const char*>(data), reinterpret_cast<const char*>(data) + size };
            struct Element
            {
                std::string name;
                u64 count;
                std::vector<PlyProperty> properties;
            };
            std::vector<Element> elements;
            PlyReader in = { t, true, false };
            bool began = false;
            for (t.nextLine(); t.more() && !began; t.nextLine())
            {
                const std::string word = t.word();
                if (word == "format")
                {
                    const std::string format = t.word();
                    in.ascii = format == "ascii";
                    in.big = format == "binary_big_endian";
                }
                else if (word == "element")
                {
                    Element e;
                    e.name = t.word();
                    e.count = static_cast<u64>(strtoul(t.word().c_str(), nullptr, 10));
                    elements.push_back(e);
                }
                else if (word == "property" && !elements.empty())
                {
                    PlyProperty p = {};
                    std::string type = t.word();
                    if (type == "list")
                    {
                        p.listType = plyType(t.word());
                        type = t.word();
                        if (p.listType == 0)
                        {
                            why = "a list in it is of a type that isn't known";
                            return false;
                        }
                    }
                    p.type = plyType(type);
                    p.name = t.word();
                    if (p.type == 0)
                    {
                        why = "something in it is of a type that isn't known";
                        return false;
                    }
                    elements.back().properties.push_back(p);
                }
                began = word == "end_header";
            }
            in.text = t;
            std::vector<ImportVertex> vertices;
            ImportPart part;
            part.normals = false;
            for (const Element& e : elements)
            {
                const bool isVertex = e.name == "vertex", isFace = e.name == "face";
                // Nothing has fewer bytes than that.
                if (e.count > size)
                {
                    why = "it says it has more than it does";
                    return false;
                }
                for (u64 i = 0; i < e.count && in.ok; i++)
                {
                    ImportVertex v = plain();
                    for (const PlyProperty& p : e.properties)
                    {
                        if (p.listType)
                        {
                            const double many = in.value(p.listType);
                            if (!(many >= 0 && many <= 10000))
                            {
                                why = "a list in it is too long";
                                return false;
                            }
                            const bool wanted = isFace && (p.name == "vertex_indices" || p.name == "vertex_index");
                            std::vector<ImportVertex> corners;
                            for (u32 k = 0; k < static_cast<u32>(many) && in.ok; k++)
                            {
                                const double index = in.value(p.type);
                                if (wanted && in.ok)
                                {
                                    if (!(index >= 0 && index < static_cast<double>(vertices.size())))
                                    {
                                        why = "a face uses a vertex that isn't there";
                                        return false;
                                    }
                                    corners.push_back(vertices[static_cast<size_t>(index)]);
                                }
                            }
                            for (size_t k = 2; k < corners.size(); k++)
                            {
                                part.corner(corners[0]);
                                part.corner(corners[k - 1]);
                                part.corner(corners[k]);
                            }
                            continue;
                        }
                        const float value = static_cast<float>(in.value(p.type));
                        if (!isVertex)
                        {
                            continue;
                        }
                        const std::string& n = p.name;
                        if (n == "x" || n == "y" || n == "z")
                        {
                            v.place[n[0] - 'x'] = value;
                        }
                        else if (n == "nx" || n == "ny" || n == "nz")
                        {
                            v.normal[n[1] - 'x'] = value;
                            part.normals = true;
                        }
                        else if (n == "red" || n == "green" || n == "blue" || n == "alpha")
                        {
                            const u32 which = n == "red" ? 0 : n == "green" ? 1 : n == "blue" ? 2 : 3;
                            v.color[which] = p.type >= 14 ? colorOf(value) : static_cast<u8>(value < 0 ? 0 : value > 255 ? 255 : value);
                            part.colors = true;
                        }
                        else if (n == "s" || n == "u" || n == "texture_u")
                        {
                            v.uv[0] = value;
                        }
                        else if (n == "t" || n == "v" || n == "texture_v")
                        {
                            v.uv[1] = 1 - value;
                        }
                    }
                    if (isVertex)
                    {
                        vertices.push_back(v);
                    }
                }
            }
            if (!in.ok || part.indices.empty())
            {
                why = in.ok ? "there are no triangles in it" : "it is cut short";
                return false;
            }
            scene.nodes.push_back({});
            scene.nodes[0].parts.push_back(static_cast<ImportPart&&>(part));
            return true;
        }

        // --- glTF in several files ---

        bool fromBase64(const std::string& text, std::vector<u8>& out)
        {
            u32 held = 0;
            int bits = 0;
            for (const char c : text)
            {
                const int v = c >= 'A' && c <= 'Z' ? c - 'A' : c >= 'a' && c <= 'z' ? c - 'a' + 26 : c >= '0' && c <= '9' ? c - '0' + 52 : c == '+' ? 62 : c == '/' ? 63 : c == '=' ? -2 : -1;
                if (v == -2)
                {
                    break;
                }
                if (v < 0)
                {
                    return false;
                }
                held = (held << 6) | static_cast<u32>(v);
                bits += 6;
                if (bits >= 8)
                {
                    bits -= 8;
                    out.push_back(static_cast<u8>(held >> bits));
                }
            }
            return true;
        }

        bool readUri(const std::string& uri, ReadBeside read, void* user, std::vector<u8>& out)
        {
            if (uri.compare(0, 5, "data:") == 0)
            {
                const size_t comma = uri.find(";base64,");
                return comma != std::string::npos && fromBase64(uri.substr(comma + 8), out);
            }
            std::string name;
            for (size_t i = 0; i < uri.size(); i++)
            {
                if (uri[i] == '%' && i + 2 < uri.size())
                {
                    name += static_cast<char>(strtoul(uri.substr(i + 1, 2).c_str(), nullptr, 16));
                    i += 2;
                }
                else
                {
                    name += uri[i];
                }
            }
            return readNamed(name, read, user, out);
        }

        const char* mimeOf(const std::vector<u8>& image)
        {
            return image.size() > 3 && image[0] == 0x89 ? "image/png" : image.size() > 3 && image[0] == 0xFF && image[1] == 0xD8 ? "image/jpeg" : "application/octet-stream";
        }

        void pad(std::vector<u8>& bytes, u8 with)
        {
            while (bytes.size() % 4)
            {
                bytes.push_back(with);
            }
        }

        void put(std::vector<u8>& out, u32 v)
        {
            for (u32 i = 0; i < 4; i++)
            {
                out.push_back(static_cast<u8>(v >> (i * 8)));
            }
        }

        void finish(const std::string& json, std::vector<u8>& bin, std::vector<u8>& glb)
        {
            std::vector<u8> text(json.begin(), json.end());
            pad(text, ' ');
            pad(bin, 0);
            glb.clear();
            glb.reserve(28 + text.size() + bin.size());
            put(glb, 0x46546C67);
            put(glb, 2);
            put(glb, static_cast<u32>(12 + 8 + text.size() + (bin.empty() ? 0 : 8 + bin.size())));
            put(glb, static_cast<u32>(text.size()));
            put(glb, 0x4E4F534A);
            glb.insert(glb.end(), text.begin(), text.end());
            if (!bin.empty())
            {
                put(glb, static_cast<u32>(bin.size()));
                put(glb, 0x004E4942);
                glb.insert(glb.end(), bin.begin(), bin.end());
            }
        }

        struct Held
        {
            JsonValue* value = create<JsonValue>(MemTag::General);
            ~Held() { value->release(); }
            Held() = default;
            Held(const Held&) = delete;
            Held& operator=(const Held&) = delete;
        };

        bool packGltf(const u8* data, size_t size, ReadBeside read, void* user, std::vector<u8>& glb, std::string& why)
        {
            Held held;
            JsonValue& json = *held.value;
            if (!json.parse(reinterpret_cast<const char*>(data), size, why))
            {
                return false;
            }
            std::vector<u8> bin;
            std::vector<u32> at;
            if (const JsonValue* buffers = json.find("buffers"))
            {
                for (u32 i = 0; i < buffers->count(); i++)
                {
                    const JsonValue* uri = buffers->at(i)->find("uri");
                    std::vector<u8> bytes;
                    if (uri == nullptr || !readUri(uri->text(), read, user, bytes) || bytes.size() + bin.size() > kMost)
                    {
                        why = "a file it needs isn't there: " + (uri ? uri->text().substr(0, 60) : std::string("no name"));
                        return false;
                    }
                    pad(bin, 0);
                    at.push_back(static_cast<u32>(bin.size()));
                    bin.insert(bin.end(), bytes.begin(), bytes.end());
                }
            }
            JsonValue* views = json.put("bufferViews");
            for (u32 i = 0; i < views->count(); i++)
            {
                JsonValue* view = views->at(i);
                const JsonValue* buffer = view->find("buffer");
                const JsonValue* offset = view->find("byteOffset");
                const double which = buffer ? buffer->number() : 0;
                if (!(which >= 0 && which < static_cast<double>(at.size())))
                {
                    why = "it is damaged";
                    return false;
                }
                const double from = (offset ? offset->number() : 0) + at[static_cast<size_t>(which)];
                view->put("buffer")->setNumber(0);
                view->put("byteOffset")->setNumber(from);
            }
            if (JsonValue* images = json.find("images"))
            {
                for (u32 i = 0; i < images->count(); i++)
                {
                    JsonValue* image = images->at(i);
                    const JsonValue* uri = image->find("uri");
                    std::vector<u8> bytes;
                    if (uri == nullptr)
                    {
                        continue;
                    }
                    // One that is missing is left out, and the rest shows.
                    const bool there = readUri(uri->text(), read, user, bytes) && bytes.size() + bin.size() <= kMost;
                    image->remove("uri");
                    if (there)
                    {
                        pad(bin, 0);
                        JsonValue* view = views->add();
                        view->put("buffer")->setNumber(0);
                        view->put("byteOffset")->setNumber(static_cast<double>(bin.size()));
                        view->put("byteLength")->setNumber(static_cast<double>(bytes.size()));
                        image->put("bufferView")->setNumber(views->count() - 1);
                        image->put("mimeType")->setText(mimeOf(bytes));
                        bin.insert(bin.end(), bytes.begin(), bytes.end());
                    }
                }
            }
            if (views->count() == 0)
            {
                json.remove("bufferViews");
            }
            pad(bin, 0);
            json.put("buffers")->setArray();
            json.put("buffers")->add()->put("byteLength")->setNumber(static_cast<double>(bin.size()));
            finish(json.write(true), bin, glb);
            return true;
        }

        // --- Writing ---

        struct Writer
        {
            std::string views, accessors, rest;
            std::vector<u8> bin;
            u32 viewCount = 0, accessorCount = 0;

            static std::string number(float v)
            {
                char text[32];
                snprintf(text, sizeof(text), "%.7g", static_cast<double>(v));
                for (char* c = text; *c; c++)
                {
                    *c = *c == ',' ? '.' : *c;   // in some languages
                }
                return text;
            }

            static std::string numbers(const float* v, u32 count)
            {
                std::string out = "[";
                for (u32 i = 0; i < count; i++)
                {
                    out += (i ? "," : "") + number(v[i]);
                }
                return out + "]";
            }

            static std::string quoted(const std::string& text)
            {
                std::string out = "\"";
                for (const char c : text)
                {
                    out += c == '"' || c == '\\' ? std::string("\\") + c : static_cast<u8>(c) < 32 ? std::string(" ") : std::string(1, c);
                }
                return out + "\"";
            }

            u32 view(const void* data, size_t size, u32 stride = 0)
            {
                pad(bin, 0);
                views += (viewCount ? "," : "") + std::string("{\"buffer\":0,\"byteOffset\":") + std::to_string(bin.size()) + ",\"byteLength\":" + std::to_string(size) + (stride ? ",\"byteStride\":" + std::to_string(stride) : "") + "}";
                const u8* bytes = static_cast<const u8*>(data);
                bin.insert(bin.end(), bytes, bytes + size);
                return viewCount++;
            }

            u32 accessor(u32 inView, u32 offset, u32 type, const char* shape, size_t count, const std::string& more = "")
            {
                accessors += (accessorCount ? "," : "") + std::string("{\"bufferView\":") + std::to_string(inView) + ",\"byteOffset\":" + std::to_string(offset) + ",\"componentType\":" + std::to_string(type) + ",\"count\":" +
                             std::to_string(count) + ",\"type\":\"" + shape + "\"" + more + "}";
                return accessorCount++;
            }
        };

        void writeScene(ImportScene& scene, std::vector<u8>& glb)
        {
            Writer w;
            std::string meshes, nodes, roots, skins, materials, images, textures, animations;
            std::vector<u32> joints;
            std::vector<int> jointOf(scene.nodes.size(), -1);
            for (size_t i = 0; i < scene.nodes.size(); i++)
            {
                if (scene.nodes[i].joint)
                {
                    jointOf[i] = static_cast<int>(joints.size());
                    joints.push_back(static_cast<u32>(i));
                }
            }
            u32 meshCount = 0;
            for (size_t i = 0; i < scene.nodes.size(); i++)
            {
                ImportNode& n = scene.nodes[i];
                std::string primitives;
                bool skinned = false;
                for (ImportPart& p : n.parts)
                {
                    if (p.indices.empty())
                    {
                        continue;
                    }
                    if (!p.normals)
                    {
                        p.makeNormals();
                    }
                    skinned = skinned || p.weights;
                    // Bones are nodes until now; a skin has its own numbers for them.
                    for (ImportVertex& v : p.vertices)
                    {
                        for (u8& b : v.bones)
                        {
                            b = p.weights && b < jointOf.size() && jointOf[b] >= 0 ? static_cast<u8>(jointOf[b]) : 0;
                        }
                    }
                    float low[3] = { 1e30f, 1e30f, 1e30f }, high[3] = { -1e30f, -1e30f, -1e30f };
                    for (const ImportVertex& v : p.vertices)
                    {
                        for (u32 k = 0; k < 3; k++)
                        {
                            low[k] = fminf(low[k], v.place[k]);
                            high[k] = fmaxf(high[k], v.place[k]);
                        }
                    }
                    const u32 stride = sizeof(ImportVertex);
                    const u32 view = w.view(p.vertices.data(), p.vertices.size() * stride, stride);
                    const size_t count = p.vertices.size();
                    const u32 place = w.accessor(view, 0, 5126, "VEC3", count, ",\"min\":" + Writer::numbers(low, 3) + ",\"max\":" + Writer::numbers(high, 3));
                    const u32 normal = w.accessor(view, 12, 5126, "VEC3", count);
                    const u32 uv = w.accessor(view, 24, 5126, "VEC2", count);
                    std::string attributes = "\"POSITION\":" + std::to_string(place) + ",\"NORMAL\":" + std::to_string(normal) + ",\"TEXCOORD_0\":" + std::to_string(uv);
                    if (p.colors)
                    {
                        attributes += ",\"COLOR_0\":" + std::to_string(w.accessor(view, 32, 5121, "VEC4", count, ",\"normalized\":true"));
                    }
                    if (p.weights)
                    {
                        attributes += ",\"JOINTS_0\":" + std::to_string(w.accessor(view, 36, 5121, "VEC4", count));
                        attributes += ",\"WEIGHTS_0\":" + std::to_string(w.accessor(view, 40, 5121, "VEC4", count, ",\"normalized\":true"));
                    }
                    const u32 indices = w.accessor(w.view(p.indices.data(), p.indices.size() * 4), 0, 5125, "SCALAR", p.indices.size());
                    primitives += (primitives.empty() ? "" : ",") + std::string("{\"attributes\":{") + attributes + "},\"indices\":" + std::to_string(indices) + (p.material >= 0 ? ",\"material\":" + std::to_string(p.material) : "") + "}";
                }
                std::string children;
                for (size_t k = 0; k < scene.nodes.size(); k++)
                {
                    if (scene.nodes[k].parent == static_cast<int>(i))
                    {
                        children += (children.empty() ? "" : ",") + std::to_string(k);
                    }
                }
                const float place[3] = { n.place.x, n.place.y, n.place.z }, turn[4] = { n.turn.x, n.turn.y, n.turn.z, n.turn.w }, size[3] = { n.size.x, n.size.y, n.size.z };
                nodes += (i ? "," : "") + std::string("{\"name\":") + Writer::quoted(n.name) + ",\"translation\":" + Writer::numbers(place, 3) + ",\"rotation\":" + Writer::numbers(turn, 4) + ",\"scale\":" + Writer::numbers(size, 3);
                nodes += children.empty() ? "" : ",\"children\":[" + children + "]";
                if (!primitives.empty())
                {
                    meshes += (meshCount ? "," : "") + std::string("{\"primitives\":[") + primitives + "]}";
                    nodes += ",\"mesh\":" + std::to_string(meshCount++) + (skinned && !joints.empty() ? ",\"skin\":0" : "");
                }
                nodes += "}";
                if (n.parent < 0)
                {
                    roots += (roots.empty() ? "" : ",") + std::to_string(i);
                }
            }
            if (!joints.empty())
            {
                std::string list;
                std::vector<float> binds;
                for (const u32 j : joints)
                {
                    list += (list.empty() ? "" : ",") + std::to_string(j);
                    binds.insert(binds.end(), scene.nodes[j].inverseBind.m, scene.nodes[j].inverseBind.m + 16);
                }
                skins = "{\"joints\":[" + list + "],\"inverseBindMatrices\":" + std::to_string(w.accessor(w.view(binds.data(), binds.size() * 4), 0, 5126, "MAT4", joints.size())) + "}";
            }
            for (size_t i = 0; i < scene.images.size(); i++)
            {
                const std::vector<u8>& bytes = scene.images[i];
                images += (i ? "," : "") + std::string("{\"mimeType\":\"") + mimeOf(bytes) + "\",\"bufferView\":" + std::to_string(w.view(bytes.data(), bytes.size())) + "}";
                textures += (i ? "," : "") + std::string("{\"source\":") + std::to_string(i) + "}";
            }
            for (size_t i = 0; i < scene.materials.size(); i++)
            {
                const ImportMaterial& m = scene.materials[i];
                const float glow[3] = { m.glow, m.glow, m.glow };
                materials += (i ? "," : "") + std::string("{\"pbrMetallicRoughness\":{\"baseColorFactor\":") + Writer::numbers(m.color, 4) + (m.image >= 0 ? ",\"baseColorTexture\":{\"index\":" + std::to_string(m.image) + "}" : "") + "}";
                materials += std::string(m.twoSided ? ",\"doubleSided\":true" : "") + (m.see ? ",\"alphaMode\":\"BLEND\"" : "") + (m.glow > 0 ? ",\"emissiveFactor\":" + Writer::numbers(glow, 3) : "") + "}";
            }
            for (const ImportAnimation& a : scene.animations)
            {
                std::string channels, samplers;
                u32 count = 0;
                for (const ImportChannel& c : a.channels)
                {
                    const float last[1] = { c.times.back() }, first[1] = { c.times.front() };
                    const u32 times = w.accessor(w.view(c.times.data(), c.times.size() * 4), 0, 5126, "SCALAR", c.times.size(), ",\"min\":" + Writer::numbers(first, 1) + ",\"max\":" + Writer::numbers(last, 1));
                    const u32 values = w.accessor(w.view(c.values.data(), c.values.size() * 4), 0, 5126, c.path == 1 ? "VEC4" : "VEC3", c.times.size());
                    samplers += (count ? "," : "") + std::string("{\"input\":") + std::to_string(times) + ",\"output\":" + std::to_string(values) + "}";
                    channels += (count ? "," : "") + std::string("{\"sampler\":") + std::to_string(count) + ",\"target\":{\"node\":" + std::to_string(c.node) + ",\"path\":\"" + (c.path == 0 ? "translation" : c.path == 1 ? "rotation" : "scale") + "\"}}";
                    count++;
                }
                if (count)
                {
                    animations += (animations.empty() ? "" : ",") + std::string("{\"name\":") + Writer::quoted(a.name) + ",\"channels\":[" + channels + "],\"samplers\":[" + samplers + "]}";
                }
            }
            pad(w.bin, 0);
            std::string json = "{\"asset\":{\"version\":\"2.0\",\"generator\":\"Nexgen\"},\"scene\":0,\"scenes\":[{\"nodes\":[" + roots + "]}],\"nodes\":[" + nodes + "]";
            const auto list = [&](const char* name, const std::string& items)
            {
                if (!items.empty())
                {
                    json += std::string(",\"") + name + "\":[" + items + "]";
                }
            };
            list("meshes", meshes);
            list("skins", skins);
            list("materials", materials);
            list("images", images);
            list("textures", textures);
            list("animations", animations);
            list("accessors", w.accessors);
            list("bufferViews", w.views);
            json += ",\"buffers\":[{\"byteLength\":" + std::to_string(w.bin.size()) + "}]}";
            finish(json, w.bin, glb);
        }
    }

    // --- Parts ---

    void ImportPart::grow()
    {
        const size_t size = m_table.empty() ? 1024 : m_table.size() * 2;
        m_table.assign(size, 0);
        for (size_t i = 0; i < vertices.size(); i++)
        {
            const u8* bytes = reinterpret_cast<const u8*>(&vertices[i]);
            u32 h = 2166136261u;
            for (size_t k = 0; k < sizeof(ImportVertex); k++)
            {
                h = (h ^ bytes[k]) * 16777619u;
            }
            size_t slot = h & (size - 1);
            while (m_table[slot])
            {
                slot = (slot + 1) & (size - 1);
            }
            m_table[slot] = static_cast<u32>(i) + 1;
        }
    }

    void ImportPart::corner(const ImportVertex& given)
    {
        ImportVertex vertex = given;
        for (float* f : { vertex.place, vertex.normal })
        {
            for (u32 i = 0; i < 3; i++)
            {
                f[i] = f[i] == 0 ? 0 : f[i];   // there are two zeros
            }
        }
        if (vertices.size() * 2 >= m_table.size())
        {
            grow();
        }
        const u8* bytes = reinterpret_cast<const u8*>(&vertex);
        u32 h = 2166136261u;
        for (size_t k = 0; k < sizeof(ImportVertex); k++)
        {
            h = (h ^ bytes[k]) * 16777619u;
        }
        size_t slot = h & (m_table.size() - 1);
        for (; m_table[slot]; slot = (slot + 1) & (m_table.size() - 1))
        {
            if (memcmp(&vertices[m_table[slot] - 1], &vertex, sizeof(vertex)) == 0)
            {
                indices.push_back(m_table[slot] - 1);
                return;
            }
        }
        vertices.push_back(vertex);
        m_table[slot] = static_cast<u32>(vertices.size());
        indices.push_back(static_cast<u32>(vertices.size()) - 1);
    }

    void ImportPart::makeNormals()
    {
        // Vertices may differ by more than their place, so those in one place are added together.
        ImportPart places;
        std::vector<Vec3> sums;
        for (const ImportVertex& v : vertices)
        {
            ImportVertex bare;
            memset(&bare, 0, sizeof(bare));
            memcpy(bare.place, v.place, sizeof(bare.place));
            places.corner(bare);
        }
        sums.resize(places.vertices.size());
        for (size_t i = 0; i + 2 < indices.size(); i += 3)
        {
            Vec3 p[3];
            for (u32 k = 0; k < 3; k++)
            {
                const float* at = vertices[indices[i + k]].place;
                p[k] = { at[0], at[1], at[2] };
            }
            const Vec3 across = (p[1] - p[0]).cross(p[2] - p[0]);
            for (u32 k = 0; k < 3; k++)
            {
                Vec3& sum = sums[places.indices[indices[i + k]]];
                sum = sum + across;
            }
        }
        for (size_t i = 0; i < vertices.size(); i++)
        {
            const Vec3& sum = sums[places.indices[i]];
            const Vec3 n = sum.dot(sum) > 0 ? sum.normalized() : Vec3(0, 1, 0);
            vertices[i].normal[0] = n.x;
            vertices[i].normal[1] = n.y;
            vertices[i].normal[2] = n.z;
        }
        normals = true;
    }

    int ImportScene::image(const char* name, ReadBeside read, void* user)
    {
        for (size_t i = 0; i < m_names.size(); i++)
        {
            if (m_names[i] == name)
            {
                return static_cast<int>(i);
            }
        }
        std::vector<u8> bytes;
        return readNamed(name, read, user, bytes) ? image(static_cast<std::vector<u8>&&>(bytes), name) : -1;
    }

    int ImportScene::image(std::vector<u8>&& bytes, const std::string& key)
    {
        for (size_t i = 0; i < m_names.size(); i++)
        {
            if (m_names[i] == key)
            {
                return static_cast<int>(i);
            }
        }
        if (images.size() >= 16 || bytes.empty())
        {
            return -1;
        }
        images.push_back(static_cast<std::vector<u8>&&>(bytes));
        m_names.push_back(key);
        return static_cast<int>(images.size()) - 1;
    }

    ModelKind modelKindByName(const char* name)
    {
        return endsWith(name, ".glb") ? ModelKind::Glb : endsWith(name, ".gltf") ? ModelKind::Gltf : endsWith(name, ".obj") ? ModelKind::Obj : endsWith(name, ".stl") ? ModelKind::Stl : endsWith(name, ".ply") ? ModelKind::Ply : endsWith(name, ".fbx") ? ModelKind::Fbx : ModelKind::None;
    }

    ModelKind modelKind(const char* name, const u8* data, size_t size)
    {
        return starts(data, size, "glTF") ? ModelKind::Glb : starts(data, size, "Kaydara FBX") ? ModelKind::Fbx : starts(data, size, "ply\n") || starts(data, size, "ply\r") ? ModelKind::Ply : modelKindByName(name);
    }

#if defined(NX_FBX)
    bool importsFbx() { return true; }
#else
    bool importsFbx() { return false; }

    bool importFbx(const u8*, size_t, ReadBeside, void*, ImportScene&, std::string& why)
    {
        why = "FBX has to be cooked for this platform";
        return false;
    }
#endif

    bool importModel(const char* name, const u8* data, size_t size, ReadBeside read, void* user, std::vector<u8>& glb, std::string& why)
    {
        ImportScene scene;
        bool ok = false;
        switch (modelKind(name, data, size))
        {
        case ModelKind::Glb: glb.assign(data, data + size); return true;
        case ModelKind::Gltf: return packGltf(data, size, read, user, glb, why);
        case ModelKind::Obj: ok = importObj(data, size, read, user, scene, why); break;
        case ModelKind::Stl: ok = importStl(data, size, scene, why); break;
        case ModelKind::Ply: ok = importPly(data, size, scene, why); break;
        case ModelKind::Fbx: ok = importFbx(data, size, read, user, scene, why); break;
        default: why = "it isn't a kind of model that can be read"; return false;
        }
        if (!ok)
        {
            return false;
        }
        bool any = false;
        for (const ImportNode& n : scene.nodes)
        {
            any = any || !n.parts.empty();
        }
        if (!any)
        {
            why = "there are no triangles in it";
            return false;
        }
        writeScene(scene, glb);
        return true;
    }
}
