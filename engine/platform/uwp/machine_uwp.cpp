#include "platform/machines.h"

#include <windows.h>
#include <gamingdeviceinformation.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.System.Profile.h>
#include <winrt/Windows.System.h>

#include <stdio.h>

// An Xbox One or Series, running the engine as a UWP app: which console it is, its system, its memory, and
// addresses it can open.
namespace nx::xboxone
{
    using namespace winrt;
    using namespace winrt::Windows::System;
    using namespace winrt::Windows::System::Profile;

    bool available() { return AnalyticsInfo::VersionInfo().DeviceFamily() == L"Windows.Xbox"; }

    std::string text(Text what)
    {
        switch (what)
        {
        case Text::Model:
        {
            GAMING_DEVICE_MODEL_INFORMATION info = {};
            if (!available() || FAILED(GetGamingDeviceModelInformation(&info)) || info.vendorId != GAMING_DEVICE_VENDOR_ID_MICROSOFT)
            {
                return {};
            }
            switch (info.deviceId)
            {
            case GAMING_DEVICE_DEVICE_ID_XBOX_ONE: return "Xbox One";
            case GAMING_DEVICE_DEVICE_ID_XBOX_ONE_S: return "Xbox One S";
            case GAMING_DEVICE_DEVICE_ID_XBOX_ONE_X: return "Xbox One X";
            case GAMING_DEVICE_DEVICE_ID_XBOX_ONE_X_DEVKIT: return "Xbox One X development kit";
            case GAMING_DEVICE_DEVICE_ID_XBOX_SERIES_S: return "Xbox Series S";
            case GAMING_DEVICE_DEVICE_ID_XBOX_SERIES_X: return "Xbox Series X";
            case GAMING_DEVICE_DEVICE_ID_XBOX_SERIES_X_DEVKIT: return "Xbox Series X development kit";
            default: return "Xbox";
            }
        }
        case Text::System:
        {
            // Four sixteen-bit numbers in one.
            const unsigned long long v = _wcstoui64(AnalyticsInfo::VersionInfo().DeviceFamilyVersion().c_str(), nullptr, 10);
            char out[48];
            snprintf(out, sizeof(out), "%llu.%llu.%llu.%llu", (v >> 48) & 0xFFFF, (v >> 32) & 0xFFFF, (v >> 16) & 0xFFFF, v & 0xFFFF);
            return out;
        }
        default:
            return {};
        }
    }

    u32 memoryLimit() { return static_cast<u32>(MemoryManager::AppMemoryUsageLimit() / (1024 * 1024)); }

    bool openUri(const char* uri)
    {
        const int length = MultiByteToWideChar(CP_UTF8, 0, uri, -1, nullptr, 0);
        std::wstring w(length > 0 ? static_cast<size_t>(length) : 1u, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, uri, -1, w.data(), length);
        w.resize(wcslen(w.c_str()));
        // It opens in a moment; whether it did is not waited for. An address that isn't one is refused.
        try
        {
            Launcher::LaunchUriAsync(winrt::Windows::Foundation::Uri(w));
            return true;
        }
        catch (const winrt::hresult_error&)
        {
            return false;
        }
    }
}
