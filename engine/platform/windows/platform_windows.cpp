#include "platform/windows/platform_windows.h"
#include "base/log.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string.h>
#include <wchar.h>

// Only calls that UWP has too.
namespace nx
{
    namespace
    {
        // Logs nothing: for a save, not being there is normal.
        bool readWhole(const wchar_t* path, FileData& out, MemTag tag)
        {
            const HANDLE file = CreateFile2(path, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, nullptr);
            if (file == INVALID_HANDLE_VALUE)
            {
                return false;
            }
            LARGE_INTEGER size = {};
            GetFileSizeEx(file, &size);
            u8* data = size.QuadPart < 0x7FFFFFFF ? static_cast<u8*>(memAlloc(static_cast<size_t>(size.QuadPart) + 1, tag)) : nullptr;
            DWORD read = 0;
            const bool ok = data && ReadFile(file, data, static_cast<DWORD>(size.QuadPart), &read, nullptr) && read == size.QuadPart;
            CloseHandle(file);
            if (!ok)
            {
                memFree(data);
                return false;
            }
            out.take(data, read);
            return true;
        }
    }

    Key PlatformWindows::translateKey(u32 code)
    {
        if (code >= 'A' && code <= 'Z')
        {
            return static_cast<Key>(static_cast<u32>(Key::A) + (code - 'A'));
        }
        if (code >= '0' && code <= '9')
        {
            return static_cast<Key>(static_cast<u32>(Key::Num0) + (code - '0'));
        }
        switch (code)
        {
        case VK_UP: return Key::Up;
        case VK_DOWN: return Key::Down;
        case VK_LEFT: return Key::Left;
        case VK_RIGHT: return Key::Right;
        case VK_SPACE: return Key::Space;
        case VK_RETURN: return Key::Enter;
        case VK_ESCAPE: return Key::Escape;
        case VK_TAB: return Key::Tab;
        case VK_BACK: return Key::Backspace;
        case VK_SHIFT: return Key::Shift;
        case VK_CONTROL: return Key::Control;
        default: return Key::None;
        }
    }

    // Written beside the old one and then swapped in, so a crash never leaves half a file.
    static bool swapIn(const wchar_t* full, const wchar_t* fresh, const char* name, const void* data, size_t size)
    {
        const HANDLE file = CreateFile2(fresh, GENERIC_WRITE, 0, CREATE_ALWAYS, nullptr);
        if (file == INVALID_HANDLE_VALUE)
        {
            NX_ERROR("cannot write %s (%lu)", name, GetLastError());
            return false;
        }
        DWORD written = 0;
        const bool ok = WriteFile(file, data, static_cast<DWORD>(size), &written, nullptr) && written == size;
        CloseHandle(file);
        return ok && MoveFileExW(fresh, full, MOVEFILE_REPLACE_EXISTING);
    }

    bool PlatformWindows::setFolders(const wchar_t* base, const wchar_t* saves)
    {
        LARGE_INTEGER value;
        QueryPerformanceFrequency(&value);
        m_frequency = value.QuadPart;
        QueryPerformanceCounter(&value);
        m_start = value.QuadPart;
        return wcscpy_s(m_base, base) == 0 && wcscpy_s(m_saves, saves) == 0;
    }

    double PlatformWindows::seconds()
    {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        return static_cast<double>(now.QuadPart - m_start) / static_cast<double>(m_frequency);
    }

    void PlatformWindows::sleep(double seconds)
    {
        Sleep(static_cast<DWORD>(seconds * 1000.0));
    }

    bool PlatformWindows::readFile(const char* path, FileData& out, MemTag tag)
    {
        wchar_t full[kMaxPath];
        if (!mediaPath(path, L"", full) || !readWhole(full, out, tag))
        {
            NX_ERROR("cannot read %s (%lu)", path, GetLastError());
            return false;
        }
        return true;
    }

    u64 PlatformWindows::fileTime(const char* path)
    {
        wchar_t full[kMaxPath];
        WIN32_FILE_ATTRIBUTE_DATA data;
        if (!mediaPath(path, L"", full) || !GetFileAttributesExW(full, GetFileExInfoStandard, &data))
        {
            return 0;
        }
        return (static_cast<u64>(data.ftLastWriteTime.dwHighDateTime) << 32) | data.ftLastWriteTime.dwLowDateTime;
    }

    bool PlatformWindows::loadSave(const char* name, FileData& out)
    {
        wchar_t full[kMaxPath];
        return savePath(name, L"", full) && readWhole(full, out, MemTag::General);
    }

    bool PlatformWindows::writeSave(const char* name, const void* data, size_t size)
    {
        wchar_t full[kMaxPath];
        wchar_t fresh[kMaxPath];
        if (!savePath(name, L"", full) || !savePath(name, L".new", fresh))
        {
            return false;
        }
        CreateDirectoryW(m_saves, nullptr);
        return swapIn(full, fresh, name, data, size);
    }

    void PlatformWindows::removeSave(const char* name)
    {
        wchar_t full[kMaxPath];
        if (savePath(name, L"", full))
        {
            DeleteFileW(full);
        }
    }

