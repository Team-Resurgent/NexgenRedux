#include "platform/machines.h"

#include <xtl.h>

#include <stdio.h>

// The Xbox 360's own: its system controller (SMC) for temperatures, the tray and the ring of light, and the
// kernel's ways to leave.
extern "C"
{
    void __stdcall HalSendSMCMessage(void* request, void* response);
    void __stdcall HalReturnToFirmware(unsigned long routine);

    struct KernelVersion
    {
        unsigned short major, minor, build, qfe;
    };
    extern KernelVersion* XboxKrnlVersion;
}

namespace nx::xbox360
{
    namespace
    {
        // A message of sixteen bytes to the system controller, and its answer.
        void smc(u8 command, u8 a, u8 b, u8 (&answer)[16])
        {
            u8 request[16] = { command, a, b };
            for (u8& c : answer)
            {
                c = 0;
            }
            HalSendSMCMessage(request, answer);
        }

        constexpr u32 kPowerDown = 5, kRebootQuiesce = 6;
    }

    bool available() { return true; }

    int temperature(u32 which)
    {
        if (which > 3)
        {
            return -1;
        }
        u8 answer[16];
        smc(0x07, 0, 0, answer);
        // Eight bits of degrees and eight of their fraction, for each.
        const u32 at = 1 + which * 2;
        const int degrees = answer[at] + (answer[at + 1] >= 128 ? 1 : 0);
        return degrees > 0 ? degrees : -1;
    }

    Tray tray()
    {
        u8 answer[16];
        smc(0x0A, 0, 0, answer);
        switch (answer[1])
        {
        case 0x60: return Tray::Open;
        case 0x61: return Tray::Closed;
        case 0x62:
        case 0x63: return Tray::Moving;
        default: return Tray::Unknown;
        }
    }

    bool moveTray(bool open)
    {
        u8 answer[16];
        smc(0x8B, open ? 0x60 : 0x62, 0, answer);
        return true;
    }

    bool setLights(u8 pattern)
    {
        u8 answer[16];
        smc(0x99, pattern ? 1 : 0, pattern, answer);
        return true;
    }

    std::string text(Text what)
    {
        char out[96] = "";
        switch (what)
        {
        case Text::Kernel:
            if (XboxKrnlVersion)
            {
                snprintf(out, sizeof(out), "%u.%u.%u.%u", XboxKrnlVersion->major, XboxKrnlVersion->minor, XboxKrnlVersion->build, XboxKrnlVersion->qfe);
            }
            break;
        case Text::Region:
        {
            static const char* const regions[] = { "North America", "Asia", "Europe", "Rest of the world" };
            const DWORD region = XGetGameRegion() >> 8;
            snprintf(out, sizeof(out), "%s", region < 4 ? regions[region] : region == 0x7F ? "Development kit" : "Unknown");
            break;
        }
        case Text::Language:
        {
            static const char* const languages[] = { "", "English", "Japanese", "German", "French", "Spanish", "Italian", "Korean", "Chinese (traditional)", "Portuguese",
                                                      "Chinese (simplified)", "Polish", "Russian" };
            const DWORD language = XGetLanguage();
            snprintf(out, sizeof(out), "%s", language < sizeof(languages) / sizeof(languages[0]) ? languages[language] : "");
            break;
        }
        case Text::Video:
        {
            XVIDEO_MODE mode = {};
            XGetVideoMode(&mode);
            snprintf(out, sizeof(out), "%lux%lu%s, %d Hz%s", static_cast<unsigned long>(mode.dwDisplayWidth), static_cast<unsigned long>(mode.dwDisplayHeight),
                     mode.fIsInterlaced ? "i" : "p", static_cast<int>(mode.RefreshRate + 0.5f), mode.fIsWideScreen ? ", widescreen" : "");
            break;
        }
        case Text::Player:
            if (XUserGetName(0, out, sizeof(out)) != ERROR_SUCCESS)
            {
                out[0] = 0;
            }
            break;
        default:
            break;
        }
        return out;
    }

    bool launch(const char* path)
    {
        if (GetFileAttributesA(path) == static_cast<DWORD>(-1))  // not there
        {
            return false;
        }
        XLaunchNewImage(path, 0);
        return true;
    }

    void toDashboard() { XLaunchNewImage(XLAUNCH_KEYWORD_DASH, 0); }
    void restart() { HalReturnToFirmware(kRebootQuiesce); }
    void powerOff() { HalReturnToFirmware(kPowerDown); }
}
