#include "platform/windows/platform_windows.h"
#include "base/log.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <xinput.h>

#include <string.h>
#include <wchar.h>

namespace nx
{
    namespace
    {
        constexpr wchar_t kWindowClass[] = L"NexgenWindow";
        class PlatformWin32 final : public PlatformWindows
        {
        public:
            ~PlatformWin32() override
            {
                if (m_window)
                {
                    DestroyWindow(m_window);
                }
                UnregisterClassW(kWindowClass, GetModuleHandleW(nullptr));
            }

            bool init()
            {
                SetConsoleOutputCP(CP_UTF8);
                SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
                wchar_t base[kMaxPath];
                const DWORD length = GetModuleFileNameW(nullptr, base, kMaxPath);
                wchar_t* slash = length && length < kMaxPath ? wcsrchr(base, L'\\') : nullptr;
                wchar_t saves[kMaxPath];
                const DWORD found = GetEnvironmentVariableW(L"LOCALAPPDATA", saves, kMaxPath);
                if (slash == nullptr || found == 0 || found >= kMaxPath || wcscat_s(saves, L"\\Nexgen") != 0)
                {
                    NX_ERROR("cannot find the program's folder");
                    return false;
                }
                *slash = 0;
                return setFolders(base, saves);
            }

            bool openWindow(const WindowDesc& desc) override
            {
                WNDCLASSEXW wc = {};
                wc.cbSize = sizeof(wc);
                wc.lpfnWndProc = windowProc;
                wc.hInstance = GetModuleHandleW(nullptr);
                wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));  // the arrow
                wc.lpszClassName = kWindowClass;
                RegisterClassExW(&wc);

                wchar_t title[256] = L"";
                MultiByteToWideChar(CP_UTF8, 0, desc.title, -1, title, 256);

                RECT rect = { 0, 0, static_cast<LONG>(desc.width), static_cast<LONG>(desc.height) };
                AdjustWindowRectExForDpi(&rect, WS_OVERLAPPEDWINDOW, FALSE, 0, GetDpiForSystem());
                m_window = CreateWindowExW(0, kWindowClass, title, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                    rect.right - rect.left, rect.bottom - rect.top, nullptr, nullptr, wc.hInstance, this);
                if (m_window == nullptr)
                {
                    NX_ERROR("CreateWindow failed (%lu)", GetLastError());
                    return false;
                }
                DragAcceptFiles(m_window, TRUE);
                // How far the mouse moves, as it is told, whatever the pointer does.
                RAWINPUTDEVICE mouse = {};
                mouse.usUsagePage = 1;
                mouse.usUsage = 2;
                mouse.hwndTarget = m_window;
                RegisterRawInputDevices(&mouse, 1, sizeof(mouse));
                if (!desc.hidden)
                {
                    ShowWindow(m_window, SW_SHOW);
                }
                return true;
            }

            void* nativeWindow() override { return m_window; }

            bool pumpEvents() override
            {
                MSG message;
                while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
                {
                    if (message.message == WM_QUIT)
                    {
                        m_quit = true;
                    }
                    TranslateMessage(&message);
                    DispatchMessageW(&message);
                }
                readPads();
                keepMouse();
                return !m_quit;
            }

            const InputState& input() const override { return m_input; }

            u64 totalMemory() override { return memory().ullTotalPhys; }
            u64 freeMemory() override { return memory().ullAvailPhys; }
            const char* name() const override { return "Windows"; }

        private:
            static MEMORYSTATUSEX memory()
            {
                MEMORYSTATUSEX status = {};
                status.dwLength = sizeof(status);
                GlobalMemoryStatusEx(&status);
                return status;
            }

