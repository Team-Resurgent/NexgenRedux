#include "platform/platform.h"
#include "base/log.h"

#include <xtl.h>

#include <stdio.h>

// The Xbox 360: XInput for pads and keyboards, the title's own disc or folder (game:) for media.
namespace nx
{
    namespace
    {
        // Goes to the debug output, which a dev kit or Xenia shows.
        void debugSink(LogLevel level, const char* message)
        {
            static const char* const names[] = { "info", "warn", "error" };
            char line[1024];
            snprintf(line, sizeof(line), "[%s] %s\n", names[static_cast<int>(level)], message);
            OutputDebugStringA(line);
        }

        class PlatformXbox360 final : public Platform
        {
        public:
            bool init()
            {
                setLogSink(debugSink);
                QueryPerformanceFrequency(&m_frequency);
                QueryPerformanceCounter(&m_start);
                mountSaves();
                return true;
            }

            // Saves go in a package of the title's own on the hard disk, seen as save:. Opened for the first player if
            // one is signed in, or for nobody.
            void mountSaves()
            {
                XCONTENT_DATA content = {};
                content.DeviceID = 1;  // the hard disk
                content.dwContentType = XCONTENTTYPE_SAVEDGAME;
                const wchar_t* shown = L"Nexgen";
                for (u32 i = 0; shown[i] && i + 1 < XCONTENT_MAX_DISPLAYNAME_LENGTH; i++)
                {
                    content.szDisplayName[i] = shown[i];
                }
                snprintf(content.szFileName, sizeof(content.szFileName), "nexgen");
                for (const DWORD user : { DWORD(0), DWORD(XUSER_INDEX_NONE) })
                {
                    const DWORD result = XContentCreate(user, "save", &content, XCONTENTFLAG_OPENALWAYS, nullptr, nullptr, nullptr);
                    if (result == ERROR_SUCCESS)
                    {
                        m_saves = "save:";
                        m_mounted = true;
                        return;
                    }
                    if (user == XUSER_INDEX_NONE)
                    {
                        NX_WARN("saves: no package on the hard disk (%lu)", static_cast<unsigned long>(result));
                    }
                }
                // Beside the title, where that can be written.
                CreateDirectoryA("game:\\saves", nullptr);
            }

            // What is written is kept once the package is flushed.
            void keep()
            {
                if (m_mounted)
                {
                    XContentFlush("save", nullptr);
                }
            }

            // There is one screen, and the GPU device owns it.
            bool openWindow(const WindowDesc&) override { return true; }
            void* nativeWindow() override { return nullptr; }

            bool pumpEvents() override
            {
                for (DWORD i = 0; i < kMaxPads; i++)
                {
                    m_input.pads[i] = readPad(i);
                }
                readKeyboard();
                return true;
            }

            static Key keyOf(WORD vk)
            {
                if (vk >= 'A' && vk <= 'Z')
                {
                    return static_cast<Key>(static_cast<u32>(Key::A) + (vk - 'A'));
                }
                if (vk >= '0' && vk <= '9')
                {
                    return static_cast<Key>(static_cast<u32>(Key::Num0) + (vk - '0'));
                }
                switch (vk)
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
                case VK_SHIFT: case VK_LSHIFT: case VK_RSHIFT: return Key::Shift;
                case VK_CONTROL: case VK_LCONTROL: case VK_RCONTROL: return Key::Control;
                default: return Key::None;
                }
            }

            // A USB keyboard, or the chatpad on a controller.
            void readKeyboard()
            {
                XINPUT_KEYSTROKE stroke;
                for (u32 n = 0; n < 32 && XInputGetKeystroke(XUSER_INDEX_ANY, XINPUT_FLAG_KEYBOARD, &stroke) == ERROR_SUCCESS; n++)
                {
                    const bool up = (stroke.Flags & XINPUT_KEYSTROKE_KEYUP) != 0;
                    const Key key = keyOf(stroke.VirtualKey);
                    if (key != Key::None)
                    {
                        m_input.keys[static_cast<size_t>(key)] = !up;
                    }
                    if (!up && stroke.Unicode)
                    {
                        m_input.type(stroke.Unicode);
                    }
                }
            }

            void rumble(u32 pad, float heavy, float light) override
            {
                XINPUT_VIBRATION motors = {};
                motors.wLeftMotorSpeed = static_cast<WORD>(heavy * 65535);
                motors.wRightMotorSpeed = static_cast<WORD>(light * 65535);
                XInputSetState(pad, &motors);
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
                return readWhole(full, out, tag, true);
            }

            // When it was last written, so that live reload sees a script copied over the network.
            u64 fileTime(const char* path) override
            {
                char full[260];
                WIN32_FILE_ATTRIBUTE_DATA data;
                if (!mediaPath(path, "", full) || !GetFileAttributesExA(full, GetFileExInfoStandard, &data))
                {
                    return 0;
                }
                return (static_cast<u64>(data.ftLastWriteTime.dwHighDateTime) << 32) | data.ftLastWriteTime.dwLowDateTime;
            }

