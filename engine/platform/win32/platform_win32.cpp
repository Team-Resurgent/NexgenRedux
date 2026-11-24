#include "platform/platform.h"
#include "base/log.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <xinput.h>

#include <string.h>
#include <wchar.h>

namespace nx
{
    namespace
    {
        constexpr wchar_t kWindowClass[] = L"NexgenWindow";
        constexpr int kMaxPath = 1024;

        Key translateKey(WPARAM code)
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

        class PlatformWin32 final : public Platform
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
                QueryPerformanceFrequency(&m_frequency);
                QueryPerformanceCounter(&m_start);

                const DWORD length = GetModuleFileNameW(nullptr, m_base, kMaxPath);
                if (length == 0 || length >= kMaxPath)
                {
                    NX_ERROR("cannot find the program's folder");
                    return false;
                }
                if (wchar_t* slash = wcsrchr(m_base, L'\\'))
                {
                    slash[1] = 0;
                }
                return true;
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
                if (!desc.hidden)
                {
                    ShowWindow(m_window, SW_SHOW);
                }
                return true;
            }

            void* nativeWindow() override { return m_window; }

            bool pumpEvents() override
            {
                m_input.wheel = 0;
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
                return !m_quit;
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
                wchar_t full[kMaxPath];
                if (!mediaPath(path, L"", full))
                {
                    return false;
                }
                const HANDLE file = CreateFileW(full, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
                if (file == INVALID_HANDLE_VALUE)
                {
                    NX_ERROR("cannot open %s (%lu)", path, GetLastError());
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
                    NX_ERROR("cannot read %s", path);
                    memFree(data);
                    return false;
                }
                out.take(data, read);
                return true;
            }

            u64 fileTime(const char* path) override
            {
                wchar_t full[kMaxPath];
                WIN32_FILE_ATTRIBUTE_DATA data;
                if (!mediaPath(path, L"", full) || !GetFileAttributesExW(full, GetFileExInfoStandard, &data))
                {
                    return 0;
                }
                return (static_cast<u64>(data.ftLastWriteTime.dwHighDateTime) << 32) | data.ftLastWriteTime.dwLowDateTime;
            }

            bool loadSave(const char* name, FileData& out) override
            {
                wchar_t full[kMaxPath];
                if (!savePath(name, L"", full))
                {
                    return false;
                }
                const HANDLE file = CreateFileW(full, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
                if (file == INVALID_HANDLE_VALUE)
                {
                    return false;  // nothing saved yet
                }
                LARGE_INTEGER size = {};
                GetFileSizeEx(file, &size);
                u8* data = size.QuadPart < 0x1000000 ? static_cast<u8*>(memAlloc(static_cast<size_t>(size.QuadPart) + 1, MemTag::General)) : nullptr;
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

            // Written beside the old one and then swapped in, so a crash never leaves half a file.
            bool writeSave(const char* name, const void* data, size_t size) override
            {
                wchar_t full[kMaxPath];
                wchar_t fresh[kMaxPath];
                if (!savePath(name, L"", full) || !savePath(name, L".new", fresh))
                {
                    return false;
                }
                const HANDLE file = CreateFileW(fresh, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
                if (file == INVALID_HANDLE_VALUE)
                {
                    NX_ERROR("cannot save %s (%lu)", name, GetLastError());
                    return false;
                }
                DWORD written = 0;
                const bool ok = WriteFile(file, data, static_cast<DWORD>(size), &written, nullptr) && written == size;
                CloseHandle(file);
                return ok && MoveFileExW(fresh, full, MOVEFILE_REPLACE_EXISTING);
            }

            void listFolders(const char* path, FolderCallback callback, void* user) override
            {
                wchar_t full[kMaxPath];
                if (!mediaPath(path, L"/*", full))
                {
                    return;
                }
                WIN32_FIND_DATAW found;
                const HANDLE search = FindFirstFileW(full, &found);
                if (search == INVALID_HANDLE_VALUE)
                {
                    return;
                }
                do
                {
                    char name[MAX_PATH * 3];
                    if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && found.cFileName[0] != L'.' &&
                        WideCharToMultiByte(CP_UTF8, 0, found.cFileName, -1, name, sizeof(name), nullptr, nullptr))
                    {
                        callback(name, user);
                    }
                } while (FindNextFileW(search, &found));
                FindClose(search);
            }

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

            static bool savePath(const char* name, const wchar_t* suffix, wchar_t (&full)[kMaxPath])
            {
                wchar_t folder[kMaxPath];
                const DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", folder, kMaxPath);
                if (length == 0 || length >= kMaxPath || swprintf(full, kMaxPath, L"%ls\\Nexgen", folder) < 0)
                {
                    return false;
                }
                CreateDirectoryW(full, nullptr);
                return swprintf(full, kMaxPath, L"%ls\\Nexgen\\%hs.sav%ls", folder, name, suffix) > 0;
            }

            bool mediaPath(const char* path, const wchar_t* suffix, wchar_t (&full)[kMaxPath]) const
            {
                wchar_t relative[kMaxPath];
                if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, relative, kMaxPath) == 0 ||
                    swprintf(full, kMaxPath, L"%lsmedia/%ls%ls", m_base, relative, suffix) < 0)
                {
                    NX_ERROR("bad path: %s", path);
                    return false;
                }
                return true;
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
                    m_input.keys[static_cast<size_t>(translateKey(wParam))] = message == WM_KEYDOWN;
                    m_input.keys[static_cast<size_t>(Key::None)] = false;
                    return 0;
                case WM_MOUSEMOVE:
                    m_input.hasMouse = true;
                    m_input.mouseX = static_cast<float>(static_cast<short>(LOWORD(lParam)));
                    m_input.mouseY = static_cast<float>(static_cast<short>(HIWORD(lParam)));
                    return 0;
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
            LARGE_INTEGER m_frequency = {};
            LARGE_INTEGER m_start = {};
            InputState m_input;
            double m_nextPadSearch = 0;
            wchar_t m_base[kMaxPath] = {};
            bool m_focused = false;
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