            void readPads()
            {
                // Asking about an empty port is slow, so those are only looked at now and then.
                const double now = seconds();
                const bool look = now >= m_nextPadSearch;
                if (look)
                {
                    m_nextPadSearch = now + 1.0;
                }
                for (u32 i = 0; i < kMaxPads; i++)
                {
                    if (!m_input.pads[i].connected && !look)
                    {
                        continue;
                    }
                    Gamepad pad;
                    XINPUT_STATE state = {};
                    if (XInputGetState(i, &state) == ERROR_SUCCESS)
                    {
                        static const struct { WORD mask; Button button; } map[] = {
                            { XINPUT_GAMEPAD_DPAD_UP, Button::Up }, { XINPUT_GAMEPAD_DPAD_DOWN, Button::Down },
                            { XINPUT_GAMEPAD_DPAD_LEFT, Button::Left }, { XINPUT_GAMEPAD_DPAD_RIGHT, Button::Right },
                            { XINPUT_GAMEPAD_START, Button::Start }, { XINPUT_GAMEPAD_BACK, Button::Back },
                            { XINPUT_GAMEPAD_LEFT_THUMB, Button::LeftStick }, { XINPUT_GAMEPAD_RIGHT_THUMB, Button::RightStick },
                            { XINPUT_GAMEPAD_A, Button::A }, { XINPUT_GAMEPAD_B, Button::B },
                            { XINPUT_GAMEPAD_X, Button::X }, { XINPUT_GAMEPAD_Y, Button::Y },
                            { XINPUT_GAMEPAD_LEFT_SHOULDER, Button::LeftShoulder }, { XINPUT_GAMEPAD_RIGHT_SHOULDER, Button::RightShoulder },
                        };
                        pad.connected = true;
                        for (const auto& m : map)
                        {
                            if (state.Gamepad.wButtons & m.mask)
                            {
                                pad.buttons |= 1u << static_cast<u32>(m.button);
                            }
                        }
                        pad.leftX = removeDeadZone(state.Gamepad.sThumbLX / 32767.0f);
                        pad.leftY = removeDeadZone(state.Gamepad.sThumbLY / 32767.0f);
                        pad.rightX = removeDeadZone(state.Gamepad.sThumbRX / 32767.0f);
                        pad.rightY = removeDeadZone(state.Gamepad.sThumbRY / 32767.0f);
                        pad.leftTrigger = state.Gamepad.bLeftTrigger / 255.0f;
                        pad.rightTrigger = state.Gamepad.bRightTrigger / 255.0f;
                    }
                    // A pad works only while the window is in front.
                    if (!m_focused)
                    {
                        const bool connected = pad.connected;
                        pad = {};
                        pad.connected = connected;
                    }
                    m_input.pads[i] = pad;
                }
            }

            void lockMouse(bool on) override
            {
                m_wantLocked = on;
                keepMouse();
            }

            // Hidden and in the middle while it is locked and the window is in front.
            void keepMouse()
            {
                const bool locked = m_wantLocked && m_focused;
                if (locked != m_locked)
                {
                    m_locked = locked;
                    ShowCursor(locked ? FALSE : TRUE);
                    if (!locked)
                    {
                        ClipCursor(nullptr);
                    }
                }
                if (!locked)
                {
                    return;
                }
                RECT inside;
                GetClientRect(m_window, &inside);
                POINT corners[2] = { { inside.left, inside.top }, { inside.right, inside.bottom } };
                MapWindowPoints(m_window, nullptr, corners, 2);
                const RECT clip = { corners[0].x, corners[0].y, corners[1].x, corners[1].y };
                ClipCursor(&clip);
                SetCursorPos((clip.left + clip.right) / 2, (clip.top + clip.bottom) / 2);
            }

            void mouse(u32 button, bool down)
            {
                m_input.mouseButtons[button] = down;
                if (down)
                {
                    SetCapture(m_window);
                }
                else if (!m_input.mouseButtons[0] && !m_input.mouseButtons[1] && !m_input.mouseButtons[2])
                {
                    ReleaseCapture();
                }
            }

