#include "platform/posix/platform_posix.h"
#include "base/log.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
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
        const int length = snprintf(full, sizeof(full), "%s/%s", m_media, path);
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

    // Written beside the old one and then swapped in, so a crash never leaves half a file.
    bool PlatformPosix::writeSave(const char* name, const void* data, size_t size)
    {
        char full[kMaxPath];
        char fresh[kMaxPath];
        if (!savePath(name, "", full) || !savePath(name, ".new", fresh))
        {
            return false;
        }
        makeFolders(m_saves);
        FILE* file = fopen(fresh, "wb");
        if (file == nullptr)
        {
            NX_ERROR("cannot save %s (%s)", name, strerror(errno));
            return false;
        }
        const bool ok = fwrite(data, 1, size, file) == size;
        return fclose(file) == 0 && ok && rename(fresh, full) == 0;
    }

    void PlatformPosix::listFolders(const char* path, FolderCallback callback, void* user)
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
            if (entry->d_name[0] != '.' && stat(inside, &info) == 0 && S_ISDIR(info.st_mode))
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

    u64 PlatformPosix::freeMemory()
    {
#if defined(_SC_AVPHYS_PAGES)
        return static_cast<u64>(sysconf(_SC_AVPHYS_PAGES)) * static_cast<u64>(sysconf(_SC_PAGESIZE));
#else
        return 0;
#endif
    }
}
