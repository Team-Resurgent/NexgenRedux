#include "platform/posix/platform_posix.h"
#include "base/log.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

namespace nx
{
    namespace
    {
        double now()
        {
            timespec t = {};
            clock_gettime(CLOCK_MONOTONIC, &t);
            return static_cast<double>(t.tv_sec) + static_cast<double>(t.tv_nsec) / 1e9;
        }

        bool readWhole(const char* path, FileData& out, MemTag tag)
        {
            FILE* file = fopen(path, "rb");
            if (file == nullptr)
            {
                return false;
            }
            struct stat info = {};
            u8* data = nullptr;
            size_t size = 0;
            // A folder opens too, but isn't a file.
            if (fstat(fileno(file), &info) == 0 && S_ISREG(info.st_mode) && info.st_size < 0x7FFFFFFF)
            {
                size = static_cast<size_t>(info.st_size);
                data = static_cast<u8*>(memAlloc(size + 1, tag));
            }
            const bool ok = data && fread(data, 1, size, file) == size;
            fclose(file);
            if (!ok)
            {
                memFree(data);
                return false;
            }
            out.take(data, size);
            return true;
        }

        // Each folder of a path that isn't there yet.
        void makeFolders(const char* path)
        {
            char part[1024];
            const size_t length = strlen(path);
            for (size_t i = 1; i <= length && length < sizeof(part); i++)
            {
                if (path[i] == '/' || path[i] == 0)
                {
                    memcpy(part, path, i);
                    part[i] = 0;
                    mkdir(part, 0755);
                }
            }
        }
    }

    // Written beside the old one and then swapped in, so a crash never leaves half a file.
    static bool swapIn(const char* full, const char* fresh, const char* name, const void* data, size_t size)
    {
        FILE* file = fopen(fresh, "wb");
        if (file == nullptr)
        {
            NX_ERROR("cannot write %s (%s)", name, strerror(errno));
            return false;
        }
        const bool ok = fwrite(data, 1, size, file) == size;
        return fclose(file) == 0 && ok && rename(fresh, full) == 0;
    }

    bool PlatformPosix::setFolders(const char* media, const char* saves)
    {
        m_start = now();
        const int a = snprintf(m_media, sizeof(m_media), "%s", media);
        const int b = snprintf(m_saves, sizeof(m_saves), "%s", saves);
        return a > 0 && b > 0 && static_cast<size_t>(a) < sizeof(m_media) && static_cast<size_t>(b) < sizeof(m_saves);
    }

    double PlatformPosix::seconds()
    {
        return now() - m_start;
    }

    void PlatformPosix::sleep(double seconds)
    {
        timespec t = {};
        t.tv_sec = static_cast<time_t>(seconds);
        t.tv_nsec = static_cast<long>((seconds - static_cast<double>(t.tv_sec)) * 1e9);
        nanosleep(&t, nullptr);
    }

    bool PlatformPosix::mediaPath(const char* path, char (&full)[kMaxPath]) const
    {
        const bool own = strncmp(path, "own", 3) == 0 && (path[3] == '/' || path[3] == 0);
        const int length = snprintf(full, sizeof(full), "%s/%s", own ? m_saves : m_media, path);
        return length > 0 && static_cast<size_t>(length) < sizeof(full);
    }

    bool PlatformPosix::savePath(const char* name, const char* suffix, char (&full)[kMaxPath]) const
    {
        const int length = snprintf(full, sizeof(full), "%s/%s.sav%s", m_saves, name, suffix);
        return length > 0 && static_cast<size_t>(length) < sizeof(full);
    }

    bool PlatformPosix::readFile(const char* path, FileData& out, MemTag tag)
    {
        char full[kMaxPath];
        if (!mediaPath(path, full) || !readWhole(full, out, tag))
        {
            NX_ERROR("cannot read %s (%s)", path, strerror(errno));
            return false;
        }
        return true;
    }

    u64 PlatformPosix::fileTime(const char* path)
    {
        char full[kMaxPath];
        struct stat info = {};
        if (!mediaPath(path, full) || stat(full, &info) != 0)
        {
            return 0;
        }
#if defined(__APPLE__)
        const timespec& time = info.st_mtimespec;
#else
        const timespec& time = info.st_mtim;
#endif
        return static_cast<u64>(time.tv_sec) * 1000000000u + static_cast<u64>(time.tv_nsec);
    }

    bool PlatformPosix::loadSave(const char* name, FileData& out)
    {
        char full[kMaxPath];
        return savePath(name, "", full) && readWhole(full, out, MemTag::General);
    }