            LRESULT handle(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
            {
                switch (message)
                {
                case WM_CLOSE:
                    m_quit = true;  // the window stays until the GPU has let go of it
                    return 0;
                case WM_SETFOCUS:
                    m_focused = true;
                    return 0;
                case WM_KILLFOCUS:
                    // Key-ups go elsewhere from here on, so nothing may stay held.
                    m_focused = false;
                    memset(m_input.keys, 0, sizeof(m_input.keys));
                    memset(m_input.mouseButtons, 0, sizeof(m_input.mouseButtons));
                    return 0;
                case WM_KEYDOWN:
                case WM_KEYUP:
                    m_input.keys[static_cast<size_t>(translateKey(static_cast<u32>(wParam)))] = message == WM_KEYDOWN;
                    m_input.keys[static_cast<size_t>(Key::None)] = false;
                    return 0;
                case WM_CHAR:
                    // In two halves outside the basic plane, which no font here has.
                    if (wParam < 0xD800 || wParam > 0xDFFF)
                    {
                        m_input.type(static_cast<u32>(wParam));
                    }
                    return 0;
                case WM_MOUSEMOVE:
                    m_input.hasMouse = true;
                    m_input.mouseX = static_cast<float>(static_cast<short>(LOWORD(lParam)));
                    m_input.mouseY = static_cast<float>(static_cast<short>(HIWORD(lParam)));
                    return 0;
                case WM_DROPFILES:
                {
                    const HDROP drop = reinterpret_cast<HDROP>(wParam);
                    const UINT count = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
                    for (UINT i = 0; i < count && i < 64; i++)
                    {
                        wchar_t path[kMaxPath];
                        if (DragQueryFileW(drop, i, path, kMaxPath))
                        {
                            keepOwn(path);
                        }
                    }
                    DragFinish(drop);
                    return 0;
                }
                case WM_INPUT:
                {
                    RAWINPUT raw;
                    UINT size = sizeof(raw);
                    if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, &raw, &size, sizeof(RAWINPUTHEADER)) != static_cast<UINT>(-1) && raw.header.dwType == RIM_TYPEMOUSE &&
                        !(raw.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE) && m_focused)
                    {
                        m_input.turnX += static_cast<float>(raw.data.mouse.lLastX);
                        m_input.turnY += static_cast<float>(raw.data.mouse.lLastY);
                    }
                    return DefWindowProcW(window, message, wParam, lParam);
                }
                case WM_MOUSEWHEEL:
                    m_input.wheel += static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) / WHEEL_DELTA;
                    return 0;
                case WM_LBUTTONDOWN: mouse(0, true); return 0;
                case WM_LBUTTONUP: mouse(0, false); return 0;
                case WM_RBUTTONDOWN: mouse(1, true); return 0;
                case WM_RBUTTONUP: mouse(1, false); return 0;
                case WM_MBUTTONDOWN: mouse(2, true); return 0;
                case WM_MBUTTONUP: mouse(2, false); return 0;
                default:
                    return DefWindowProcW(window, message, wParam, lParam);
                }
            }

            static LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
            {
                if (message == WM_NCCREATE)
                {
                    const CREATESTRUCTW* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
                    SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
                }
                PlatformWin32* self = reinterpret_cast<PlatformWin32*>(GetWindowLongPtrW(window, GWLP_USERDATA));
                return self ? self->handle(window, message, wParam, lParam) : DefWindowProcW(window, message, wParam, lParam);
            }

            HWND m_window = nullptr;
            InputState m_input;
            double m_nextPadSearch = 0;
            bool m_focused = false;
            bool m_wantLocked = false, m_locked = false;
            bool m_quit = false;
        };
    }

    Platform* createPlatform()
    {
        PlatformWin32* platform = create<PlatformWin32>(MemTag::General);
        if (platform && !platform->init())
        {
            destroy(platform);
            return nullptr;
        }
        return platform;
    }
}
