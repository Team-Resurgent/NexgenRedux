#include "bindings/script_refs.h"
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
        Tray x360Tray() { return xbox360::tray(); }
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

    void bindMachines(Binder& b)
    {
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
