#include "platform/platform.h"
#include "base/log.h"

#include <xtl.h>

#include <stdio.h>

namespace nx
{
    namespace
    {
        // Goes to the kernel debug output, which a devkit or an emulator shows on the serial port.
        void debugSink(LogLevel level, const char* message)
        {
            static const char* const names[] = { "info", "warn", "error" };
            DbgPrint("[%s] %s\n", names[static_cast<int>(level)], message);
        }

        class PlatformXboxOG final : public Platform
        {
        public:
            ~PlatformXboxOG() override
            {
                for (HANDLE pad : m_pads)
                {
                    if (pad)
                    {
                        XInputClose(pad);
                    }
                }
            }

            bool init()
            {
                setLogSink(debugSink);
                QueryPerformanceFrequency(&m_frequency);
                QueryPerformanceCounter(&m_start);

                XDEVICE_PREALLOC_TYPE types[] = { { XDEVICE_TYPE_GAMEPAD, kMaxPads } };
                XInitDevices(1, types);
                return true;
            }

            // There is one screen, and the GPU device owns it.
            bool openWindow(const WindowDesc&) override { return true; }
            void* nativeWindow() override { return nullptr; }

            bool pumpEvents() override
            {
                const DWORD present = XGetDevices(XDEVICE_TYPE_GAMEPAD);
                for (DWORD i = 0; i < kMaxPads; i++)
                {
                    const bool plugged = (present & (1u << i)) != 0;
                    if (plugged && m_pads[i] == nullptr)
                    {
                        m_pads[i] = XInputOpen(XDEVICE_TYPE_GAMEPAD, i, XDEVICE_NO_SLOT, nullptr);
                    }
                    else if (!plugged && m_pads[i])
                    {
                        XInputClose(m_pads[i]);
                        m_pads[i] = nullptr;
                    }
                    m_input.pads[i] = m_pads[i] ? readPad(m_pads[i]) : Gamepad();
                }
                return true;
            }

            const InputState& input() const override { return m_input; }

            double seconds() override
            {
                LARGE_INTEGER now;
                QueryPerformanceCounter(&now);
                return static_cast<double>(now.QuadPart - m_start.QuadPart) / static_cast<double>(m_frequency.QuadPart);
            }

            void sleep(double seconds) override { Sleep(static_cast<DWORD>(seconds * 1000.0)); }

