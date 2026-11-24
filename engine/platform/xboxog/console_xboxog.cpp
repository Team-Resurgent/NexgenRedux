#include "platform/console.h"
#include "base/log.h"

#include <xtl.h>
#include <xboxkrnl/xboxkrnl.h>

#include <stdio.h>
#include <string.h>

namespace nx
{
    namespace
    {
        // Addresses on the bus.
        constexpr UCHAR kController = 0x20;   // which runs the fan, the light and the tray
        constexpr UCHAR kSettings = 0xA8;     // the memory of keys and settings

        bool read(UCHAR address, UCHAR command, ULONG& value, bool word = false)
        {
            value = 0;
            // Busy now and then: more than one thing uses the bus.
            for (u32 i = 0; i < 3; i++)
            {
                if (NT_SUCCESS(HalReadSMBusValue(address, command, word ? TRUE : FALSE, &value)))
                {
                    return true;
                }
            }
            return false;
        }

        bool write(UCHAR address, UCHAR command, ULONG value, bool word = false)
        {
            for (u32 i = 0; i < 3; i++)
            {
                if (NT_SUCCESS(HalWriteSMBusValue(address, command, word ? TRUE : FALSE, value)))
                {
                    return true;
                }
            }
            return false;
        }

        // Where a drive is, or nullptr for a letter that isn't one.
        const char* device(char letter)
        {
            switch (letter >= 'a' ? letter - 32 : letter)
            {
            case 'C': return "\\Device\\Harddisk0\\Partition2";
            case 'D': return "\\Device\\CdRom0";
            case 'E': return "\\Device\\Harddisk0\\Partition1";
            case 'F': return "\\Device\\Harddisk0\\Partition6";
            case 'G': return "\\Device\\Harddisk0\\Partition7";
            case 'X': return "\\Device\\Harddisk0\\Partition3";
            case 'Y': return "\\Device\\Harddisk0\\Partition4";
            case 'Z': return "\\Device\\Harddisk0\\Partition5";
            default: return nullptr;
            }
        }

        // Titles are given only their own drives to begin with. D is theirs, and left as it is.
        bool mount(char letter)
        {
            letter = static_cast<char>(letter >= 'a' ? letter - 32 : letter);
            const char* where = device(letter);
            if (where == nullptr || letter == 'D')
            {
                return where != nullptr;
            }
            char name[16];
            snprintf(name, sizeof(name), "\\??\\%c:", letter);
            OBJECT_STRING link, target;
            RtlInitAnsiString(reinterpret_cast<PANSI_STRING>(&link), name);
            RtlInitAnsiString(reinterpret_cast<PANSI_STRING>(&target), where);
            IoCreateSymbolicLink(&link, &target);   // fails if it is there already, which is as good
            return true;
        }

        bool setting(ULONG index, void* out, ULONG size)
        {
            ULONG type = 0, got = 0;
            return NT_SUCCESS(ExQueryNonVolatileSetting(index, &type, out, size, &got));
        }
    }

    bool consoleAvailable() { return true; }

    int consoleTemperature(bool processor)
    {
        ULONG value;
        return read(kController, processor ? 0x09 : 0x0A, value) && value > 0 && value < 128 ? static_cast<int>(value) : -1;
    }

    int consoleFan()
    {
        // In fiftieths.
        ULONG value;
        return read(kController, 0x10, value) && value <= 50 ? static_cast<int>(value) * 2 : -1;
    }

    bool consoleSetFan(int percent)
    {
        if (percent < 0)
        {
            return write(kController, 0x05, 0);
        }
        // Never so slow that it might as well be off.
        const int safe = percent < 20 ? 20 : percent > 100 ? 100 : percent;
        return write(kController, 0x06, static_cast<ULONG>(safe / 2)) && write(kController, 0x05, 1);
    }

    bool consoleSetLight(u8 pattern)
    {
        if (pattern == 0)
        {
            return write(kController, 0x07, 0);
        }
        return write(kController, 0x08, pattern) && write(kController, 0x07, 1);
    }

    Tray consoleTray()
    {
        ULONG state = 0;
        if (!NT_SUCCESS(HalReadSMCTrayState(&state, nullptr)))
        {
            return Tray::Unknown;
        }
        switch (state & 0x70)
        {
        case SMC_TRAY_STATE_OPEN: return Tray::Open;
        case SMC_TRAY_STATE_NO_MEDIA: return Tray::Empty;
        case SMC_TRAY_STATE_CLOSED:
        case SMC_TRAY_STATE_MEDIA_DETECT: return Tray::Closed;
        default: return Tray::Moving;
        }
    }

    bool consoleMoveTray(bool open)
    {
        return write(kController, 0x0C, open ? 0 : 1);
    }

    int consoleBusRead(u32 address, u32 command, bool word)
    {
        ULONG value;
        const UCHAR at = static_cast<UCHAR>(address & 0xFE);
        return address < 256 && command < 256 && at != kSettings && read(at, static_cast<UCHAR>(command), value, word) ? static_cast<int>(value) : -1;
    }

    bool consoleBusWrite(u32 address, u32 command, u32 value, bool word)
    {
        const UCHAR at = static_cast<UCHAR>(address & 0xFE);
        return address < 256 && command < 256 && at != kSettings && write(at, static_cast<UCHAR>(command), value, word);
    }

