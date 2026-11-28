#include "assets/decode.h"
#include "bindings/script_refs.h"

#include <scriptarray/scriptarray.h>

#include <string.h>
#include <string>
#include <vector>

// Bytes as file formats lay them out: gzip and bare deflate, numbers and text packed into bytes and read back from
// them, and zip files read and written. All of it without the screen, so threads have it too.
namespace nx
{
    namespace
    {
        constexpr size_t kMostBytes = 64u << 20;

        std::string taken(u8* out, size_t size)
        {
            std::string result(reinterpret_cast<const char*>(out), out ? size : 0);
            freeImage(out);
            return result;
        }

        const u8* bytesOf(const std::string& s) { return reinterpret_cast<const u8*>(s.data()); }

        void put32(std::string& s, u32 v)
        {
            for (int i = 0; i < 4; i++)
            {
                s += static_cast<char>((v >> (i * 8)) & 0xFF);
            }
        }
        void put16(std::string& s, u32 v)
        {
            s += static_cast<char>(v & 0xFF);
            s += static_cast<char>((v >> 8) & 0xFF);
        }
        u32 get32(const u8* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | (static_cast<u32>(p[3]) << 24); }
        u32 get16(const u8* p) { return p[0] | (p[1] << 8); }

        // --- Squeezed ---

        std::string deflateText(const std::string& text, int level)
        {
            size_t size = 0;
            u8* out = deflateBytes(bytesOf(text), text.size(), size, level);
            return taken(out, size);
        }

        std::string inflateText(const std::string& text)
        {
            size_t size = 0;
            u8* out = inflateBytes(bytesOf(text), text.size(), size);
            if (out == nullptr)
            {
                scriptError("that isn't what deflate() makes");
                return std::string();
            }
            return taken(out, size);
        }

        std::string gzipText(const std::string& text, int level)
        {
            size_t size = 0;
            u8* out = deflateBytes(bytesOf(text), text.size(), size, level);
            if (out == nullptr)
            {
                return std::string();
            }
            // Its header: deflate, no name or time, from an unknown kind of machine.
            std::string result("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff", 10);
            result.append(reinterpret_cast<const char*>(out), size);
            freeImage(out);
            put32(result, crc32Of(bytesOf(text), text.size()));
            put32(result, static_cast<u32>(text.size()));
            return result;
        }

        std::string gunzipText(const std::string& text)
        {
            const u8* p = bytesOf(text);
            const size_t n = text.size();
            if (n < 18 || p[0] != 0x1f || p[1] != 0x8b || p[2] != 8)
            {
                scriptError("that isn't a gzip file");
                return std::string();
            }
            const u8 flags = p[3];
            size_t at = 10;
            if (flags & 4)
            {
                at += 2 + (at + 2 <= n ? get16(p + at) : 0);  // extra
            }
            for (int field = 8; field <= 16; field *= 2)
            {
                if (flags & field)  // a name, then a comment, each ending in a zero
                {
                    while (at < n && p[at] != 0)
                    {
                        at++;
                    }
                    at++;
                }
            }
            if (flags & 2)
            {
                at += 2;  // the header's own check
            }
            if (at + 8 > n)
            {
                scriptError("that gzip file is cut short");
                return std::string();
            }
            size_t size = 0;
            u8* out = inflateBytes(p + at, n - 8 - at, size);
            if (out == nullptr || crc32Of(out, size) != get32(p + n - 8))
            {
                freeImage(out);
                scriptError("that gzip file is broken");
                return std::string();
            }
            return taken(out, size);
        }

        u32 crcText(const std::string& text) { return crc32Of(bytesOf(text), text.size()); }

        // --- Packed ---

        struct Packed
        {
            int refs = 1;
            std::string bytes;
            size_t at = 0;
            bool big = false;
            bool whole = true;

            void addRef() { refs++; }
            void release()
            {
                if (--refs == 0)
                {
                    destroy(this);
                }
            }

