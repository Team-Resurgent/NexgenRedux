#include "bindings/script_refs.h"
#include "gpu/gpu.h"
#include "platform/machines.h"

// What each kind of machine can do that others can't, beside the original Xbox's in bind_xbox.cpp. Each is a
// namespace of its own, there everywhere so that scripts run anywhere: elsewhere `available` is false.
namespace nx
{
    namespace
    {
        // --- Xbox 360 ---
        bool x360Available() { return xbox360::available(); }
        int x360Cpu() { return xbox360::temperature(0); }
        int x360Gpu() { return xbox360::temperature(1); }
        int x360Memory() { return xbox360::temperature(2); }
        int x360Board() { return xbox360::temperature(3); }
        int x360Tray() { return static_cast<int>(xbox360::tray()); }
        bool x360OpenTray() { return xbox360::moveTray(true); }
        bool x360CloseTray() { return xbox360::moveTray(false); }
        void x360Lights(u32 pattern) { xbox360::setLights(static_cast<u8>(pattern)); }
        std::string x360Kernel() { return xbox360::text(xbox360::Text::Kernel); }
        std::string x360Region() { return xbox360::text(xbox360::Text::Region); }
        std::string x360Language() { return xbox360::text(xbox360::Text::Language); }
        std::string x360Video() { return xbox360::text(xbox360::Text::Video); }
        std::string x360Player() { return xbox360::text(xbox360::Text::Player); }
        bool x360Launch(const std::string& path) { return xbox360::launch(path.c_str()); }
        void x360Dashboard() { xbox360::toDashboard(); }
        void x360Restart() { xbox360::restart(); }
        void x360PowerOff() { xbox360::powerOff(); }

        // --- Xbox One and Series ---
        bool oneAvailable() { return xboxone::available(); }
        std::string oneModel() { return xboxone::text(xboxone::Text::Model); }
        std::string oneSystem() { return xboxone::text(xboxone::Text::System); }
        u32 oneMemory() { return xboxone::memoryLimit(); }
        bool oneOpen(const std::string& uri) { return xboxone::openUri(uri.c_str()); }

        // --- Windows ---
        bool winAvailable() { return windows::available(); }
        std::string winVersion() { return windows::text(windows::Text::Version); }
        std::string winComputer() { return windows::text(windows::Text::Computer); }
        std::string winUser() { return windows::text(windows::Text::User); }
        bool winOpen(const std::string& what) { return windows::open(what.c_str()); }
        bool winReveal(const std::string& path) { return windows::reveal(path.c_str()); }
        bool winRun(const std::string& program, const std::string& arguments) { return windows::run(program.c_str(), arguments.c_str()); }
        int winBattery() { return windows::battery(); }
        bool winCharging() { return windows::charging(); }
        bool winDark() { return windows::darkMode(); }
        std::string winClipboard() { return windows::clipboard(); }
        void winSetClipboard(const std::string& text) { windows::setClipboard(text.c_str()); }

        // --- macOS ---
        bool macAvailable() { return mac::available(); }
        std::string macVersion() { return mac::text(mac::Text::Version); }
        std::string macComputer() { return mac::text(mac::Text::Computer); }
        std::string macUser() { return mac::text(mac::Text::User); }
        bool macOpen(const std::string& what) { return mac::open(what.c_str()); }
        bool macReveal(const std::string& path) { return mac::reveal(path.c_str()); }
        bool macRun(const std::string& application, const std::string& arguments) { return mac::run(application.c_str(), arguments.c_str()); }
        int macBattery() { return mac::battery(); }
        bool macCharging() { return mac::charging(); }
        bool macDark() { return mac::darkMode(); }
        std::string macClipboard() { return mac::clipboard(); }
        void macSetClipboard(const std::string& text) { mac::setClipboard(text.c_str()); }

