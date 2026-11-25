#include "base/json_value.h"
#include "bindings/script_refs.h"

#include <stdio.h>
#include <string.h>

namespace nx
{
    namespace
    {
        constexpr size_t kMaxFile = 1024 * 1024;

        // --- Json ---

        JsonValue* makeJson() { return create<JsonValue>(MemTag::Script); }
        JsonValue* makeJsonText(const std::string& text)
        {
            JsonValue* j = makeJson();
            std::string error;
            if (!j->parse(text.data(), text.size(), error))
            {
                j->release();
                scriptError(error);
                return nullptr;
            }
            return j;
        }
        void jsonCopy(const JsonValue* other, JsonValue* j)
        {
            if (other && !j->setCopy(*other))
            {
                scriptError("too much inside one another");
            }
        }
        bool jsonParse(const std::string& text, std::string& error, JsonValue* j) { return j->parse(text.data(), text.size(), error); }
        std::string jsonWrite(bool oneLine, const JsonValue* j) { return j->write(oneLine); }

        // One that isn't there is null, and can be asked things all the same.
        JsonValue* held(JsonValue* v)
        {
            if (v == nullptr)
            {
                return makeJson();
            }
            v->addRef();
            return v;
        }
        JsonValue* jsonKey(const std::string& key, JsonValue* j) { return held(j->find(key.c_str())); }
        JsonValue* jsonIndex(u32 index, JsonValue* j) { return held(j->at(index)); }
        bool jsonHas(const std::string& key, const JsonValue* j) { return j->find(key.c_str()) != nullptr; }
        std::string jsonKeyAt(u32 index, const JsonValue* j) { return j->key(index); }
        bool jsonIs(const JsonValue* j, JsonValue::Type type) { return j->type() == type; }
        bool jsonIsNull(const JsonValue* j) { return jsonIs(j, JsonValue::Type::Null); }
        bool jsonIsNumber(const JsonValue* j) { return jsonIs(j, JsonValue::Type::Number); }
        bool jsonIsText(const JsonValue* j) { return jsonIs(j, JsonValue::Type::String); }
        bool jsonIsFlag(const JsonValue* j) { return jsonIs(j, JsonValue::Type::Bool); }
        bool jsonIsArray(const JsonValue* j) { return jsonIs(j, JsonValue::Type::Array); }
        bool jsonIsObject(const JsonValue* j) { return jsonIs(j, JsonValue::Type::Object); }

        double jsonNumber(double otherwise, const JsonValue* j) { return jsonIsNumber(j) ? j->number() : otherwise; }
        std::string jsonText(const std::string& otherwise, const JsonValue* j) { return jsonIsText(j) ? j->text() : otherwise; }
        bool jsonFlag(bool otherwise, const JsonValue* j) { return jsonIsFlag(j) ? j->flag() : otherwise; }
        float part(const JsonValue* j, u32 index, float otherwise)
        {
            const JsonValue* v = j->at(index);
            return v && jsonIsNumber(v) ? static_cast<float>(v->number()) : otherwise;
        }
        Vec3 jsonVec3(const Vec3& otherwise, const JsonValue* j)
        {
            return jsonIsArray(j) ? Vec3(part(j, 0, otherwise.x), part(j, 1, otherwise.y), part(j, 2, otherwise.z)) : otherwise;
        }
        int hex(char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : 0; }
        Color jsonColor(const Color& otherwise, const JsonValue* j)
        {
            if (jsonIsArray(j))
            {
                return { part(j, 0, 0), part(j, 1, 0), part(j, 2, 0), part(j, 3, 1) };
            }
            const std::string& text = j->text();
            if (!jsonIsText(j) || (text.size() != 7 && text.size() != 9) || text[0] != '#')
            {
                return otherwise;
            }
            float parts[4] = { 1, 1, 1, 1 };
            for (size_t i = 0; i < (text.size() - 1) / 2; i++)
            {
                parts[i] = static_cast<float>(hex(text[1 + i * 2]) * 16 + hex(text[2 + i * 2])) / 255.0f;
            }
            return { parts[0], parts[1], parts[2], parts[3] };
        }

        JsonValue* place(JsonValue* v)
        {
            if (v == nullptr)
            {
                scriptError("too much inside one another");
            }
            return v;
        }
        void setParts(JsonValue* v, const float* parts, u32 count)
        {
            v->setArray();
            for (u32 i = 0; i < count; i++)
            {
                JsonValue* p = v->add();
                if (p == nullptr)
                {
                    scriptError("too much inside one another");
                    return;
                }
                p->setNumber(static_cast<double>(parts[i]));
            }
        }
        void write(JsonValue* v, double value) { v->setNumber(value); }
        void write(JsonValue* v, bool value) { v->setFlag(value); }
        void write(JsonValue* v, const std::string& value) { v->setText(value); }
        void write(JsonValue* v, const Vec3& value)
        {
            const float parts[] = { value.x, value.y, value.z };
            setParts(v, parts, 3);
        }
        void write(JsonValue* v, const Color& c)
        {
            const float parts[] = { c.r, c.g, c.b, c.a };
            char text[12] = "#";
            for (u32 i = 0; i < 4; i++)
            {
                if (!(parts[i] >= 0 && parts[i] <= 1))
                {
                    setParts(v, parts, 4);
                    return;
                }
                snprintf(text + 1 + i * 2, 3, "%02x", static_cast<unsigned>(parts[i] * 255.0f + 0.5f));
            }
            text[c.a == 1 ? 7 : 9] = 0;
            v->setText(text);
        }
        void write(JsonValue* v, JsonValue* other)
        {
            if (other == nullptr)
            {
                v->setNull();
            }
            else if (!v->setCopy(*other))
            {
                scriptError("too much inside one another");
            }
        }

