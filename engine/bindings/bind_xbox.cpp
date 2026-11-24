#include "bindings/script_refs.h"
#include "platform/console.h"

#include <scriptarray/scriptarray.h>

namespace nx
{
    namespace
    {
        int cpuTemperature() { return consoleTemperature(true); }
        int boardTemperature() { return consoleTemperature(false); }
        bool setFan(int percent) { return consoleSetFan(percent < 0 ? 0 : percent); }
        bool autoFan() { return consoleSetFan(-1); }
        bool setLight(u32 pattern) { return consoleSetLight(static_cast<u8>(pattern)); }
        bool openTray() { return consoleMoveTray(true); }
        bool closeTray() { return consoleMoveTray(false); }
        std::string text(ConsoleText what) { return what < ConsoleText::Count ? consoleText(what) : ""; }
        u32 driveSize(const std::string& letter)
        {
            u32 total, free;
            return !letter.empty() && consoleDrive(letter[0], total, free) ? total : 0;
        }
        u32 driveFree(const std::string& letter)
        {
            u32 total, free;
            return !letter.empty() && consoleDrive(letter[0], total, free) ? free : 0;
        }
        CScriptArray* list(const std::string& path, bool folders)
        {
            std::vector<std::string> names;
            consoleList(path.c_str(), folders, names);
            asIScriptEngine* engine = asGetActiveContext()->GetEngine();
            CScriptArray* out = CScriptArray::Create(engine->GetTypeInfoByDecl("array<string>"), static_cast<asUINT>(names.size()));
            for (size_t i = 0; i < names.size(); i++)
            {
                out->SetValue(static_cast<asUINT>(i), &names[i]);
            }
            return out;
        }
        CScriptArray* folders(const std::string& path) { return list(path, true); }
        CScriptArray* files(const std::string& path) { return list(path, false); }
        void toFirmware() { consoleExit(ConsoleExit::Firmware); }
        void restart() { consoleExit(ConsoleExit::Restart); }
        void toDashboard() { consoleExit(ConsoleExit::Dashboard); }
        void powerOff() { consoleExit(ConsoleExit::Off); }
        bool launch(const std::string& path) { return consoleLaunch(path.c_str()); }
    }

    void bindXbox(Binder& b)
    {
        b.enumType("Tray", "What the disc tray is doing.");
        b.enumValue("TrayClosed", static_cast<int>(Tray::Closed), "Closed, with a disc in it.");
        b.enumValue("TrayOpen", static_cast<int>(Tray::Open), "Open.");
        b.enumValue("TrayMoving", static_cast<int>(Tray::Moving), "On its way.");
        b.enumValue("TrayEmpty", static_cast<int>(Tray::Empty), "Closed, with nothing in it.");
        b.enumValue("TrayUnknown", static_cast<int>(Tray::Unknown), "It can't be told.");

        b.enumType("ConsoleText", "Things a console can say about itself.");
        b.enumValue("ConsoleKernel", static_cast<int>(ConsoleText::Kernel), "The version of its kernel.");
        b.enumValue("ConsoleHardware", static_cast<int>(ConsoleText::Hardware), "Which chips it has.");
        b.enumValue("ConsoleSerial", static_cast<int>(ConsoleText::Serial), "Its serial number.");
        b.enumValue("ConsoleEthernet", static_cast<int>(ConsoleText::Ethernet), "Its network hardware address.");
        b.enumValue("ConsoleRegion", static_cast<int>(ConsoleText::Region), "Where it was sold.");
        b.enumValue("ConsoleVideo", static_cast<int>(ConsoleText::Video), "Its kind of picture.");
        b.enumValue("ConsoleCable", static_cast<int>(ConsoleText::Cable), "The cable to the television.");

        b.section("Xbox", "What only the original Xbox has. Elsewhere `available` is false, readings are -1 or empty, and the rest do nothing.");
        b.names("Xbox");
        b.function("bool available()", NX_FN(consoleAvailable), "Whether this is one.");
        b.function("int cpuTemperature()", NX_FN(cpuTemperature), "Degrees Celsius, or -1.");
        b.function("int boardTemperature()", NX_FN(boardTemperature), "The same, of the board.");
        b.function("int fan()", NX_FN(consoleFan), "How fast it is turning, 0 to 100, or -1.");
        b.function("bool setFan(int percent)", NX_FN(setFan), "Sets its speed, to no less than 20.");
        b.function("bool autoFan()", NX_FN(autoFan), "Lets the console decide again.");
        b.function("bool setLight(uint pattern)", NX_FN(setLight), "The light on the front: four moments, with a bit for each, red in the high four and green in the low four. 0x0F is green, 0xF0 red, 0xFF orange and 0xA5 blinks between two. 0 lets the console decide.");
        b.function("Tray tray()", NX_FN(consoleTray), "What it is doing.");
        b.function("bool openTray()", NX_FN(openTray), "Opens it.");
        b.function("bool closeTray()", NX_FN(closeTray), "Closes it.");
        b.function("string text(ConsoleText what)", NX_FN(text), "Something about the console.");
        b.function("uint driveSize(const string &in letter)", NX_FN(driveSize), "Megabytes on C, E, F, G, X, Y or Z, or 0 if it isn't there.");
        b.function("uint driveFree(const string &in letter)", NX_FN(driveFree), "Megabytes free.");
        b.function("array<string>@ folders(const string &in path)", NX_FN(folders), "The folders in one, such as \"E:\\\\Games\".");
        b.function("array<string>@ files(const string &in path)", NX_FN(files), "The files in one.");
        b.function("bool launch(const string &in path)", NX_FN(launch), "Runs another title, such as \"E:\\\\Games\\\\Thing\\\\default.xbe\". False if it isn't there; otherwise this one ends.");
        b.function("void toDashboard()", NX_FN(toDashboard), "Leaves for the dashboard.");
        b.function("void toFirmware()", NX_FN(toFirmware), "Leaves for the firmware, as if it had just been turned on.");
        b.function("void restart()", NX_FN(restart), "Restarts, without the start-up animation.");
        b.function("void powerOff()", NX_FN(powerOff), "Turns the console off.");
        b.function("int busRead(uint address, uint command, bool word = false)", NX_FN(consoleBusRead), "Reads from something on the SMBus, or -1. 0x20 is the system controller. The console's settings at 0xA8 are refused, as there are calls for what is in them.");
        b.function("bool busWrite(uint address, uint command, uint value, bool word = false)", NX_FN(consoleBusWrite), "Writes to something on the SMBus. The console's settings are refused: a mistake there can't be undone.");
        b.names("");
    }
}