            bool loadSave(const char* name, FileData& out) override
            {
                char full[96];
                return savePath(name, "sav", full) && readWhole(full, out, MemTag::General, false);
            }

            bool writeSave(const char* name, const void* data, size_t size) override
            {
                char full[96];
                char fresh[96];
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
                const bool moved = MoveFileA(fresh, full) != 0;
                keep();
                return moved;
            }

            void removeSave(const char* name) override
            {
                char full[96];
                if (savePath(name, "sav", full))
                {
                    DeleteFileA(full);
                    keep();
                }
            }

            bool setSaveFolder(const char* path) override
            {
                m_saves = path;
                CreateDirectoryA(path, nullptr);
                return true;
            }

            void list(const char* path, bool folders, FolderCallback callback, void* user) override
            {
                char full[260];
                if (!mediaPath(path, "\\*", full))
                {
                    return;
                }
                WIN32_FIND_DATA found;
                const HANDLE search = FindFirstFile(full, &found);
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
                } while (FindNextFile(search, &found));
                FindClose(search);
            }

            u64 totalMemory() override { return memory().dwTotalPhys; }
            u64 freeMemory() override { return memory().dwAvailPhys; }
            const char* name() const override { return "Xbox 360"; }

        private:
            static bool readWhole(const char* full, FileData& out, MemTag tag, bool complain)
            {
                const HANDLE file = CreateFileA(full, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
                if (file == INVALID_HANDLE_VALUE)
                {
                    if (complain)
                    {
                        NX_ERROR("cannot open %s (%lu)", full, static_cast<unsigned long>(GetLastError()));
                    }
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

            bool savePath(const char* name, const char* ending, char (&full)[96]) const
            {
                return snprintf(full, sizeof(full), "%s\\%s.%s", m_saves, name, ending) < static_cast<int>(sizeof(full));
            }

            static MEMORYSTATUS memory()
            {
                MEMORYSTATUS status = {};
                status.dwLength = sizeof(status);
                GlobalMemoryStatus(&status);
                return status;
            }

            static bool mediaPath(const char* path, const char* suffix, char (&full)[260])
            {
                const int length = snprintf(full, sizeof(full), "game:\\media\\%s%s", path, suffix);
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

            static Gamepad readPad(DWORD index)
            {
                Gamepad pad;
                XINPUT_STATE state;
                if (XInputGetState(index, &state) != ERROR_SUCCESS)
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
                const WORD b = state.Gamepad.wButtons;
                set(b & XINPUT_GAMEPAD_DPAD_UP, Button::Up);
                set(b & XINPUT_GAMEPAD_DPAD_DOWN, Button::Down);
                set(b & XINPUT_GAMEPAD_DPAD_LEFT, Button::Left);
                set(b & XINPUT_GAMEPAD_DPAD_RIGHT, Button::Right);
                set(b & XINPUT_GAMEPAD_START, Button::Start);
                set(b & XINPUT_GAMEPAD_BACK, Button::Back);
                set(b & XINPUT_GAMEPAD_LEFT_THUMB, Button::LeftStick);
                set(b & XINPUT_GAMEPAD_RIGHT_THUMB, Button::RightStick);
                set(b & XINPUT_GAMEPAD_A, Button::A);
                set(b & XINPUT_GAMEPAD_B, Button::B);
                set(b & XINPUT_GAMEPAD_X, Button::X);
                set(b & XINPUT_GAMEPAD_Y, Button::Y);
                set(b & XINPUT_GAMEPAD_LEFT_SHOULDER, Button::LeftShoulder);
                set(b & XINPUT_GAMEPAD_RIGHT_SHOULDER, Button::RightShoulder);
                pad.leftTrigger = state.Gamepad.bLeftTrigger / 255.0f;
                pad.rightTrigger = state.Gamepad.bRightTrigger / 255.0f;
                pad.leftX = removeDeadZone(state.Gamepad.sThumbLX / 32767.0f);
                pad.leftY = removeDeadZone(state.Gamepad.sThumbLY / 32767.0f);
                pad.rightX = removeDeadZone(state.Gamepad.sThumbRX / 32767.0f);
                pad.rightY = removeDeadZone(state.Gamepad.sThumbRY / 32767.0f);
                return pad;
            }

            const char* m_saves = "game:\\saves";
            bool m_mounted = false;
            LARGE_INTEGER m_frequency = {};
            LARGE_INTEGER m_start = {};
            InputState m_input;
        };
    }

    Platform* createPlatform()
    {
        PlatformXbox360* platform = create<PlatformXbox360>(MemTag::General);
        if (platform && !platform->init())
        {
            destroy(platform);
            return nullptr;
        }
        return platform;
    }
}