            bool readFile(const char* path, FileData& out, MemTag tag) override
            {
                char full[260];
                if (!mediaPath(path, "", full))
                {
                    return false;
                }
                const HANDLE file = CreateFileA(full, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
                if (file == INVALID_HANDLE_VALUE)
                {
                    NX_ERROR("cannot open %s (%lu)", full, static_cast<unsigned long>(GetLastError()));
                    return false;
                }
                const DWORD size = GetFileSize(file, nullptr);
                u8* data = size != 0xFFFFFFFF ? static_cast<u8*>(memAlloc(static_cast<size_t>(size) + 1, tag)) : nullptr;
                DWORD read = 0;
                const bool ok = data && ReadFile(file, data, size, &read, nullptr) && read == size;
                CloseHandle(file);
                if (!ok)
                {
                    NX_ERROR("cannot read %s", full);
                    memFree(data);
                    return false;
                }
                out.take(data, size);
                return true;
            }

            u64 fileTime(const char*) override { return 0; }

            bool loadSave(const char* name, FileData& out) override
            {
                char full[64];
                if (!savePath(name, "sav", full))
                {
                    return false;
                }
                const HANDLE file = CreateFileA(full, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
                if (file == INVALID_HANDLE_VALUE)
                {
                    return false;
                }
                const DWORD size = GetFileSize(file, nullptr);
                u8* data = size < 0x100000 ? static_cast<u8*>(memAlloc(static_cast<size_t>(size) + 1, MemTag::General)) : nullptr;
                DWORD read = 0;
                const bool ok = data && ReadFile(file, data, size, &read, nullptr) && read == size;
                CloseHandle(file);
                if (!ok)
                {
                    memFree(data);
                    return false;
                }
                out.take(data, size);
                return true;
            }

            bool writeSave(const char* name, const void* data, size_t size) override
            {
                char full[64];
                char fresh[64];
                if (!savePath(name, "sav", full) || !savePath(name, "new", fresh))
                {
                    return false;
                }
                const HANDLE file = CreateFileA(fresh, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
                if (file == INVALID_HANDLE_VALUE)
                {
                    NX_ERROR("cannot save %s (%lu)", name, static_cast<unsigned long>(GetLastError()));
                    return false;
                }
                DWORD written = 0;
                const bool ok = WriteFile(file, data, static_cast<DWORD>(size), &written, nullptr) && written == size;
                CloseHandle(file);
                if (!ok)
                {
                    return false;
                }
                DeleteFileA(full);
                return MoveFileA(fresh, full) != 0;
            }

            void removeSave(const char* name) override
            {
                char full[64];
                if (savePath(name, "sav", full))
                {
                    DeleteFileA(full);
                }
            }

            bool setSaveFolder(const char* path) override
            {
                m_saves = path;
                return true;
            }

            void list(const char* path, bool folders, FolderCallback callback, void* user) override
            {
                char full[260];
                if (!mediaPath(path, "\\*", full))
                {
                    return;
                }
                WIN32_FIND_DATAA found;
                const HANDLE search = FindFirstFileA(full, &found);
                if (search == INVALID_HANDLE_VALUE)
                {
                    return;
                }
                do
                {
                    if (((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) == folders && found.cFileName[0] != '.')
                    {
                        callback(found.cFileName, user);
                    }
                } while (FindNextFileA(search, &found));
                FindClose(search);
            }

            u64 totalMemory() override { return memory().dwTotalPhys; }
            u64 freeMemory() override { return memory().dwAvailPhys; }
            const char* name() const override { return "Original Xbox"; }

        private:
            bool savePath(const char* name, const char* ending, char (&full)[64]) const
            {
                return snprintf(full, sizeof(full), "%s\\%s.%s", m_saves, name, ending) < static_cast<int>(sizeof(full));
            }

            const char* m_saves = "T:";  // the title's own place on the hard disk

            static MEMORYSTATUS memory()
            {
                MEMORYSTATUS status = {};
                status.dwLength = sizeof(status);
                GlobalMemoryStatus(&status);
                return status;
            }

            static bool mediaPath(const char* path, const char* suffix, char (&full)[260])
            {
                const int length = snprintf(full, sizeof(full), "D:\\media\\%s%s", path, suffix);
                if (length < 0 || length >= static_cast<int>(sizeof(full)))
                {
                    NX_ERROR("path too long: %s", path);
                    return false;
                }
                for (char* c = full; *c; c++)
                {
                    *c = *c == '/' ? '\\' : *c;
                }
                return true;
            }

            static Gamepad readPad(HANDLE handle)
            {
                Gamepad pad;
                XINPUT_STATE state;
                if (XInputGetState(handle, &state) != ERROR_SUCCESS)
                {
                    return pad;
                }
                pad.connected = true;
                const auto set = [&](bool down, Button button)
                {
                    if (down)
                    {
                        pad.buttons |= 1u << static_cast<u32>(button);
                    }
                };
                const WORD digital = state.Gamepad.wButtons;
                set(digital & XINPUT_GAMEPAD_DPAD_UP, Button::Up);
                set(digital & XINPUT_GAMEPAD_DPAD_DOWN, Button::Down);
                set(digital & XINPUT_GAMEPAD_DPAD_LEFT, Button::Left);
                set(digital & XINPUT_GAMEPAD_DPAD_RIGHT, Button::Right);
                set(digital & XINPUT_GAMEPAD_START, Button::Start);
                set(digital & XINPUT_GAMEPAD_BACK, Button::Back);
                set(digital & XINPUT_GAMEPAD_LEFT_THUMB, Button::LeftStick);
                set(digital & XINPUT_GAMEPAD_RIGHT_THUMB, Button::RightStick);
                // The other buttons are pressure sensitive.
                const BYTE* analog = state.Gamepad.bAnalogButtons;
                set(analog[XINPUT_GAMEPAD_A] > XINPUT_GAMEPAD_MAX_CROSSTALK, Button::A);
                set(analog[XINPUT_GAMEPAD_B] > XINPUT_GAMEPAD_MAX_CROSSTALK, Button::B);
                set(analog[XINPUT_GAMEPAD_X] > XINPUT_GAMEPAD_MAX_CROSSTALK, Button::X);
                set(analog[XINPUT_GAMEPAD_Y] > XINPUT_GAMEPAD_MAX_CROSSTALK, Button::Y);
                set(analog[XINPUT_GAMEPAD_WHITE] > XINPUT_GAMEPAD_MAX_CROSSTALK, Button::LeftShoulder);
                set(analog[XINPUT_GAMEPAD_BLACK] > XINPUT_GAMEPAD_MAX_CROSSTALK, Button::RightShoulder);
                pad.leftTrigger = analog[XINPUT_GAMEPAD_LEFT_TRIGGER] / 255.0f;
                pad.rightTrigger = analog[XINPUT_GAMEPAD_RIGHT_TRIGGER] / 255.0f;
                pad.leftX = removeDeadZone(state.Gamepad.sThumbLX / 32767.0f);
                pad.leftY = removeDeadZone(state.Gamepad.sThumbLY / 32767.0f);
                pad.rightX = removeDeadZone(state.Gamepad.sThumbRX / 32767.0f);
                pad.rightY = removeDeadZone(state.Gamepad.sThumbRY / 32767.0f);
                return pad;
            }

            LARGE_INTEGER m_frequency = {};
            LARGE_INTEGER m_start = {};
            InputState m_input;
            HANDLE m_pads[kMaxPads] = {};
        };
    }

    Platform* createPlatform()
    {
        PlatformXboxOG* platform = create<PlatformXboxOG>(MemTag::General);
        if (platform && !platform->init())
        {
            destroy(platform);
            return nullptr;
        }
        return platform;
    }
}