        // --- Linux ---
        bool linuxAvailable() { return linux_::available(); }
        std::string linuxDistribution() { return linux_::text(linux_::Text::Distribution); }
        std::string linuxKernel() { return linux_::text(linux_::Text::Kernel); }
        std::string linuxDesktop() { return linux_::text(linux_::Text::Desktop); }
        std::string linuxComputer() { return linux_::text(linux_::Text::Computer); }
        std::string linuxUser() { return linux_::text(linux_::Text::User); }
        bool linuxOpen(const std::string& what) { return linux_::open(what.c_str()); }
        bool linuxRun(const std::string& program, const std::string& arguments) { return linux_::run(program.c_str(), arguments.c_str()); }
        int linuxBattery() { return linux_::battery(); }
        bool linuxCharging() { return linux_::charging(); }
    }

    namespace
    {
        // --- The window and the system, the same on every machine ---
        Platform& platform() { return *services().platform; }
        bool windowAvailable() { const std::string n = platform().name(); return n == "Windows" || n == "Linux" || n == "macOS"; }
        bool windowFullscreen() { return platform().fullscreen(); }
        void windowSetFullscreen(bool on) { platform().setFullscreen(on); }
        std::string g_title = "Nexgen";
        std::string windowTitle() { return g_title; }
        void windowSetTitle(const std::string& title) { g_title = title; platform().setTitle(title.c_str()); }
        u32 windowWidth() { u32 w = 0, h = 0; services().device->size(w, h); return w; }
        u32 windowHeight() { u32 w = 0, h = 0; services().device->size(w, h); return h; }
        bool windowResize(u32 width, u32 height) { return platform().setWindowSize(width, height); }
        bool g_vsync = true;
        bool windowVsync() { return g_vsync; }
        void windowSetVsync(bool on) { g_vsync = on; services().device->setWait(on); }
        PointerShape g_pointer = PointerShape::Arrow;
        // Enums go to and from scripts as whole ints: a smaller one leaves the rest of the register as it was.
        int windowPointer() { return static_cast<int>(g_pointer); }
        void windowSetPointer(int value)
        {
            const PointerShape shape = static_cast<PointerShape>(value);
            if (value >= 0 && value < static_cast<int>(PointerShape::Count))
            {
                g_pointer = shape;
                platform().setPointer(shape);
            }
        }
        bool windowFocused() { return platform().focused(); }

        // The time of day; -1, or nothing, where it can't be told.
        int timePart(int which)
        {
            u32 parts[3] = {};
            return platform().localTime(parts[0], parts[1], parts[2]) ? static_cast<int>(parts[which]) : -1;
        }
        int systemHour() { return timePart(0); }
        int systemMinute() { return timePart(1); }
        int systemSecond() { return timePart(2); }
        std::string systemClock(bool twelve)
        {
            u32 h = 0, m = 0, s = 0;
            if (!platform().localTime(h, m, s))
            {
                return std::string();
            }
            char out[16];
            if (twelve)
            {
                snprintf(out, sizeof(out), "%u:%02u %s", static_cast<unsigned>(h % 12 ? h % 12 : 12), static_cast<unsigned>(m), h < 12 ? "am" : "pm");
            }
            else
            {
                snprintf(out, sizeof(out), "%02u:%02u", static_cast<unsigned>(h), static_cast<unsigned>(m));
            }
            return out;
        }

