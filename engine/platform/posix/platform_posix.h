#pragma once

#include "platform/platform.h"

namespace nx
{
    // Time and files, which Linux and macOS share. Windows and input are in the classes made from it.
    class PlatformPosix : public Platform
    {
    public:
        double seconds() override;
        void sleep(double seconds) override;
        bool readFile(const char* path, FileData& out, MemTag tag) override;
        u64 fileTime(const char* path) override;
        bool loadSave(const char* name, FileData& out) override;
        bool writeSave(const char* name, const void* data, size_t size) override;
        void listFolders(const char* path, FolderCallback callback, void* user) override;
        u64 totalMemory() override;
        u64 freeMemory() override;

    protected:
        static constexpr size_t kMaxPath = 1024;

        // Where the media and the saves are.
        bool setFolders(const char* media, const char* saves);

    private:
        bool mediaPath(const char* path, char (&full)[kMaxPath]) const;
        bool savePath(const char* name, const char* suffix, char (&full)[kMaxPath]) const;

        char m_media[kMaxPath] = {};
        char m_saves[kMaxPath] = {};
        double m_start = 0;
    };
}