            void write(u64 value, int count)
            {
                if (bytes.size() + count > kMostBytes)
                {
                    whole = false;
                    return;
                }
                for (int i = 0; i < count; i++)
                {
                    const int shift = (big ? count - 1 - i : i) * 8;
                    bytes += static_cast<char>((value >> shift) & 0xFF);
                }
            }

            u64 read(int count)
            {
                if (at + count > bytes.size())
                {
                    whole = false;
                    at = bytes.size();
                    return 0;
                }
                u64 value = 0;
                for (int i = 0; i < count; i++)
                {
                    const int shift = (big ? count - 1 - i : i) * 8;
                    value |= static_cast<u64>(static_cast<u8>(bytes[at + i])) << shift;
                }
                at += count;
                return value;
            }
        };

        Packed* makePacked() { return create<Packed>(MemTag::Script); }
        Packed* makePackedFrom(const std::string& bytes)
        {
            Packed* p = makePacked();
            if (p)
            {
                p->bytes = bytes;
            }
            return p;
        }

        void writeU8(u32 v, Packed* p) { p->write(v, 1); }
        void writeU16(u32 v, Packed* p) { p->write(v, 2); }
        void writeU32(u32 v, Packed* p) { p->write(v, 4); }
        void writeI8(int v, Packed* p) { p->write(static_cast<u32>(v), 1); }
        void writeI16(int v, Packed* p) { p->write(static_cast<u32>(v), 2); }
        void writeI32(int v, Packed* p) { p->write(static_cast<u32>(v), 4); }
        void writeI64(i64 v, Packed* p) { p->write(static_cast<u64>(v), 8); }
        void writeFloat(float v, Packed* p)
        {
            u32 bits;
            memcpy(&bits, &v, 4);
            p->write(bits, 4);
        }
        void writeDouble(double v, Packed* p)
        {
            u64 bits;
            memcpy(&bits, &v, 8);
            p->write(bits, 8);
        }
        void writeRaw(const std::string& text, Packed* p)
        {
            if (p->bytes.size() + text.size() > kMostBytes)
            {
                p->whole = false;
                return;
            }
            p->bytes += text;
        }
        void writeText(const std::string& text, Packed* p)
        {
            const size_t zero = text.find('\0');
            writeRaw(zero == std::string::npos ? text : text.substr(0, zero), p);
            p->write(0, 1);
        }
        void writeFixed(const std::string& text, u32 count, Packed* p)
        {
            std::string fixed = text.substr(0, count);
            fixed.resize(count, '\0');
            writeRaw(fixed, p);
        }

        u32 readU8(Packed* p) { return static_cast<u32>(p->read(1)); }
        u32 readU16(Packed* p) { return static_cast<u32>(p->read(2)); }
        u32 readU32(Packed* p) { return static_cast<u32>(p->read(4)); }
        int readI8(Packed* p) { return static_cast<int>(static_cast<int8_t>(p->read(1))); }
        int readI16(Packed* p) { return static_cast<int>(static_cast<i16>(p->read(2))); }
        int readI32(Packed* p) { return static_cast<int>(static_cast<u32>(p->read(4))); }
        i64 readI64(Packed* p) { return static_cast<i64>(p->read(8)); }
        float readFloat(Packed* p)
        {
            const u32 bits = static_cast<u32>(p->read(4));
            float v;
            memcpy(&v, &bits, 4);
            return v;
        }
        double readDouble(Packed* p)
        {
            const u64 bits = p->read(8);
            double v;
            memcpy(&v, &bits, 8);
            return v;
        }
        std::string readRaw(u32 count, Packed* p)
        {
            if (p->at + count > p->bytes.size())
            {
                p->whole = false;
                count = static_cast<u32>(p->bytes.size() - p->at);
            }
            std::string out = p->bytes.substr(p->at, count);
            p->at += count;
            return out;
        }
        std::string readText(Packed* p)
        {
            const size_t zero = p->bytes.find('\0', p->at);
            if (zero == std::string::npos)
            {
                p->whole = false;
                std::string out = p->bytes.substr(p->at);
                p->at = p->bytes.size();
                return out;
            }
            std::string out = p->bytes.substr(p->at, zero - p->at);
            p->at = zero + 1;
            return out;
        }
        std::string readFixed(u32 count, Packed* p)
        {
            std::string out = readRaw(count, p);
            const size_t zero = out.find('\0');
            return zero == std::string::npos ? out : out.substr(0, zero);
        }