    bool PlatformWindows::setSaveFolder(const char* path)
    {
        wchar_t full[kMaxPath];
        return MultiByteToWideChar(CP_UTF8, 0, path, -1, full, kMaxPath) > 1 && wcscpy_s(m_saves, full) == 0;
    }

    bool PlatformWindows::writeFile(const char* path, const void* data, size_t size)
    {
        wchar_t full[kMaxPath];
        wchar_t fresh[kMaxPath];
        return mediaPath(path, L"", full) && mediaPath(path, L".new", fresh) && swapIn(full, fresh, path, data, size);
    }

    void PlatformWindows::list(const char* path, bool folders, FolderCallback callback, void* user)
    {
        wchar_t full[kMaxPath];
        if (!mediaPath(path, L"/*", full))
        {
            return;
        }
        WIN32_FIND_DATAW found;
        const HANDLE search = FindFirstFileExW(full, FindExInfoBasic, &found, FindExSearchNameMatch, nullptr, 0);
        if (search == INVALID_HANDLE_VALUE)
        {
            return;
        }
        do
        {
            char name[MAX_PATH * 3];
            if (((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) == folders && found.cFileName[0] != L'.' &&
                WideCharToMultiByte(CP_UTF8, 0, found.cFileName, -1, name, sizeof(name), nullptr, nullptr))
            {
                callback(name, user);
            }
        } while (FindNextFileW(search, &found));
        FindClose(search);
    }

    void PlatformWindows::keepOwn(const wchar_t* from)
    {
        WIN32_FILE_ATTRIBUTE_DATA data;
        const wchar_t* last = from;
        for (const wchar_t* c = from; *c; c++)
        {
            last = *c == L'\\' || *c == L'/' ? c + 1 : last;
        }
        // Under names that are the same on every machine.
        std::string name;
        for (const wchar_t* c = last; *c; c++)
        {
            name += plainLetter(*c < 128 ? static_cast<char>(*c) : '_');
        }
        if (!GetFileAttributesExW(from, GetFileExInfoStandard, &data) || (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || data.nFileSizeHigh || data.nFileSizeLow > 64 * 1024 * 1024 || name.size() > 60 || name[0] == '.')
        {
            NX_WARN("a file that was dropped is too large, or isn't one");
            return;
        }
        wchar_t folder[kMaxPath], full[kMaxPath];
        if (swprintf(folder, kMaxPath, L"%ls\\own", m_saves) < 0 || swprintf(full, kMaxPath, L"%ls\\%hs", folder, name.c_str()) < 0)
        {
            return;
        }
        CreateDirectoryW(m_saves, nullptr);
        CreateDirectoryW(folder, nullptr);
        COPYFILE2_EXTENDED_PARAMETERS how = {};
        how.dwSize = sizeof(how);
        if (FAILED(CopyFile2(from, full, &how)))
        {
            NX_ERROR("cannot keep %s (%lu)", name.c_str(), GetLastError());
            return;
        }
        m_dropped.push_back(name);
    }

    std::string PlatformWindows::locale()
    {
        wchar_t name[LOCALE_NAME_MAX_LENGTH] = L"";
        char out[LOCALE_NAME_MAX_LENGTH * 3] = "";
        if (GetUserDefaultLocaleName(name, LOCALE_NAME_MAX_LENGTH) > 0)
        {
            WideCharToMultiByte(CP_UTF8, 0, name, -1, out, sizeof(out), nullptr, nullptr);
        }
        return out;
    }

    bool PlatformWindows::localTime(u32& hour, u32& minute, u32& second)
    {
        SYSTEMTIME t;
        GetLocalTime(&t);
        hour = t.wHour;
        minute = t.wMinute;
        second = t.wSecond;
        return true;
    }

    u32 PlatformWindows::processors()
    {
        SYSTEM_INFO info = {};
        GetNativeSystemInfo(&info);
        return info.dwNumberOfProcessors ? info.dwNumberOfProcessors : 1;
    }

    bool PlatformWindows::takeDropped(std::string& name)
    {
        if (m_dropped.empty())
        {
            return false;
        }
        name = m_dropped.front();
        m_dropped.erase(m_dropped.begin());
        return true;
    }

    bool PlatformWindows::savePath(const char* name, const wchar_t* suffix, wchar_t (&full)[kMaxPath]) const
    {
        return swprintf(full, kMaxPath, L"%ls\\%hs.sav%ls", m_saves, name, suffix) > 0;
    }

    bool PlatformWindows::mediaPath(const char* path, const wchar_t* suffix, wchar_t (&full)[kMaxPath]) const
    {
        wchar_t relative[kMaxPath];
        const bool own = strncmp(path, "own", 3) == 0 && (path[3] == '/' || path[3] == 0);
        if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, relative, kMaxPath) == 0 ||
            swprintf(full, kMaxPath, own ? L"%ls/%ls%ls" : L"%ls/media/%ls%ls", own ? m_saves : m_base, relative, suffix) < 0)
        {
            NX_ERROR("bad path: %s", path);
            return false;
        }
        return true;
    }
}
