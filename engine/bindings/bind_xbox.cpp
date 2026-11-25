#include "bindings/script_refs.h"
#include "net/net.h"
#include "platform/console.h"

#include <scriptarray/scriptarray.h>

namespace nx
{
    namespace
    {
        int cpuTemperature() { return consoleTemperature(true); }
        int boardTemperature() { return consoleTemperature(false); }
        void setFan(int percent) { consoleSetFan(percent); }
        void setLight(u32 pattern) { consoleSetLight(static_cast<u8>(pattern)); }
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
        bool startFtp(const std::string& user, const std::string& password, bool mayWrite, u32 port)
        {
            Net& net = *services().net;
            FtpFiles* files = consoleFiles();
            return files && port < 65536 && net.start() && net.ftp().start(*files, user, password, mayWrite, static_cast<u16>(port), net.onlyHere());
        }
        void stopFtp() { services().net->ftp().stop(); }
        bool ftpRunning() { return services().net->ftp().running(); }
        u32 ftpClients() { return services().net->ftp().clients(); }
        std::string ftpLast() { return services().net->ftp().running() ? services().net->ftp().last() : ""; }
        u32 ftpKilobytes() { return static_cast<u32>((services().net->ftp().sent() + services().net->ftp().received()) / 1024); }
    }

    void bindXbox(Binder& b)
    {
        b.enumType("Tray", "What the disc tray is doing.");
        b.enumValue("TrayClosed", static_cast<int>(Tray::Closed), "Closed, with a disc in it.");
        b.enumValue("TrayOpen", static_cast<int>(Tray::Open), "Open.");
        b.enumValue("TrayMoving", static_cast<int>(Tray::Moving), "On its way.");
        b.enumValue("TrayEmpty", static_cast<int>(Tray::Empty), "Closed, with nothing in it.");
        b.enumValue("TrayUnknown", static_cast<int>(Tray::Unknown), "It can't be told.");

        b.enumType("XboxText", "Things a console can say about itself.");
        b.enumValue("XboxKernel", static_cast<int>(ConsoleText::Kernel), "The version of its kernel.");
        b.enumValue("XboxHardware", static_cast<int>(ConsoleText::Hardware), "Which chips it has.");
        b.enumValue("XboxSerial", static_cast<int>(ConsoleText::Serial), "Its serial number.");
        b.enumValue("XboxEthernet", static_cast<int>(ConsoleText::Ethernet), "Its network hardware address.");
        b.enumValue("XboxRegion", static_cast<int>(ConsoleText::Region), "Where it was sold.");
        b.enumValue("XboxVideo", static_cast<int>(ConsoleText::Video), "Its kind of picture.");
        b.enumValue("XboxCable", static_cast<int>(ConsoleText::Cable), "The cable to the television.");

        b.section("Xbox", "What only the original Xbox has. Elsewhere `available` is false, readings are -1 or empty, and the rest do nothing.");
        b.names("Xbox");
        b.function("bool get_available() property", NX_FN(consoleAvailable), "Whether this is one.");
        b.function("int get_cpuTemperature() property", NX_FN(cpuTemperature), "Degrees Celsius, or -1.");
        b.function("int get_boardTemperature() property", NX_FN(boardTemperature), "The same, of the board.");
        b.function("int get_fan() property", NX_FN(consoleFan), "How fast it is turning, 0 to 100, or -1.");
        b.function("void set_fan(int) property", NX_FN(setFan), "Sets its speed, to no less than 20; -1 lets the console decide again.");
        b.function("void set_light(uint) property", NX_FN(setLight), "The light on the front: four moments, with a bit for each, red in the high four and green in the low four. 0x0F is green, 0xF0 red, 0xFF orange and 0xA5 blinks between two. 0 lets the console decide.");
        b.function("Tray get_tray() property", NX_FN(consoleTray), "What it is doing.");
        b.function("bool openTray()", NX_FN(openTray), "Opens it.");
        b.function("bool closeTray()", NX_FN(closeTray), "Closes it.");
        b.function("string text(XboxText what)", NX_FN(text), "Something about the console.");
        b.function("uint driveSize(const string &in letter)", NX_FN(driveSize), "Megabytes on C, E, F, G, X, Y or Z, or 0 if it isn't there.");
        b.function("uint driveFree(const string &in letter)", NX_FN(driveFree), "Megabytes free.");
        b.function("array<string>@ folders(const string &in path)", NX_FN(folders), "The folders in one, such as \"E:\\\\Games\".");
        b.function("array<string>@ files(const string &in path)", NX_FN(files), "The files in one.");
        b.function("bool launch(const string &in path)", NX_FN(launch), "Runs another title, such as \"E:\\\\Games\\\\Thing\\\\default.xbe\". False if it isn't there; otherwise this one ends.");
        b.function("bool startFtp(const string &in user, const string &in password, bool mayWrite = false, uint port = 21)", NX_FN(startFtp), "Serves drives E, F and G to FTP programs on the same network, until it is stopped. The password must be four characters or more. Nothing can be changed unless it may write. Whoever has the password, and whoever can listen on the network, sees everything: FTP hides nothing.");
        b.function("void stopFtp()", NX_FN(stopFtp), "Ends it.");
        b.function("bool get_ftpRunning() property", NX_FN(ftpRunning), "Whether it is going.");
        b.function("uint get_ftpClients() property", NX_FN(ftpClients), "How many are connected.");
        b.function("string get_ftpLast() property", NX_FN(ftpLast), "The latest thing that happened.");
        b.function("uint get_ftpKilobytes() property", NX_FN(ftpKilobytes), "How much has gone either way.");
        b.function("void toDashboard()", NX_FN(toDashboard), "Leaves for the dashboard.");
        b.function("void toFirmware()", NX_FN(toFirmware), "Leaves for the firmware, as if it had just been turned on.");
        b.function("void restart()", NX_FN(restart), "Restarts, without the start-up animation.");
        b.function("void powerOff()", NX_FN(powerOff), "Turns the console off.");
        b.function("int busRead(uint address, uint command, bool word = false)", NX_FN(consoleBusRead), "Reads from something on the SMBus, or -1. 0x20 is the system controller. The console's settings at 0xA8 are refused, as there are calls for what is in them.");
        b.function("bool busWrite(uint address, uint command, uint value, bool word = false)", NX_FN(consoleBusWrite), "Writes to something on the SMBus. The console's settings are refused: a mistake there can't be undone.");
        b.names("");
    }
}