    std::string consoleText(ConsoleText what)
    {
        char text[64] = "";
        switch (what)
        {
        case ConsoleText::Kernel:
            snprintf(text, sizeof(text), "%u.%u.%u.%u", XboxKrnlVersion.Major, XboxKrnlVersion.Minor, XboxKrnlVersion.Build, XboxKrnlVersion.Qfe & 0x7FFF);
            break;
        case ConsoleText::Hardware:
            snprintf(text, sizeof(text), "GPU %02X, MCP %02X%s", XboxHardwareInfo.GpuRevision, XboxHardwareInfo.McpRevision, XboxHardwareInfo.Flags & XBOX_HW_FLAG_DEVKIT_KERNEL ? ", devkit" : "");
            break;
        case ConsoleText::Serial:
        {
            UCHAR serial[12] = {};
            if (setting(0x100, serial, sizeof(serial)))
            {
                memcpy(text, serial, sizeof(serial));
            }
            break;
        }
        case ConsoleText::Ethernet:
        {
            UCHAR a[6] = {};
            if (setting(0x101, a, sizeof(a)))
            {
                snprintf(text, sizeof(text), "%02X:%02X:%02X:%02X:%02X:%02X", a[0], a[1], a[2], a[3], a[4], a[5]);
            }
            break;
        }
        case ConsoleText::Region:
        {
            ULONG region = 0;
            if (setting(0x104, &region, sizeof(region)))
            {
                snprintf(text, sizeof(text), "%s", region & 0x80000000 ? "Any" : region == 1 ? "North America" : region == 2 ? "Japan" : region == 4 ? "Rest of world" : "Unknown");
            }
            break;
        }
        case ConsoleText::Video:
        {
            const DWORD standard = XGetVideoStandard();
            snprintf(text, sizeof(text), "%s", standard == XC_VIDEO_STANDARD_NTSC_M ? "NTSC" : standard == XC_VIDEO_STANDARD_NTSC_J ? "NTSC-J" : standard == XC_VIDEO_STANDARD_PAL_I ? "PAL" : "Unknown");
            break;
        }
        case ConsoleText::Cable:
        {
            static const char* const names[] = { "SCART", "Component", "VGA", "RF", "S-Video", "", "Composite" };
            const DWORD pack = XGetAVPack();
            snprintf(text, sizeof(text), "%s", pack < 7 ? names[pack] : "Unknown");
            break;
        }
        default: break;
        }
        return text;
    }

    bool consoleDrive(char letter, u32& total, u32& free)
    {
        total = free = 0;
        if (!mount(letter))
        {
            return false;
        }
        char root[8];
        snprintf(root, sizeof(root), "%c:\\", letter);
        ULARGE_INTEGER mine, all, left;
        if (!GetDiskFreeSpaceExA(root, &mine, &all, &left))
        {
            return false;
        }
        total = static_cast<u32>(all.QuadPart / (1024 * 1024));
        free = static_cast<u32>(left.QuadPart / (1024 * 1024));
        return true;
    }

    void consoleList(const char* path, bool folders, std::vector<std::string>& out)
    {
        if (path[0] == 0 || path[1] != ':' || !mount(path[0]))
        {
            return;
        }
        char pattern[280];
        const size_t length = strlen(path);
        snprintf(pattern, sizeof(pattern), "%s%s*", path, path[length - 1] == '\\' ? "" : "\\");
        WIN32_FIND_DATAA found;
        const HANDLE search = FindFirstFileA(pattern, &found);
        if (search == INVALID_HANDLE_VALUE)
        {
            return;
        }
        do
        {
            const bool folder = (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
            if (folder == folders && found.cFileName[0] != '.' && out.size() < 1000)
            {
                out.push_back(found.cFileName);
            }
        } while (FindNextFileA(search, &found));
        FindClose(search);
    }

    void consoleExit(ConsoleExit how)
    {
        switch (how)
        {
        case ConsoleExit::Firmware: HalReturnToFirmware(HalRebootRoutine); break;
        case ConsoleExit::Restart: HalReturnToFirmware(HalQuickRebootRoutine); break;
        case ConsoleExit::Off: HalInitiateShutdown(); break;
        case ConsoleExit::Dashboard:
        {
            LD_LAUNCH_DASHBOARD launch;
            memset(&launch, 0, sizeof(launch));
            launch.dwReason = XLD_LAUNCH_DASHBOARD_MAIN_MENU;
            XLaunchNewImageA(nullptr, reinterpret_cast<PLAUNCH_DATA>(&launch));
            break;
        }
        }
    }

    bool consoleLaunch(const char* path)
    {
        const char* file = strrchr(path, '\\');
        if (path[0] == 0 || path[1] != ':' || path[2] != '\\' || file == nullptr || strlen(path) > 250 || !mount(path[0]))
        {
            return false;
        }
        const DWORD attributes = GetFileAttributesA(path);
        if (attributes == 0xFFFFFFFF || (attributes & FILE_ATTRIBUTE_DIRECTORY))
        {
            return false;
        }
        // The kernel is told where by its own names, with the folder, which becomes D, before a semicolon.
        if (LaunchDataPage == nullptr)
        {
            LaunchDataPage = static_cast<PLAUNCH_DATA_PAGE>(MmAllocateContiguousMemory(4096));
        }
        if (LaunchDataPage == nullptr)
        {
            return false;
        }
        MmPersistContiguousMemory(LaunchDataPage, 4096, TRUE);
        memset(LaunchDataPage, 0, 4096);
        LaunchDataPage->Header.dwLaunchDataType = 0xFFFFFFFF;   // none
        const size_t folder = static_cast<size_t>(file - path);
        snprintf(LaunchDataPage->Header.szLaunchPath, sizeof(LaunchDataPage->Header.szLaunchPath), "%s%.*s;%s", device(path[0]), static_cast<int>(folder - 2), path + 2, file + 1);
        HalReturnToFirmware(HalQuickRebootRoutine);
        return true;
    }
}