    bool PlatformPosix::writeSave(const char* name, const void* data, size_t size)
    {
        char full[kMaxPath];
        char fresh[kMaxPath];
        if (!savePath(name, "", full) || !savePath(name, ".new", fresh))
        {
            return false;
        }
        makeFolders(m_saves);
        return swapIn(full, fresh, name, data, size);
    }

    void PlatformPosix::removeSave(const char* name)
    {
        char full[kMaxPath];
        if (savePath(name, "", full))
        {
            unlink(full);
        }
    }

    bool PlatformPosix::setSaveFolder(const char* path)
    {
        const int length = snprintf(m_saves, sizeof(m_saves), "%s", path);
        return length > 0 && static_cast<size_t>(length) < sizeof(m_saves);
    }

    void PlatformPosix::keepOwn(const char* from)
    {
        // Under names that are the same on every machine.
        const char* slash = strrchr(from, '/');
        std::string name;
        for (const char* c = slash ? slash + 1 : from; *c; c++)
        {
            name += plainLetter(*c);
        }
        struct stat info = {};
        if (stat(from, &info) != 0 || !S_ISREG(info.st_mode) || info.st_size > 64 * 1024 * 1024 || name.empty() || name.size() > 60 || name[0] == '.')
        {
            NX_WARN("a file that was dropped is too large, or isn't one");
            return;
        }
        FileData data;
        char folder[kMaxPath], full[kMaxPath + 64], fresh[kMaxPath + 70];
        if (!mediaPath("own", folder) || !readWhole(from, data, MemTag::General))
        {
            return;
        }
        makeFolders(folder);
        snprintf(full, sizeof(full), "%s/%s", folder, name.c_str());
        snprintf(fresh, sizeof(fresh), "%s.new", full);
        if (swapIn(full, fresh, name.c_str(), data.data, data.size))
        {
            m_dropped.push_back(name);
        }
    }

    bool PlatformPosix::takeDropped(std::string& name)
    {
        if (m_dropped.empty())
        {
            return false;
        }
        name = m_dropped.front();
        m_dropped.erase(m_dropped.begin());
        return true;
    }

    bool PlatformPosix::writeFile(const char* path, const void* data, size_t size)
    {
        char full[kMaxPath];
        char fresh[kMaxPath + 4];
        if (!mediaPath(path, full))
        {
            return false;
        }
        snprintf(fresh, sizeof(fresh), "%s.new", full);
        return swapIn(full, fresh, path, data, size);
    }

    void PlatformPosix::list(const char* path, bool folders, FolderCallback callback, void* user)
    {
        char full[kMaxPath];
        DIR* folder = mediaPath(path, full) ? opendir(full) : nullptr;
        if (folder == nullptr)
        {
            return;
        }
        while (const dirent* entry = readdir(folder))
        {
            char inside[kMaxPath + 256];
            struct stat info = {};
            snprintf(inside, sizeof(inside), "%s/%s", full, entry->d_name);
            if (entry->d_name[0] != '.' && stat(inside, &info) == 0 && (S_ISDIR(info.st_mode) != 0) == folders)
            {
                callback(entry->d_name, user);
            }
        }
        closedir(folder);
    }

    u64 PlatformPosix::totalMemory()
    {
        return static_cast<u64>(sysconf(_SC_PHYS_PAGES)) * static_cast<u64>(sysconf(_SC_PAGESIZE));
    }

    // From LC_ALL, LC_MESSAGES or LANG, as "en_GB.UTF-8": "en-GB".
    std::string PlatformPosix::locale()
    {
        const char* names[] = { "LC_ALL", "LC_MESSAGES", "LANG" };
        for (const char* name : names)
        {
            const char* value = getenv(name);
            if (value == nullptr || value[0] == 0)
            {
                continue;
            }
            std::string out;
            for (const char* c = value; *c && *c != '.' && *c != '@'; c++)
            {
                out += *c == '_' ? '-' : *c;
            }
            // C and POSIX, as "C.UTF-8", aren't a language: as if unset.
            if (out != "C" && out != "POSIX" && !out.empty())
            {
                return out;
            }
        }
        return std::string();
    }

    u32 PlatformPosix::processors()
    {
        const long count = sysconf(_SC_NPROCESSORS_ONLN);
        return count > 0 ? static_cast<u32>(count) : 1;
    }

    u64 PlatformPosix::freeMemory()
    {
#if defined(_SC_AVPHYS_PAGES)
        return static_cast<u64>(sysconf(_SC_AVPHYS_PAGES)) * static_cast<u64>(sysconf(_SC_PAGESIZE));
#else
        return 0;
#endif
    }
}