        std::string packedBytes(const Packed* p) { return p->bytes; }
        u32 packedSize(const Packed* p) { return static_cast<u32>(p->bytes.size()); }
        u32 packedAt(const Packed* p) { return static_cast<u32>(p->at); }
        void packedSetAt(u32 at, Packed* p) { p->at = at < p->bytes.size() ? at : p->bytes.size(); }
        bool packedMore(const Packed* p) { return p->at < p->bytes.size(); }
        bool packedWhole(const Packed* p) { return p->whole; }
        bool packedBig(const Packed* p) { return p->big; }
        void packedSetBig(bool big, Packed* p) { p->big = big; }
        void packedClear(Packed* p)
        {
            p->bytes.clear();
            p->at = 0;
            p->whole = true;
        }

        // --- Zip files ---

        struct ZipEntry
        {
            std::string name;
            std::string data;   // squeezed, or as it is if method is 0
            u32 method = 0;     // 0 kept as it is, 8 deflated
            u32 crc = 0;
            u32 size = 0;       // as it is
        };

        struct Zip
        {
            int refs = 1;
            std::vector<ZipEntry> entries;
            bool failed = false;

            void addRef() { refs++; }
            void release()
            {
                if (--refs == 0)
                {
                    destroy(this);
                }
            }

            int find(const std::string& name) const
            {
                for (size_t i = 0; i < entries.size(); i++)
                {
                    if (entries[i].name == name)
                    {
                        return static_cast<int>(i);
                    }
                }
                return -1;
            }

            // The central directory at the end says what is where.
            bool open(const std::string& file)
            {
                const u8* p = bytesOf(file);
                const size_t n = file.size();
                if (n < 22)
                {
                    return false;
                }
                size_t end = n - 22;
                const size_t lowest = n > 22 + 65535 ? n - 22 - 65535 : 0;
                while (get32(p + end) != 0x06054b50)
                {
                    if (end == lowest)
                    {
                        return false;
                    }
                    end--;
                }
                const u32 count = get16(p + end + 10);
                size_t at = get32(p + end + 16);
                for (u32 i = 0; i < count; i++)
                {
                    if (at + 46 > n || get32(p + at) != 0x02014b50)
                    {
                        return false;
                    }
                    ZipEntry e;
                    e.method = get16(p + at + 10);
                    e.crc = get32(p + at + 16);
                    const u32 packed = get32(p + at + 20);
                    e.size = get32(p + at + 24);
                    const u32 nameLength = get16(p + at + 28), extra = get16(p + at + 30), comment = get16(p + at + 32);
                    const u32 local = get32(p + at + 42);
                    if (at + 46 + nameLength > n || local + 30 > n || get32(p + local) != 0x04034b50)
                    {
                        return false;
                    }
                    e.name.assign(reinterpret_cast<const char*>(p + at + 46), nameLength);
                    const size_t data = local + 30 + get16(p + local + 26) + get16(p + local + 28);
                    if (data + packed > n || (e.method != 0 && e.method != 8))
                    {
                        return false;
                    }
                    e.data.assign(reinterpret_cast<const char*>(p + data), packed);
                    at += 46 + nameLength + extra + comment;
                    if (e.name.empty() || e.name.back() != '/')  // folders hold nothing
                    {
                        entries.push_back(std::move(e));
                    }
                }
                return true;
            }
        };

