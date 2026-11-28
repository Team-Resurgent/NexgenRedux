#include "platform/machines.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>

#include <stdio.h>

// Windows on a desktop: its shell, its battery, its look and its clipboard.
namespace nx::windows
{
    namespace
    {
        std::wstring wide(const char* text)
        {
            const int length = MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);
            std::wstring out(length > 0 ? static_cast<size_t>(length) : 1u, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, text, -1, out.data(), length);
            out.resize(wcslen(out.c_str()));
            return out;
        }

        std::string narrow(const wchar_t* text)
        {
            const int length = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
            std::string out(length > 0 ? static_cast<size_t>(length) : 1u, '\0');
            WideCharToMultiByte(CP_UTF8, 0, text, -1, out.data(), length, nullptr, nullptr);
            out.resize(strlen(out.c_str()));
            return out;
        }

        bool shell(const wchar_t* verb, const wchar_t* file, const wchar_t* parameters)
        {
            return reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, verb, file, parameters, nullptr, SW_SHOWNORMAL)) > 32;
        }
    }

    bool available() { return true; }

    namespace
    {
        // What it really is: GetVersionEx says less to programs that don't declare the newest.
        bool version(OSVERSIONINFOW& info)
        {
            using RtlGetVersion = LONG(WINAPI*)(OSVERSIONINFOW*);
            info = {};
            info.dwOSVersionInfoSize = sizeof(info);
            const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
            const auto get = ntdll ? reinterpret_cast<RtlGetVersion>(reinterpret_cast<void*>(GetProcAddress(ntdll, "RtlGetVersion"))) : nullptr;
            return get != nullptr && get(&info) == 0;
        }
    }

    u32 build()
    {
        OSVERSIONINFOW info;
        return version(info) && info.dwMajorVersion >= 10 ? static_cast<u32>(info.dwBuildNumber) : 0;
    }

    std::string text(Text what)
    {
        switch (what)
        {
        case Text::Version:
        {
            OSVERSIONINFOW info;
            if (!version(info))
            {
                return "Windows";
            }
            char out[64];
            snprintf(out, sizeof(out), "Windows %s (build %lu)", info.dwMajorVersion == 10 && info.dwBuildNumber >= 22000 ? "11" : info.dwMajorVersion == 10 ? "10" : "",
                     static_cast<unsigned long>(info.dwBuildNumber));
            return out;
        }
        case Text::Computer:
        {
            wchar_t name[256];
            DWORD size = 256;
            return GetComputerNameExW(ComputerNameDnsHostname, name, &size) ? narrow(name) : std::string();
        }
        case Text::User:
        {
            wchar_t name[256];
            DWORD size = 256;
            return GetUserNameW(name, &size) ? narrow(name) : std::string();
        }
        default:
            return {};
        }
    }

    bool open(const char* what) { return shell(L"open", wide(what).c_str(), nullptr); }

    bool reveal(const char* path)
    {
        const std::wstring select = L"/select,\"" + wide(path) + L"\"";
        return shell(L"open", L"explorer.exe", select.c_str());
    }

    bool run(const char* program, const char* arguments) { return shell(L"open", wide(program).c_str(), arguments && *arguments ? wide(arguments).c_str() : nullptr); }

    int battery()
    {
        SYSTEM_POWER_STATUS status;
        if (!GetSystemPowerStatus(&status) || (status.BatteryFlag & 128) || status.BatteryLifePercent > 100)
        {
            return -1;
        }
        return status.BatteryLifePercent;
    }

    bool charging()
    {
        SYSTEM_POWER_STATUS status;
        return GetSystemPowerStatus(&status) && !(status.BatteryFlag & 128) && status.ACLineStatus == 1;
    }

    bool darkMode()
    {
        DWORD light = 1, size = sizeof(light);
        return RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &light, &size) ==
                   ERROR_SUCCESS &&
               light == 0;
    }

    std::string clipboard()
    {
        std::string out;
        if (!OpenClipboard(nullptr))
        {
            return out;
        }
        if (const HANDLE data = GetClipboardData(CF_UNICODETEXT))
        {
            if (const wchar_t* text = static_cast<const wchar_t*>(GlobalLock(data)))
            {
                out = narrow(text);
                GlobalUnlock(data);
            }
        }
        CloseClipboard();
        return out;
    }

    bool setClipboard(const char* text)
    {
        const std::wstring w = wide(text);
        const HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, (w.size() + 1) * sizeof(wchar_t));
        if (memory == nullptr)
        {
            return false;
        }
        memcpy(GlobalLock(memory), w.c_str(), (w.size() + 1) * sizeof(wchar_t));
        GlobalUnlock(memory);
        if (!OpenClipboard(nullptr))
        {
            GlobalFree(memory);
            return false;
        }
        EmptyClipboard();
        const bool fine = SetClipboardData(CF_UNICODETEXT, memory) != nullptr;
        CloseClipboard();
        if (!fine)
        {
            GlobalFree(memory);
        }
        return fine;
    }
}