        template <typename T>
        void jsonSet(const std::string& key, const T& value, JsonValue* j)
        {
            if (JsonValue* v = place(j->put(key)))
            {
                write(v, value);
            }
        }
        template <typename T>
        void jsonAdd(const T& value, JsonValue* j)
        {
            if (JsonValue* v = place(j->add()))
            {
                write(v, value);
            }
        }
        void jsonSetNumber(const std::string& key, double value, JsonValue* j) { jsonSet(key, value, j); }
        void jsonSetFlag(const std::string& key, bool value, JsonValue* j) { jsonSet(key, value, j); }
        void jsonSetText(const std::string& key, const std::string& value, JsonValue* j) { jsonSet(key, value, j); }
        void jsonSetVec3(const std::string& key, const Vec3& value, JsonValue* j) { jsonSet(key, value, j); }
        void jsonSetColor(const std::string& key, const Color& value, JsonValue* j) { jsonSet(key, value, j); }
        // The other may be this one, or have this one in it: it is taken as it was.
        void jsonSetJson(const std::string& key, JsonValue* value, JsonValue* j)
        {
            JsonValue* taken = makeJson();
            write(taken, value);
            jsonSet(key, taken, j);
            taken->release();
        }
        void jsonAddNumber(double value, JsonValue* j) { jsonAdd(value, j); }
        void jsonAddFlag(bool value, JsonValue* j) { jsonAdd(value, j); }
        void jsonAddText(const std::string& value, JsonValue* j) { jsonAdd(value, j); }
        void jsonAddVec3(const Vec3& value, JsonValue* j) { jsonAdd(value, j); }
        void jsonAddColor(const Color& value, JsonValue* j) { jsonAdd(value, j); }
        void jsonAddJson(JsonValue* value, JsonValue* j)
        {
            JsonValue* taken = makeJson();
            write(taken, value);
            jsonAdd(taken, j);
            taken->release();
        }
        void jsonRemove(const std::string& key, JsonValue* j) { j->remove(key.c_str()); }
        void jsonRemoveAt(u32 index, JsonValue* j) { j->removeAt(index); }

        // --- A screen's own files ---

        // "asset:rooms/first.json" in the screen "editor" is kept as "editor.rooms.first". Short, as the original Xbox's
        // names are.
        bool keptName(const Services& s, const std::string& name, std::string& out)
        {
            const size_t ending = name.size() > 11 ? name.size() - 5 : 0;
            if (ending == 0 || name.compare(0, 6, "asset:") != 0 || name.compare(ending, 5, ".json") != 0)
            {
                return false;
            }
            const char* slash = strrchr(s.folder, '/');
            out = std::string(slash ? slash + 1 : s.folder) + ".";
            for (size_t i = 6; i < ending; i++)
            {
                const char c = name[i];
                const bool plain = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '/';
                if (!plain)
                {
                    return false;
                }
                out += c == '/' ? '.' : c;
            }
            return out.size() <= 34;
        }

        std::string readText(const std::string& name)
        {
            Services& s = services();
            FileData file;
            if (!readOwn(s, name.c_str(), file))
            {
                scriptError(name + " can't be read: it must be asset: or global:, and there");
                return "";
            }
            return std::string(reinterpret_cast<const char*>(file.data), file.size);
        }

        bool writeText(const std::string& name, const std::string& text)
        {
            Services& s = services();
            std::string kept;
            char path[192];
            if (!keptName(s, name, kept) || !resolveAsset(s, name.c_str(), path, sizeof(path)))
            {
                scriptError("only asset: files that end in .json can be written, with short, plain names (" + name + ")");
                return false;
            }
            if (text.size() > kMaxFile)
            {
                scriptError("that is too long to write");
                return false;
            }
            if (s.save->readOnly())
            {
                return true;
            }
            return s.development && s.platform->writeFile(path, text.data(), text.size()) ? true : !s.development && s.platform->writeSave(kept.c_str(), text.data(), text.size());
        }
    }

    bool readOwn(const Services& s, const char* name, FileData& out)
    {
        std::string kept;
        char path[192];
        if (!s.development && keptName(s, name, kept) && s.platform->loadSave(kept.c_str(), out))
        {
            return true;
        }
        return strncmp(name, "http", 4) != 0 && resolveAsset(s, name, path, sizeof(path)) && s.platform->readFile(path, out, MemTag::General);
    }

