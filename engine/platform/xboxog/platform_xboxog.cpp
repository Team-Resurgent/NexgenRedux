#include "platform/platform.h"
#include "base/log.h"

#include <xtl.h>
#include <xkbd.h>

#include <stdio.h>
#include <string.h>

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
                for (u32 i = 0; i < kPorts; i++)
                {
                    if (m_keyboards[i])
                    {
                        XInputClose(m_keyboards[i]);
                    }
                    if (m_mice[i])
                    {
                        XInputClose(m_mice[i]);
                    }
                }
            }

            bool init()
            {
                setLogSink(debugSink);
                QueryPerformanceFrequency(&m_frequency);
                QueryPerformanceCounter(&m_start);

                // A USB keyboard and mouse too, through an adapter in a controller port or its bottom slot.
                XDEVICE_PREALLOC_TYPE types[] = { { XDEVICE_TYPE_GAMEPAD, kMaxPads }, { XDEVICE_TYPE_DEBUG_KEYBOARD, 1 }, { XDEVICE_TYPE_DEBUG_MOUSE, 1 } };
                XInitDevices(3, types);
                XINPUT_DEBUG_KEYQUEUE_PARAMETERS queue = {};
                queue.dwFlags = XINPUT_DEBUG_KEYQUEUE_FLAG_KEYDOWN | XINPUT_DEBUG_KEYQUEUE_FLAG_KEYREPEAT | XINPUT_DEBUG_KEYQUEUE_FLAG_KEYUP;
                queue.dwQueueSize = 32;
                queue.dwRepeatDelay = 500;
                queue.dwRepeatInterval = 50;
                m_keyQueue = XInputDebugInitKeyboardQueue(&queue) == ERROR_SUCCESS;
                // The mouse stays on the screen, which is this size.
                const bool pal50 = XGetVideoStandard() == XC_VIDEO_STANDARD_PAL_I && !(XGetVideoFlags() & XC_VIDEO_FLAGS_PAL_60Hz);
                m_screenHeight = pal50 ? 576.0f : 480.0f;
                m_input.mouseX = 320;
                m_input.mouseY = m_screenHeight * 0.5f;
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
                        // What was being sent went with it; the next pad starts still.
                        m_rumble[i] = PadRumble();
                    }
                    m_input.pads[i] = m_pads[i] ? readPad(m_pads[i]) : Gamepad();
                    sendRumble(i);
                }
                readKeyboard();
                readMouse();
                return true;
            }

            void lockMouse(bool on) override { m_locked = on; }

            void rumble(u32 pad, float heavy, float light) override
            {
                if (pad < kMaxPads)
                {
                    m_rumble[pad].heavy = static_cast<WORD>(heavy * 65535);
                    m_rumble[pad].light = static_cast<WORD>(light * 65535);
                    m_rumble[pad].due = true;
                    sendRumble(pad);
                }
            }

            // The pad is told while it isn't still busy with what it was told last: until then the request waits, and
            // only the latest is sent.
            void sendRumble(u32 pad)
            {
                PadRumble& r = m_rumble[pad];
                if (!r.due || m_pads[pad] == nullptr || r.feedback.Header.dwStatus == ERROR_IO_PENDING)
                {
                    return;
                }
                memset(&r.feedback, 0, sizeof(r.feedback));
                r.feedback.Rumble.wLeftMotorSpeed = r.heavy;
                r.feedback.Rumble.wRightMotorSpeed = r.light;
                XInputSetState(m_pads[pad], &r.feedback);
                r.due = false;
            }
            bool drawsPointer() const override { return !m_locked; }

            // Ports, then the bottom slots of each: bits 0 to 3 and 16 to 19.
            static constexpr u32 kPorts = 8;
            static DWORD portBit(u32 i) { return 1u << (i < 4 ? i : i + 12); }

            static void openClose(XPP_DEVICE_TYPE* type, HANDLE (&handles)[kPorts], XINPUT_POLLING_PARAMETERS* poll)
            {
                const DWORD present = XGetDevices(type);
                for (u32 i = 0; i < kPorts; i++)
                {
                    const bool plugged = (present & portBit(i)) != 0;
                    if (plugged && handles[i] == nullptr)
                    {
                        handles[i] = XInputOpen(type, i % 4, i < 4 ? XDEVICE_NO_SLOT : XDEVICE_BOTTOM_SLOT, poll);
                    }
                    else if (!plugged && handles[i])
                    {
                        XInputClose(handles[i]);
                        handles[i] = nullptr;
                    }
                }
            }

            static Key keyOf(BYTE vk)
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

            void readKeyboard()
            {
                if (!m_keyQueue)
                {
                    return;
                }
                XINPUT_POLLING_PARAMETERS poll = {};
                poll.fAutoPoll = TRUE;
                poll.fInterruptOut = TRUE;
                poll.bInputInterval = 32;
                poll.bOutputInterval = 32;
                openClose(XDEVICE_TYPE_DEBUG_KEYBOARD, m_keyboards, &poll);
                // One queue for every keyboard.
                XINPUT_DEBUG_KEYSTROKE stroke;
                for (u32 n = 0; n < 32 && XInputDebugGetKeystroke(&stroke) == ERROR_SUCCESS; n++)
                {
                    const bool up = (stroke.Flags & XINPUT_DEBUG_KEYSTROKE_FLAG_KEYUP) != 0;
                    const Key key = keyOf(stroke.VirtualKey);
                    if (key != Key::None)
                    {
                        m_input.keys[static_cast<size_t>(key)] = !up;
                    }
                    if (!up && stroke.Ascii)
                    {
                        m_input.type(static_cast<u8>(stroke.Ascii));
                    }
                }
            }

            void readMouse()
            {
                openClose(XDEVICE_TYPE_DEBUG_MOUSE, m_mice, nullptr);
                for (u32 i = 0; i < kPorts; i++)
                {
                    XINPUT_STATE state;
                    if (m_mice[i] == nullptr || XInputGetState(m_mice[i], &state) != ERROR_SUCCESS)
                    {
                        continue;
                    }
                    const XINPUT_MOUSE& m = state.DebugMouse;
                    m_input.mouseButtons[0] = (m.bButtons & XINPUT_DEBUG_MOUSE_LEFT_BUTTON) != 0;
                    m_input.mouseButtons[1] = (m.bButtons & XINPUT_DEBUG_MOUSE_RIGHT_BUTTON) != 0;
                    m_input.mouseButtons[2] = (m.bButtons & XINPUT_DEBUG_MOUSE_MIDDLE_BUTTON) != 0;
                    if (state.dwPacketNumber == m_mousePacket[i])
                    {
                        continue;
                    }
                    // What it moved by since the last report.
                    m_mousePacket[i] = state.dwPacketNumber;
                    const float dx = static_cast<float>(m.cMickeysX), dy = static_cast<float>(m.cMickeysY);
                    m_input.turnX += dx;
                    m_input.turnY += dy;
                    m_input.wheel += static_cast<float>(m.cWheel);
                    const float x = m_input.mouseX + dx, y = m_input.mouseY + dy;
                    m_input.mouseX = x < 0 ? 0 : x > 639 ? 639 : x;
                    m_input.mouseY = y < 0 ? 0 : y > m_screenHeight - 1 ? m_screenHeight - 1 : y;
                    m_input.hasMouse = m_input.hasMouse || dx != 0 || dy != 0 || m.bButtons != 0;
                }
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
            struct PadRumble
            {
                XINPUT_FEEDBACK feedback = {};  // the pad reads it until its status is no longer pending
                WORD heavy = 0, light = 0;
                bool due = false;
            };
            PadRumble m_rumble[kMaxPads];
            HANDLE m_keyboards[kPorts] = {};
            HANDLE m_mice[kPorts] = {};
            DWORD m_mousePacket[kPorts] = {};
            bool m_keyQueue = false, m_locked = false;
            float m_screenHeight = 480;
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
