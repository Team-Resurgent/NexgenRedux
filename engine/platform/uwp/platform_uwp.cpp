#include "gpu/d3d11/uwp_window.h"
#include "platform/windows/platform_windows.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <winrt/Windows.ApplicationModel.Activation.h>
#include <winrt/Windows.ApplicationModel.Core.h>
#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Gaming.Input.h>
#include <winrt/Windows.Graphics.Display.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.System.Profile.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.UI.Core.h>
#include <winrt/Windows.UI.Input.h>
#include <winrt/Windows.UI.ViewManagement.h>

#include <stdio.h>
#include <string.h>

// The app's own main, which its build renames: here the system calls us, not the other way round.
int nxAppMain(int argc, char** argv);

using namespace winrt;
using namespace winrt::Windows::ApplicationModel;
using namespace winrt::Windows::ApplicationModel::Activation;
using namespace winrt::Windows::ApplicationModel::Core;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Graphics::Display;
using namespace winrt::Windows::System;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::UI::ViewManagement;
namespace gaming = winrt::Windows::Gaming::Input;

namespace nx
{
    namespace
    {
        class PlatformUwp final : public PlatformWindows
        {
        public:
            ~PlatformUwp() override
            {
                if (m_window)
                {
                    m_window.KeyDown(m_keyDown);
                    m_window.CharacterReceived(m_typed);
                    m_window.KeyUp(m_keyUp);
                    m_window.PointerMoved(m_pointerMoved);
                    m_window.PointerPressed(m_pointerPressed);
                    m_window.PointerReleased(m_pointerReleased);
                    m_window.PointerWheelChanged(m_wheel);
                    m_window.Activated(m_activated);
                    m_window.Closed(m_closed);
                    SystemNavigationManager::GetForCurrentView().BackRequested(m_back);
                }
            }

            bool init()
            {
                const hstring base = Package::Current().InstalledLocation().Path();
                const hstring saves = winrt::Windows::Storage::ApplicationData::Current().LocalFolder().Path();
                return setFolders(base.c_str(), saves.c_str());
            }

            bool openWindow(const WindowDesc& desc) override
            {
                m_window = CoreWindow::GetForCurrentThread();
                if (!m_window)
                {
                    return false;
                }
                // On a television, the whole screen: nothing important is drawn at its edges.
                const ApplicationView view = ApplicationView::GetForCurrentView();
                view.SetDesiredBoundsMode(ApplicationViewBoundsMode::UseCoreWindow);
                view.TryResizeView({ static_cast<float>(desc.width), static_cast<float>(desc.height) });

                const auto key = [this](const KeyEventArgs& args, bool down)
                {
                    const Key k = translateKey(static_cast<u32>(args.VirtualKey()));
                    m_input.keys[static_cast<size_t>(k)] = down;
                    m_input.keys[static_cast<size_t>(Key::None)] = false;
                };
                m_keyDown = m_window.KeyDown([key](const CoreWindow&, const KeyEventArgs& args) { key(args, true); });
                m_keyUp = m_window.KeyUp([key](const CoreWindow&, const KeyEventArgs& args) { key(args, false); });
                m_typed = m_window.CharacterReceived([this](const CoreWindow&, const CharacterReceivedEventArgs& args)
                {
                    m_input.type(args.KeyCode());
                });
                const auto pointer = [this](const CoreWindow&, const PointerEventArgs& args)
                {
                    const auto point = args.CurrentPoint();
                    const float scale = pixelScale();
                    m_input.hasMouse = true;
                    m_input.mouseX = point.Position().X * scale;
                    m_input.mouseY = point.Position().Y * scale;
                    const auto properties = point.Properties();
                    m_input.mouseButtons[0] = properties.IsLeftButtonPressed();
                    m_input.mouseButtons[1] = properties.IsRightButtonPressed();
                    m_input.mouseButtons[2] = properties.IsMiddleButtonPressed();
                };
                m_pointerMoved = m_window.PointerMoved(pointer);
                m_pointerPressed = m_window.PointerPressed(pointer);
                m_pointerReleased = m_window.PointerReleased(pointer);
                m_wheel = m_window.PointerWheelChanged([this](const CoreWindow&, const PointerEventArgs& args)
                {
                    m_input.wheel += static_cast<float>(args.CurrentPoint().Properties().MouseWheelDelta()) / 120.0f;
                });
                m_activated = m_window.Activated([this](const CoreWindow&, const WindowActivatedEventArgs& args)
                {
                    m_focused = args.WindowActivationState() != CoreWindowActivationState::Deactivated;
                    if (!m_focused)
                    {
                        // Key-ups go elsewhere from here on, so nothing may stay held.
                        memset(m_input.keys, 0, sizeof(m_input.keys));
                        memset(m_input.mouseButtons, 0, sizeof(m_input.mouseButtons));
                    }
                });
                m_closed = m_window.Closed([this](const CoreWindow&, const CoreWindowEventArgs&) { m_quit = true; });
                // B is a button like any other, not a way out of the app.
                m_back = SystemNavigationManager::GetForCurrentView().BackRequested([](const IInspectable&, const BackRequestedEventArgs& args) { args.Handled(true); });

                m_native.window = reinterpret_cast<::IUnknown*>(get_abi(m_window));
                m_native.user = this;
                m_native.size = [](void* self, u32& width, u32& height) { static_cast<PlatformUwp*>(self)->pixelSize(width, height); };
                m_window.Activate();
                m_window.Dispatcher().ProcessEvents(CoreProcessEventsOption::ProcessAllIfPresent);
                return true;
            }

            void* nativeWindow() override { return m_window ? &m_native : nullptr; }