        Zip* makeZip() { return create<Zip>(MemTag::Script); }
        Zip* makeZipFrom(const std::string& file)
        {
            Zip* z = makeZip();
            if (z && !z->open(file))
            {
                z->entries.clear();
                z->failed = true;
            }
            return z;
        }

        u32 zipCount(const Zip* z) { return static_cast<u32>(z->entries.size()); }
        bool zipFailed(const Zip* z) { return z->failed; }
        std::string zipName(u32 index, const Zip* z) { return index < z->entries.size() ? z->entries[index].name : std::string(); }
        bool zipHas(const std::string& name, const Zip* z) { return z->find(name) >= 0; }
        u32 zipSize(const std::string& name, const Zip* z)
        {
            const int i = z->find(name);
            return i >= 0 ? z->entries[i].size : 0;
        }

        std::string zipRead(const std::string& name, const Zip* z)
        {
            const int i = z->find(name);
            if (i < 0)
            {
                scriptError(name + " isn't in the zip");
                return std::string();
            }
            const ZipEntry& e = z->entries[i];
            std::string out;
            if (e.method == 0)
            {
                out = e.data;
            }
            else
            {
                size_t size = 0;
                u8* bytes = e.data.empty() ? nullptr : inflateBytes(bytesOf(e.data), e.data.size(), size);
                if (bytes == nullptr && e.size != 0)
                {
                    scriptError(name + " in the zip is broken");
                    return std::string();
                }
                out = taken(bytes, size);
            }
            if (out.size() != e.size || crc32Of(bytesOf(out), out.size()) != e.crc)
            {
                scriptError(name + " in the zip is broken");
                return std::string();
            }
            return out;
        }

        void zipAdd(const std::string& name, const std::string& bytes, int level, Zip* z)
        {
            if (name.empty() || name.size() > 0xFFFF || bytes.size() > kMostBytes)
            {
                scriptError("a zip's files have names, and are no larger than 64 MB");
                return;
            }
            ZipEntry e;
            e.name = name;
            e.size = static_cast<u32>(bytes.size());
            e.crc = crc32Of(bytesOf(bytes), bytes.size());
            e.method = 0;
            e.data = bytes;
            if (level > 0 && !bytes.empty())
            {
                size_t size = 0;
                u8* squeezed = deflateBytes(bytesOf(bytes), bytes.size(), size, level);
                if (squeezed && size < bytes.size())
                {
                    e.method = 8;
                    e.data.assign(reinterpret_cast<const char*>(squeezed), size);
                }
                freeImage(squeezed);
            }
            const int at = z->find(name);
            if (at >= 0)
            {
                z->entries[at] = std::move(e);
            }
            else
            {
                z->entries.push_back(std::move(e));
            }
        }

        void zipRemove(const std::string& name, Zip* z)
        {
            const int i = z->find(name);
            if (i >= 0)
            {
                z->entries.erase(z->entries.begin() + i);
            }
        }

        std::string zipBytes(const Zip* z)
        {
            std::string out, directory;
            for (const ZipEntry& e : z->entries)
            {
                const u32 local = static_cast<u32>(out.size());
                // Each file: a header, its name, its bytes. 1 January 1980, as nothing says when.
                const auto header = [&](std::string& s, bool central)
                {
                    put32(s, central ? 0x02014b50 : 0x04034b50);
                    if (central)
                    {
                        put16(s, 20);
                    }
                    put16(s, 20);
                    put16(s, 0x0800);  // names in UTF-8
                    put16(s, e.method);
                    put16(s, 0);
                    put16(s, 0x21);
                    put32(s, e.crc);
                    put32(s, static_cast<u32>(e.data.size()));
                    put32(s, e.size);
                    put16(s, static_cast<u32>(e.name.size()));
                    put16(s, 0);
                    if (central)
                    {
                        put16(s, 0);
                        put16(s, 0);
                        put16(s, 0);
                        put32(s, 0);
                        put32(s, local);
                    }
                    s += e.name;
                };
                header(out, false);
                out += e.data;
                header(directory, true);
            }
            const u32 start = static_cast<u32>(out.size());
            out += directory;
            put32(out, 0x06054b50);
            put16(out, 0);
            put16(out, 0);
            put16(out, static_cast<u32>(z->entries.size()));
            put16(out, static_cast<u32>(z->entries.size()));
            put32(out, static_cast<u32>(directory.size()));
            put32(out, start);
            put16(out, 0);
            return out;
        }
    }