        // Windows 11's frame, kept here and given to the window whole each time a part of it changes.
        enum class TitleLook : u8 { System, Light, Dark };
        TitleBar g_titleBar;
        void titleBarChanged() { platform().setTitleBar(g_titleBar); }
        int titleLook() { return g_titleBar.dark + 1; }
        void setTitleLook(int value)
        {
            const TitleLook look = static_cast<TitleLook>(value);
            g_titleBar.dark = look == TitleLook::Light ? 0 : look == TitleLook::Dark ? 1 : -1;
            titleBarChanged();
        }
        u32 rgbOf(const Color& c)
        {
            const auto byte = [](float v) { return static_cast<u32>(v <= 0 ? 0 : v >= 1 ? 255 : v * 255.0f + 0.5f); };
            return c.a <= 0 ? TitleBar::kSystem : (byte(c.r) << 16) | (byte(c.g) << 8) | byte(c.b);
        }
        Color colorOf(u32 rgb)
        {
            if (rgb >= TitleBar::kNone)
            {
                return Color{ 0, 0, 0, 0 };
            }
            return Color{ static_cast<float>((rgb >> 16) & 0xFF) / 255.0f, static_cast<float>((rgb >> 8) & 0xFF) / 255.0f, static_cast<float>(rgb & 0xFF) / 255.0f, 1 };
        }
        Color titleColor() { return colorOf(g_titleBar.back); }
        void setTitleColor(const Color& c) { g_titleBar.back = rgbOf(c); titleBarChanged(); }
        Color titleTextColor() { return colorOf(g_titleBar.text); }
        void setTitleTextColor(const Color& c) { g_titleBar.text = rgbOf(c); titleBarChanged(); }
        Color borderColor() { return colorOf(g_titleBar.border); }
        void setBorderColor(const Color& c) { g_titleBar.border = rgbOf(c); titleBarChanged(); }
        bool noBorder() { return g_titleBar.border == TitleBar::kNone; }
        void setNoBorder(bool none)
        {
            g_titleBar.border = none ? TitleBar::kNone : TitleBar::kSystem;
            titleBarChanged();
        }
        // What this Windows can do with its frame: colours, borders and corners from Windows 11 on, backdrops from its
        // 2022 update (build 22621). The frame's own window, where there is one.
        bool titleColors() { return windows::build() >= 22000 && platform().nativeWindow() != nullptr; }
        bool backdrops() { return windows::build() >= 22621 && platform().nativeWindow() != nullptr; }
        int corners() { return static_cast<int>(g_titleBar.corners); }
        void setCorners(int value)
        {
            const TitleBar::Corners c = static_cast<TitleBar::Corners>(value);
            if (value >= 0 && value <= static_cast<int>(TitleBar::Corners::SmallRound))
            {
                g_titleBar.corners = c;
                titleBarChanged();
            }
        }
        int backdrop() { return static_cast<int>(g_titleBar.backdrop); }
        void setBackdrop(int value)
        {
            const TitleBar::Backdrop d = static_cast<TitleBar::Backdrop>(value);
            if (value >= 0 && value <= static_cast<int>(TitleBar::Backdrop::Tabbed))
            {
                g_titleBar.backdrop = d;
                titleBarChanged();
            }
        }
        bool windowMessage(const std::string& title, const std::string& text) { return platform().showMessage(title.c_str(), text.c_str(), false); }
        bool windowAsk(const std::string& title, const std::string& text) { return platform().showMessage(title.c_str(), text.c_str(), true); }

        // Where the system has no clipboard, or it can't be reached, the engine keeps one of its own, so copying and
        // pasting still works within the game.
        std::string g_clipboard;
        std::string systemClipboard()
        {
            std::string text;
            return platform().clipboard(text) ? text : g_clipboard;
        }
        void systemSetClipboard(const std::string& text)
        {
            g_clipboard = text.size() > (1u << 20) ? text.substr(0, 1u << 20) : text;
            platform().setClipboard(g_clipboard.c_str());
        }
        std::string systemLocale()
        {
            const std::string l = platform().locale();
            return l.empty() ? "en" : l;
        }
        std::string systemLanguage()
        {
            const std::string l = systemLocale();
            const size_t dash = l.find('-');
            return dash == std::string::npos ? l : l.substr(0, dash);
        }
        u32 systemProcessors() { return platform().processors(); }
        bool systemOpen(const std::string& what)
        {
            if (what.empty())
            {
                return false;
            }
            return windows::available() ? windows::open(what.c_str()) : mac::available() ? mac::open(what.c_str()) : linux_::available() ? linux_::open(what.c_str())
                                                                                     : xboxone::available() ? xboxone::openUri(what.c_str())
                                                                                                             : false;
        }
        int systemBattery() { return windows::available() ? windows::battery() : mac::available() ? mac::battery() : linux_::available() ? linux_::battery() : -1; }
        bool systemCharging() { return windows::available() ? windows::charging() : mac::available() ? mac::charging() : linux_::available() ? linux_::charging() : false; }
    }

