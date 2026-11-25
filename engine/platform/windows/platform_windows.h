#pragma once

#include "platform/platform.h"

namespace nx
{
    // Time and files, which the desktop and UWP share. Windows and input are in the classes made from it.
    class PlatformWindows : public Platform
    {
    public:
        double seconds() override;
        void sleep(double seconds) override;
        bool readFile(const char* path, FileData& out, MemTag tag) override;
        u64 fileTime(const char* path) override;
        bool loadSave(const char* name, FileData& out) override;
        bool writeSave(const char* name, const void* data, size_t size) override;
        void removeSave(const char* name) override;
        bool setSaveFolder(const char* path) override;
        bool writeFile(const char* path, const void* data, size_t size) override;
        void listFolders(const char* path, FolderCallback callback, void* user) override;

        // By virtual key code, which both use.
        static Key translateKey(u32 code);

    protected:
        static constexpr int kMaxPath = 1024;

        // The folders that hold "media" and the saves, without a slash at the end.
        bool setFolders(const wchar_t* base, const wchar_t* saves);

    private:
        bool mediaPath(const char* path, const wchar_t* suffix, wchar_t (&full)[kMaxPath]) const;
        bool savePath(const char* name, const wchar_t* suffix, wchar_t (&full)[kMaxPath]) const;

        wchar_t m_base[kMaxPath] = {};
        wchar_t m_saves[kMaxPath] = {};
        long long m_frequency = 1;
        long long m_start = 0;
    };
}