            bool pumpEvents() override
            {
                if (m_window)
                {
                    m_window.Dispatcher().ProcessEvents(CoreProcessEventsOption::ProcessAllIfPresent);
                }
                readPads();
                return !m_quit;
            }

            void rumble(u32 pad, float heavy, float light) override
            {
                const auto pads = gaming::Gamepad::Gamepads();
                if (pad < pads.Size())
                {
                    gaming::GamepadVibration motors = {};
                    motors.LeftMotor = heavy;
                    motors.RightMotor = light;
                    pads.GetAt(pad).Vibration(motors);
                }
            }

            const InputState& input() const override { return m_input; }

            u64 totalMemory() override { return MemoryManager::AppMemoryUsageLimit(); }

            u64 freeMemory() override
            {
                const u64 used = MemoryManager::AppMemoryUsage();
                const u64 limit = MemoryManager::AppMemoryUsageLimit();
                return limit > used ? limit - used : 0;
            }

            const char* name() const override { return m_xbox ? "Xbox" : "Windows UWP"; }

        private:
            float pixelScale() const
            {
                return static_cast<float>(DisplayInformation::GetForCurrentView().RawPixelsPerViewPixel());
            }

            void pixelSize(u32& width, u32& height)
            {
                const Rect bounds = m_window.Bounds();
                const float scale = pixelScale();
                width = static_cast<u32>(bounds.Width * scale + 0.5f);
                height = static_cast<u32>(bounds.Height * scale + 0.5f);
            }

            void readPads()
            {
                const auto pads = gaming::Gamepad::Gamepads();
                const u32 count = pads.Size() < kMaxPads ? pads.Size() : kMaxPads;
                for (u32 i = 0; i < kMaxPads; i++)
                {
                    Gamepad pad;
                    pad.connected = i < count;
                    // A pad works only while the window is in front.
                    if (pad.connected && m_focused)
                    {
                        static const struct { gaming::GamepadButtons mask; Button button; } map[] = {
                            { gaming::GamepadButtons::DPadUp, Button::Up }, { gaming::GamepadButtons::DPadDown, Button::Down },
                            { gaming::GamepadButtons::DPadLeft, Button::Left }, { gaming::GamepadButtons::DPadRight, Button::Right },
                            { gaming::GamepadButtons::Menu, Button::Start }, { gaming::GamepadButtons::View, Button::Back },
                            { gaming::GamepadButtons::LeftThumbstick, Button::LeftStick }, { gaming::GamepadButtons::RightThumbstick, Button::RightStick },
                            { gaming::GamepadButtons::A, Button::A }, { gaming::GamepadButtons::B, Button::B },
                            { gaming::GamepadButtons::X, Button::X }, { gaming::GamepadButtons::Y, Button::Y },
                            { gaming::GamepadButtons::LeftShoulder, Button::LeftShoulder }, { gaming::GamepadButtons::RightShoulder, Button::RightShoulder },
                        };
                        const gaming::GamepadReading reading = pads.GetAt(i).GetCurrentReading();
                        for (const auto& m : map)
                        {
                            if ((reading.Buttons & m.mask) == m.mask)
                            {
                                pad.buttons |= 1u << static_cast<u32>(m.button);
                            }
                        }
                        pad.leftX = removeDeadZone(static_cast<float>(reading.LeftThumbstickX));
                        pad.leftY = removeDeadZone(static_cast<float>(reading.LeftThumbstickY));
                        pad.rightX = removeDeadZone(static_cast<float>(reading.RightThumbstickX));
                        pad.rightY = removeDeadZone(static_cast<float>(reading.RightThumbstickY));
                        pad.leftTrigger = static_cast<float>(reading.LeftTrigger);
                        pad.rightTrigger = static_cast<float>(reading.RightTrigger);
                    }
                    m_input.pads[i] = pad;
                }
            }

            CoreWindow m_window{ nullptr };
            UwpWindow m_native;
            InputState m_input;
            event_token m_keyDown, m_keyUp, m_typed, m_pointerMoved, m_pointerPressed, m_pointerReleased, m_wheel, m_activated, m_closed, m_back;
            const bool m_xbox = winrt::Windows::System::Profile::AnalyticsInfo::VersionInfo().DeviceFamily() == L"Windows.Xbox";
            bool m_focused = true;
            bool m_quit = false;
        };

        // The log goes to a file, as there is no console: LocalState\nexgen.log.
        void openLog()
        {
            const hstring folder = winrt::Windows::Storage::ApplicationData::Current().LocalFolder().Path();
            // Shared, so it can be read while the app runs.
            FILE* file = _wfreopen((folder + L"\\nexgen.log").c_str(), L"w", stdout);
            if (file)
            {
                setvbuf(file, nullptr, _IONBF, 0);
            }
        }

        struct View : implements<View, IFrameworkView>
        {
            void Initialize(const CoreApplicationView& view)
            {
                // Run() isn't called until the view is activated.
                view.Activated([](const CoreApplicationView&, const IActivatedEventArgs&) { CoreWindow::GetForCurrentThread().Activate(); });
            }

            void SetWindow(const CoreWindow&) {}
            void Load(const hstring&) {}
            void Uninitialize() {}

            void Run()
            {
                openLog();
                char program[] = "showcase";
                char* argv[] = { program };
                nxAppMain(1, argv);
            }
        };

        struct ViewSource : implements<ViewSource, IFrameworkViewSource>
        {
            IFrameworkView CreateView() { return make<View>(); }
        };
    }

    Platform* createPlatform()
    {
        PlatformUwp* platform = create<PlatformUwp>(MemTag::General);
        if (platform && !platform->init())
        {
            destroy(platform);
            return nullptr;
        }
        return platform;
    }
}

int __stdcall WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    init_apartment();
    CoreApplication::Run(make<nx::ViewSource>());
    return 0;
}