    void bindPacking(Binder& b)
    {
        b.names("Data");
        b.function("string deflate(const string &in bytes, int level = 6)", NX_FN(deflateText), "Smaller, as bare deflate with nothing round it: what zip files, PNGs and some web servers hold.");
        b.function("string inflate(const string &in bytes)", NX_FN(inflateText), "As it was before deflate().");
        b.function("string gzip(const string &in bytes, int level = 6)", NX_FN(gzipText), "Smaller, as a .gz file: what gzip and web servers make.");
        b.function("string gunzip(const string &in bytes)", NX_FN(gunzipText), "As it was before gzip(), or what gzip made, checked.");
        b.function("uint crc32(const string &in bytes)", NX_FN(crcText), "The check that zip, gzip and PNG files keep: the same bytes give the same number.");
        b.names("");

        b.refType("Packed", 0, "Numbers and text laid out in bytes, as a file format or another program wants them, and read back in the same order. Little-endian, as most machines are, unless told otherwise.");
        b.behaviour(asBEHAVE_FACTORY, "Packed@ f()", NX_FN(makePacked));
        b.behaviour(asBEHAVE_FACTORY, "Packed@ f(const string &in bytes)", NX_FN(makePackedFrom));
        b.behaviour(asBEHAVE_ADDREF, "void f()", NX_METHOD(Packed, addRef));
        b.behaviour(asBEHAVE_RELEASE, "void f()", NX_METHOD(Packed, release));
        b.method("bool get_bigEndian() const property", NX_OBJ_LAST(packedBig), "Whether the largest byte of a number comes first, as networks and some old formats want.");
        b.method("void set_bigEndian(bool) property", NX_OBJ_LAST(packedSetBig), "Changes it, for what is written or read next.");
        b.method("void writeU8(uint value)", NX_OBJ_LAST(writeU8), "A byte, 0 to 255.");
        b.method("void writeU16(uint value)", NX_OBJ_LAST(writeU16), "Two bytes, 0 to 65535.");
        b.method("void writeU32(uint value)", NX_OBJ_LAST(writeU32), "Four bytes.");
        b.method("void writeI8(int value)", NX_OBJ_LAST(writeI8), "A byte, -128 to 127.");
        b.method("void writeI16(int value)", NX_OBJ_LAST(writeI16), "Two bytes, -32768 to 32767.");
        b.method("void writeI32(int value)", NX_OBJ_LAST(writeI32), "Four bytes.");
        b.method("void writeI64(int64 value)", NX_OBJ_LAST(writeI64), "Eight bytes.");
        b.method("void writeFloat(float value)", NX_OBJ_LAST(writeFloat), "Four bytes.");
        b.method("void writeDouble(double value)", NX_OBJ_LAST(writeDouble), "Eight bytes.");
        b.method("void writeText(const string &in text)", NX_OBJ_LAST(writeText), "Text and a zero byte after it.");
        b.method("void writeFixed(const string &in text, uint count)", NX_OBJ_LAST(writeFixed), "Text in so many bytes: cut short, or made up with zeros.");
        b.method("void write(const string &in bytes)", NX_OBJ_LAST(writeRaw), "Bytes as they are.");
        b.method("uint readU8()", NX_OBJ_LAST(readU8), "The next byte.");
        b.method("uint readU16()", NX_OBJ_LAST(readU16), "The next two.");
        b.method("uint readU32()", NX_OBJ_LAST(readU32), "The next four.");
        b.method("int readI8()", NX_OBJ_LAST(readI8), "The next byte, below 0 too.");
        b.method("int readI16()", NX_OBJ_LAST(readI16), "The next two, below 0 too.");
        b.method("int readI32()", NX_OBJ_LAST(readI32), "The next four, below 0 too.");
        b.method("int64 readI64()", NX_OBJ_LAST(readI64), "The next eight.");
        b.method("float readFloat()", NX_OBJ_LAST(readFloat), "The next four, as a number.");
        b.method("double readDouble()", NX_OBJ_LAST(readDouble), "The next eight, as a number.");
        b.method("string readText()", NX_OBJ_LAST(readText), "Text, up to a zero byte, which is passed over.");
        b.method("string readFixed(uint count)", NX_OBJ_LAST(readFixed), "Text in so many bytes, up to a zero if there is one.");
        b.method("string read(uint count)", NX_OBJ_LAST(readRaw), "So many bytes as they are.");
        b.method("string get_bytes() const property", NX_OBJ_LAST(packedBytes), "All of them, to keep, send or put in a file.");
        b.method("uint get_size() const property", NX_OBJ_LAST(packedSize), "How many bytes.");
        b.method("uint get_position() const property", NX_OBJ_LAST(packedAt), "Where the next read starts.");
        b.method("void set_position(uint) property", NX_OBJ_LAST(packedSetAt), "Reads from somewhere else: to skip, or go back.");
        b.method("bool get_more() const property", NX_OBJ_LAST(packedMore), "Whether there is anything left to read.");
        b.method("bool get_whole() const property", NX_OBJ_LAST(packedWhole), "False if more was read than there was, or written than 64 MB.");
        b.method("void clear()", NX_OBJ_LAST(packedClear), "No bytes, to start again.");

        b.refType("Zip", 0, "A zip file's files, read from its bytes or put together to make one: a level pack, a player's mod, a save with its pictures.");
        b.behaviour(asBEHAVE_FACTORY, "Zip@ f()", NX_FN(makeZip));
        b.behaviour(asBEHAVE_FACTORY, "Zip@ f(const string &in file)", NX_FN(makeZipFrom));
        b.behaviour(asBEHAVE_ADDREF, "void f()", NX_METHOD(Zip, addRef));
        b.behaviour(asBEHAVE_RELEASE, "void f()", NX_METHOD(Zip, release));
        b.method("bool get_failed() const property", NX_OBJ_LAST(zipFailed), "Whether what it was made from isn't a zip file it can read: one kept as it is or deflated, as nearly all are, and under 4 GB.");
        b.method("uint get_count() const property", NX_OBJ_LAST(zipCount), "How many files are in it; folders aren't counted.");
        b.method("string name(uint index) const", NX_OBJ_LAST(zipName), "One's name, with / between folders: `levels/first.json`.");
        b.method("bool has(const string &in name) const", NX_OBJ_LAST(zipHas), "Whether a file of that name is in it.");
        b.method("uint size(const string &in name) const", NX_OBJ_LAST(zipSize), "How large one is, as it is.");
        b.method("string read(const string &in name) const", NX_OBJ_LAST(zipRead), "What is in one, checked: text, or bytes for Assets::pixelsFrom, Json or Packed.");
        b.method("void add(const string &in name, const string &in bytes, int level = 6)", NX_OBJ_LAST(zipAdd), "Puts a file in, or replaces one of that name: deflated, unless level is 0 or that makes it no smaller.");
        b.method("void remove(const string &in name)", NX_OBJ_LAST(zipRemove), "Takes one out.");
        b.method("string get_bytes() const property", NX_OBJ_LAST(zipBytes), "The whole zip file, for any program to open.");
    }
}