    void bindJson(Binder& b)
    {
        b.refType("Json", 0, "Numbers, text and flags, in lists and by name: for files, and for what comes from the web. One that isn't there is null, and can be asked things all the same: `j[\"player\"][\"score\"].number()`.");
        b.behaviour(asBEHAVE_FACTORY, "Json@ f()", NX_FN(makeJson));
        b.behaviour(asBEHAVE_FACTORY, "Json@ f(const string &in text) explicit", NX_FN(makeJsonText));
        b.behaviour(asBEHAVE_ADDREF, "void f()", NX_METHOD(JsonValue, addRef));
        b.behaviour(asBEHAVE_RELEASE, "void f()", NX_METHOD(JsonValue, release));
        b.method("bool parse(const string &in text, string &out error)", NX_OBJ_LAST(jsonParse), "Reads it from text. False, with what was wrong, if it isn't JSON.");
        b.method("void copy(const Json@+ other)", NX_OBJ_LAST(jsonCopy), "Becomes the same as another.");
        b.method("string write(bool oneLine = false) const", NX_OBJ_LAST(jsonWrite), "The text of it.");
        b.method("Json@ opIndex(const string &in key)", NX_OBJ_LAST(jsonKey), "A part by name.");
        b.method("Json@ opIndex(uint index)", NX_OBJ_LAST(jsonIndex), "A part by number, from 0.");
        b.method("bool has(const string &in key) const", NX_OBJ_LAST(jsonHas), "Whether there is one of that name.");
        b.method("uint get_count() const property", NX_METHOD(JsonValue, count), "How many parts it has.");
        b.method("string key(uint index) const", NX_OBJ_LAST(jsonKeyAt), "The name of one.");
        b.method("bool get_isNull() const property", NX_OBJ_LAST(jsonIsNull), "Whether it is nothing, or isn't there.");
        b.method("bool get_isNumber() const property", NX_OBJ_LAST(jsonIsNumber), "Whether it is a number.");
        b.method("bool get_isText() const property", NX_OBJ_LAST(jsonIsText), "Whether it is text.");
        b.method("bool get_isFlag() const property", NX_OBJ_LAST(jsonIsFlag), "Whether it is true or false.");
        b.method("bool get_isArray() const property", NX_OBJ_LAST(jsonIsArray), "Whether its parts are in a list.");
        b.method("bool get_isObject() const property", NX_OBJ_LAST(jsonIsObject), "Whether its parts have names.");
        b.method("double number(double otherwise = 0) const", NX_OBJ_LAST(jsonNumber), "It as a number, or otherwise if it isn't one.");
        b.method("string text(const string &in otherwise = \"\") const", NX_OBJ_LAST(jsonText), "It as text.");
        b.method("bool flag(bool otherwise = false) const", NX_OBJ_LAST(jsonFlag), "It as true or false.");
        b.method("Vec3 vec3(const Vec3 &in otherwise = Vec3()) const", NX_OBJ_LAST(jsonVec3), "It as a place: [x, y, z].");
        b.method("Color color(const Color &in otherwise = White) const", NX_OBJ_LAST(jsonColor), "It as a colour: \"#rrggbb\", \"#rrggbbaa\" or [r, g, b, a].");
        b.method("void set(const string &in key, double value)", NX_OBJ_LAST(jsonSetNumber), "Gives it a part of that name, or changes the one there.");
        b.method("void set(const string &in key, bool value)", NX_OBJ_LAST(jsonSetFlag), "The same.");
        b.method("void set(const string &in key, const string &in value)", NX_OBJ_LAST(jsonSetText), "The same.");
        b.method("void set(const string &in key, const Vec3 &in value)", NX_OBJ_LAST(jsonSetVec3), "The same.");
        b.method("void set(const string &in key, const Color &in value)", NX_OBJ_LAST(jsonSetColor), "The same.");
        b.method("void set(const string &in key, Json@+ value)", NX_OBJ_LAST(jsonSetJson), "The same, with a copy of another.");
        b.method("void add(double value)", NX_OBJ_LAST(jsonAddNumber), "Puts one more at the end of a list.");
        b.method("void add(bool value)", NX_OBJ_LAST(jsonAddFlag), "The same.");
        b.method("void add(const string &in value)", NX_OBJ_LAST(jsonAddText), "The same.");
        b.method("void add(const Vec3 &in value)", NX_OBJ_LAST(jsonAddVec3), "The same.");
        b.method("void add(const Color &in value)", NX_OBJ_LAST(jsonAddColor), "The same.");
        b.method("void add(Json@+ value)", NX_OBJ_LAST(jsonAddJson), "The same, with a copy of another.");
        b.method("void remove(const string &in key)", NX_OBJ_LAST(jsonRemove), "Takes a part out.");
        b.method("void remove(uint index)", NX_OBJ_LAST(jsonRemoveAt), "The same, by number.");

        b.names("Assets");
        b.function("string read(const string &in name)", NX_FN(readText), "What is in a text file, at once. If the screen has written one of that name, that is what it is.");
        b.function("bool write(const string &in name, const string &in text)", NX_FN(writeText), "Writes one of the screen's own files: `asset:` something `.json`. While developing it is the file itself; otherwise it is kept with the saves, and read from there from then on. False if it couldn't be.");
        b.names("");
    }
}