    void bindMachines(Binder& b)
    {
        b.enumType("Pointer", "How the mouse pointer looks over the window.");
        const char* const shapes[] = { "PointerArrow", "PointerHand", "PointerText", "PointerCrosshair", "PointerWait", "PointerMove", "PointerResizeAcross", "PointerResizeUpDown",
                                       "PointerNotAllowed" };
        const char* const meanings[] = { "The usual arrow.", "A hand, over what can be clicked.", "A bar, over text to type in.", "A cross, for aiming or picking a point.",
                                         "Busy.", "Arrows four ways, for dragging something about.", "Left and right, for resizing across.", "Up and down.", "Can't be done here." };
        for (u32 i = 0; i < 9; i++)
        {
            b.enumValue(shapes[i], static_cast<int>(i), meanings[i]);
        }

        b.section("Window", "The window, on a computer: full-screen or not, its size, title and pointer, and messages. On a console there is only the screen: `available` is false, and changing it does nothing.");
        b.names("Window");
        b.function("bool get_available() property", NX_FN(windowAvailable), "Whether there is a window to change: on Windows, Linux and macOS.");
        b.function("bool get_fullscreen() property", NX_FN(windowFullscreen), "Whether it fills the screen.");
        b.function("void set_fullscreen(bool) property", NX_FN(windowSetFullscreen), "Fills the screen without a border, or goes back to a window. Alt and Enter does the same on Windows.");
        b.function("string get_title() property", NX_FN(windowTitle), "What its title bar says.");
        b.function("void set_title(const string &in) property", NX_FN(windowSetTitle), "Changes it.");
        b.function("uint get_width() property", NX_FN(windowWidth), "Pixels across what is drawn: the window's inside, or the screen.");
        b.function("uint get_height() property", NX_FN(windowHeight), "Pixels down.");
        b.function("bool resize(uint width, uint height)", NX_FN(windowResize), "Makes its inside so many pixels, while it isn't full-screen. False where it can't be.");
        b.function("bool get_vsync() property", NX_FN(windowVsync), "Whether each frame waits for the screen's refresh: true to begin with.");
        b.function("void set_vsync(bool) property", NX_FN(windowSetVsync), "Waits for the refresh, smooth and without tearing, or doesn't, for as many frames as it can make. Consoles always wait.");
        b.function("Pointer get_pointer() property", NX_FN(windowPointer), "How the pointer looks.");
        b.function("void set_pointer(Pointer) property", NX_FN(windowSetPointer), "Changes how it looks over the window, until changed again: a hand over a button, a bar over a field.");
        b.function("bool get_focused() property", NX_FN(windowFocused), "Whether it is the window being used: false while the player is in another, which is a time to pause.");
        b.function("bool message(const string &in title, const string &in text)", NX_FN(windowMessage), "The system's own box with a message and OK, waited for. False where there is none: on a console, or Linux with no zenity, kdialog or xmessage.");
        b.function("bool ask(const string &in title, const string &in question)", NX_FN(windowAsk), "The same with Yes and No: true for yes.");
        b.names("");

        b.section("System", "What every machine can say about itself, and do, in the same way.");
        b.names("System");
        b.function("string get_clipboard() property", NX_FN(systemClipboard), "The text that was copied. Where the system has no clipboard, as on a console, the game keeps its own.");
        b.function("void set_clipboard(const string &in) property", NX_FN(systemSetClipboard), "Copies text, to paste elsewhere or here. On Linux it needs wl-copy, xclip or xsel.");
        b.function("string get_locale() property", NX_FN(systemLocale), "The language and region it is set to, as \"en-GB\" or \"pt-BR\"; a console says the language only, as \"ja\". \"en\" if it can't be told.");
        b.function("string get_language() property", NX_FN(systemLanguage), "The language alone, as \"en\": for choosing which words to show.");
        b.function("int get_hour() property", NX_FN(systemHour), "The hour where the machine is, as its clock is set, 0 to 23; -1 if it can't be told.");
        b.function("int get_minute() property", NX_FN(systemMinute), "The minute, 0 to 59.");
        b.function("int get_second() property", NX_FN(systemSecond), "The second, 0 to 59.");
        b.function("string clock(bool twelveHour = false)", NX_FN(systemClock), "The time as a clock shows it: \"16:55\", or \"4:55 pm\". Empty if it can't be told.");
        b.function("uint get_processors() property", NX_FN(systemProcessors), "Threads it can run at once: 1 on the original Xbox, 6 on the 360.");
        b.function("bool open(const string &in address)", NX_FN(systemOpen), "A web page, file or folder, in what the system opens it with. False where it can't: the consoles but the Xbox One.");
        b.function("int get_battery() property", NX_FN(systemBattery), "How full the battery is, 0 to 100, or -1 without one.");
        b.function("bool get_charging() property", NX_FN(systemCharging), "Whether it is plugged in.");
        b.names("");

        b.section("Xbox360", "What only the Xbox 360 has. Elsewhere `available` is false, readings are -1 or empty, and the rest do nothing.");
        b.names("Xbox360");
        b.function("bool get_available() property", NX_FN(x360Available), "Whether this is one.");
        b.function("int get_cpuTemperature() property", NX_FN(x360Cpu), "Degrees Celsius, from the system controller, or -1.");
        b.function("int get_gpuTemperature() property", NX_FN(x360Gpu), "The same, of the graphics.");
        b.function("int get_memoryTemperature() property", NX_FN(x360Memory), "The same, of the memory.");
        b.function("int get_boardTemperature() property", NX_FN(x360Board), "The same, of the board.");
        b.function("Tray get_tray() property", NX_FN(x360Tray), "What the disc tray is doing.");
        b.function("bool openTray()", NX_FN(x360OpenTray), "Opens it.");
        b.function("bool closeTray()", NX_FN(x360CloseTray), "Closes it.");
        b.function("void set_lights(uint) property", NX_FN(x360Lights), "The ring of light round the power button: a bit for each quarter, green in the low four and red in the high, so 0x0F is all green, 0xF0 all red and 0xFF orange. 0 lets the console decide again.");
        b.function("string get_kernel() property", NX_FN(x360Kernel), "The kernel's version, such as \"2.0.17559.0\".");
        b.function("string get_region() property", NX_FN(x360Region), "Where it is from: North America, Asia, Europe, the rest of the world, or a development kit.");
        b.function("string get_language() property", NX_FN(x360Language), "The language it is set to.");
        b.function("string get_video() property", NX_FN(x360Video), "What it sends to the screen, such as \"1280x720p, 60 Hz, widescreen\".");
        b.function("string get_player() property", NX_FN(x360Player), "The first player's gamertag, or empty if no one is signed in.");
        b.function("bool launch(const string &in path)", NX_FN(x360Launch), "Runs another title, such as \"game:\\\\Other\\\\default.xex\". False if it isn't there; otherwise this one ends.");
        b.function("void toDashboard()", NX_FN(x360Dashboard), "Leaves for the dashboard.");
        b.function("void restart()", NX_FN(x360Restart), "Restarts the console.");
        b.function("void powerOff()", NX_FN(x360PowerOff), "Turns it off.");
        b.names("");

        b.section("XboxOne", "What only an Xbox One or Series has, running the engine as an app. Elsewhere `available` is false and readings are empty.");
        b.names("XboxOne");
        b.function("bool get_available() property", NX_FN(oneAvailable), "Whether this is one.");
        b.function("string get_model() property", NX_FN(oneModel), "Which: Xbox One, One S, One X, Series S or Series X.");
        b.function("string get_system() property", NX_FN(oneSystem), "Its system's version.");
        b.function("uint get_memoryLimit() property", NX_FN(oneMemory), "Megabytes the app may use, which a game is given more of than an app.");
        b.function("bool open(const string &in address)", NX_FN(oneOpen), "Opens a web page, a page of the Store (`ms-windows-store://pdp/?productid=...`) or a setting (`ms-settings:`).");
        b.names("");

        b.section("Windows", "What only Windows on a desktop has. Elsewhere `available` is false, readings are -1 or empty, and the rest do nothing.");
        b.names("Windows");
        b.function("bool get_available() property", NX_FN(winAvailable), "Whether this is it.");
        b.function("string get_version() property", NX_FN(winVersion), "Such as \"Windows 11 (build 26100)\".");
        b.function("string get_computer() property", NX_FN(winComputer), "The computer's name.");
        b.function("string get_user() property", NX_FN(winUser), "Who is signed in.");
        b.function("bool open(const string &in what)", NX_FN(winOpen), "A file, folder or web address, in what Windows opens it with.");
        b.function("bool reveal(const string &in path)", NX_FN(winReveal), "A file, selected in Explorer.");
        b.function("bool run(const string &in program, const string &in arguments = \"\")", NX_FN(winRun), "Starts another program.");
        b.function("int get_battery() property", NX_FN(winBattery), "How full the battery is, 0 to 100, or -1 without one.");
        b.function("bool get_charging() property", NX_FN(winCharging), "Whether it is plugged in.");
        b.function("bool get_darkMode() property", NX_FN(winDark), "Whether apps are set to look dark.");
        b.function("string get_clipboard() property", NX_FN(winClipboard), "The text that was copied.");
        b.function("void set_clipboard(const string &in) property", NX_FN(winSetClipboard), "Copies text, to paste elsewhere.");
        b.names("");

        b.enumType("TitleLook", "Whether the window's frame is light or dark, on Windows.");
        b.enumValue("TitleSystem", static_cast<int>(TitleLook::System), "As the system's apps are set to look, and changing when they do. As it starts.");
        b.enumValue("TitleLight", static_cast<int>(TitleLook::Light), "Light.");
        b.enumValue("TitleDark", static_cast<int>(TitleLook::Dark), "Dark.");
        b.enumType("Corners", "How the window's corners are drawn, on Windows 11.");
        b.enumValue("CornersSystem", static_cast<int>(TitleBar::Corners::System), "As Windows draws them: round. As it starts.");
        b.enumValue("CornersSquare", static_cast<int>(TitleBar::Corners::Square), "Square.");
        b.enumValue("CornersRound", static_cast<int>(TitleBar::Corners::Round), "Round.");
        b.enumValue("CornersSmall", static_cast<int>(TitleBar::Corners::SmallRound), "A little round.");
        b.enumType("Backdrop", "What shows through the window's frame, on Windows 11.");
        b.enumValue("BackdropSystem", static_cast<int>(TitleBar::Backdrop::System), "As Windows chooses. As it starts.");
        b.enumValue("BackdropNone", static_cast<int>(TitleBar::Backdrop::None), "Plain.");
        b.enumValue("BackdropMica", static_cast<int>(TitleBar::Backdrop::Mica), "Mica: the desktop behind it, blurred far into a tint, as Windows' own apps have.");
        b.enumValue("BackdropAcrylic", static_cast<int>(TitleBar::Backdrop::Acrylic), "Acrylic: what is behind it, frosted.");
        b.enumValue("BackdropTabbed", static_cast<int>(TitleBar::Backdrop::Tabbed), "Mica, more strongly tinted, as behind tabs.");
        b.names("Windows");
        b.function("bool get_titleColors() property", NX_FN(titleColors), "Whether the frame's colours, border and corners can be changed: on Windows 11. Elsewhere setting them does nothing.");
        b.function("bool get_backdrops() property", NX_FN(backdrops), "Whether a backdrop can be chosen: on Windows 11 from its 2022 update on.");
        b.function("TitleLook get_titleLook() property", NX_FN(titleLook), "Whether the window's frame is light or dark.");
        b.function("void set_titleLook(TitleLook) property", NX_FN(setTitleLook), "Changes it: on Windows 10 too.");
        b.function("Color get_titleColor() property", NX_FN(titleColor), "The colour of the title bar; see-through where it is the system's.");
        b.function("void set_titleColor(const Color &in) property", NX_FN(setTitleColor), "Colours the title bar, on Windows 11, to suit the game. Anything see-through puts back the system's.");
        b.function("Color get_titleTextColor() property", NX_FN(titleTextColor), "The colour of the title's words.");
        b.function("void set_titleTextColor(const Color &in) property", NX_FN(setTitleTextColor), "Colours them, on Windows 11.");
        b.function("Color get_borderColor() property", NX_FN(borderColor), "The colour of the line round the window.");
        b.function("void set_borderColor(const Color &in) property", NX_FN(setBorderColor), "Colours it, on Windows 11.");
        b.function("bool get_noBorder() property", NX_FN(noBorder), "Whether the line round the window is left out.");
        b.function("void set_noBorder(bool) property", NX_FN(setNoBorder), "Leaves it out, on Windows 11, or puts back the system's.");
        b.function("Corners get_corners() property", NX_FN(corners), "How the window's corners are drawn.");
        b.function("void set_corners(Corners) property", NX_FN(setCorners), "Changes them, on Windows 11.");
        b.function("Backdrop get_backdrop() property", NX_FN(backdrop), "What shows through the frame.");
        b.function("void set_backdrop(Backdrop) property", NX_FN(setBackdrop), "Changes it, on Windows 11 from its 2022 update on: it shows in the title bar, the game covering the rest.");
        b.names("");

        b.section("Mac", "What only macOS has. Elsewhere `available` is false, readings are -1 or empty, and the rest do nothing.");
        b.names("Mac");
        b.function("bool get_available() property", NX_FN(macAvailable), "Whether this is one.");
        b.function("string get_version() property", NX_FN(macVersion), "Such as \"macOS Version 15.1 (Build 24B83)\".");
        b.function("string get_computer() property", NX_FN(macComputer), "The computer's name.");
        b.function("string get_user() property", NX_FN(macUser), "Who is signed in.");
        b.function("bool open(const string &in what)", NX_FN(macOpen), "A file, folder or web address, in what the Mac opens it with.");
        b.function("bool reveal(const string &in path)", NX_FN(macReveal), "A file, selected in the Finder.");
        b.function("bool run(const string &in application, const string &in arguments = \"\")", NX_FN(macRun), "Starts an application, by its name (\"Safari\") or its path.");
        b.function("int get_battery() property", NX_FN(macBattery), "How full the battery is, 0 to 100, or -1 without one.");
        b.function("bool get_charging() property", NX_FN(macCharging), "Whether it is plugged in.");
        b.function("bool get_darkMode() property", NX_FN(macDark), "Whether it is set to look dark.");
        b.function("string get_clipboard() property", NX_FN(macClipboard), "The text that was copied.");
        b.function("void set_clipboard(const string &in) property", NX_FN(macSetClipboard), "Copies text, to paste elsewhere.");
        b.names("");

        b.section("Linux", "What only Linux has. Elsewhere `available` is false, readings are -1 or empty, and the rest do nothing.");
        b.names("Linux");
        b.function("bool get_available() property", NX_FN(linuxAvailable), "Whether this is it.");
        b.function("string get_distribution() property", NX_FN(linuxDistribution), "Such as \"Ubuntu 24.04.1 LTS\".");
        b.function("string get_kernel() property", NX_FN(linuxKernel), "Such as \"Linux 6.8.0\".");
        b.function("string get_desktop() property", NX_FN(linuxDesktop), "Such as \"GNOME\" or \"KDE\", or empty.");
        b.function("string get_computer() property", NX_FN(linuxComputer), "The computer's name.");
        b.function("string get_user() property", NX_FN(linuxUser), "Who is signed in.");
        b.function("bool open(const string &in what)", NX_FN(linuxOpen), "A file, folder or web address, with xdg-open.");
        b.function("bool run(const string &in program, const string &in arguments = \"\")", NX_FN(linuxRun), "Starts another program; arguments are separated by spaces.");
        b.function("int get_battery() property", NX_FN(linuxBattery), "How full the battery is, 0 to 100, or -1 without one.");
        b.function("bool get_charging() property", NX_FN(linuxCharging), "Whether it is plugged in.");
        b.names("");
    }
}
