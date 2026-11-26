#include "platform/machines.h"

#import <AppKit/AppKit.h>

#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include <vector>

extern char** environ;

// macOS: the workspace, other applications, the battery as pmset tells of it, the look and the pasteboard.
namespace nx::mac
{
    namespace
    {
        std::string utf8(NSString* s) { return s ? std::string([s UTF8String]) : std::string(); }

        NSURL* urlOf(const char* what)
        {
            NSString* s = [NSString stringWithUTF8String:what];
            return s == nil ? nil : strstr(what, "://") ? [NSURL URLWithString:s] : [NSURL fileURLWithPath:s];
        }

        // What pmset says of the battery: "... 87%; charging; ...".
        std::string power()
        {
            std::string out;
            FILE* p = popen("/usr/bin/pmset -g batt", "r");
            char line[256];
            while (p && fgets(line, sizeof(line), p))
            {
                out += line;
            }
            if (p)
            {
                pclose(p);
            }
            return out;
        }
    }

    bool available() { return true; }

    std::string text(Text what)
    {
        @autoreleasepool
        {
            switch (what)
            {
            case Text::Version: return "macOS " + utf8([[NSProcessInfo processInfo] operatingSystemVersionString]);
            case Text::Computer: return utf8([[NSHost currentHost] localizedName]);
            case Text::User: return utf8(NSUserName());
            default: return {};
            }
        }
    }

    bool open(const char* what)
    {
        @autoreleasepool
        {
            NSURL* url = urlOf(what);
            return url != nil && [[NSWorkspace sharedWorkspace] openURL:url];
        }
    }

    bool reveal(const char* path)
    {
        @autoreleasepool
        {
            NSURL* url = urlOf(path);
            if (url == nil)
            {
                return false;
            }
            [[NSWorkspace sharedWorkspace] activateFileViewerSelectingURLs:@[ url ]];
            return true;
        }
    }

    // With open -a, which finds an application by its name as well as its path.
    bool run(const char* application, const char* arguments)
    {
        std::vector<std::string> words = { "/usr/bin/open", "-a", application };
        if (arguments && *arguments)
        {
            words.push_back("--args");
            for (const char* at = arguments; *at;)
            {
                while (*at == ' ')
                {
                    at++;
                }
                const char* end = at;
                while (*end && *end != ' ')
                {
                    end++;
                }
                if (end > at)
                {
                    words.emplace_back(at, static_cast<size_t>(end - at));
                }
                at = end;
            }
        }
        std::vector<char*> argv;
        for (std::string& w : words)
        {
            argv.push_back(w.data());
        }
        argv.push_back(nullptr);
        pid_t child = 0;
        if (posix_spawn(&child, argv[0], nullptr, nullptr, argv.data(), environ) != 0)
        {
            return false;
        }
        int status = 0;
        waitpid(child, &status, 0);
        return WIFEXITED(status) && WEXITSTATUS(status) == 0;
    }

    int battery()
    {
        const std::string said = power();
        const size_t percent = said.find('%');
        if (said.find("InternalBattery") == std::string::npos || percent == std::string::npos)
        {
            return -1;
        }
        size_t start = percent;
        while (start > 0 && said[start - 1] >= '0' && said[start - 1] <= '9')
        {
            start--;
        }
        return start < percent ? atoi(said.c_str() + start) : -1;
    }

    bool charging()
    {
        const std::string said = power();
        return said.find("InternalBattery") != std::string::npos && (said.find("; charging") != std::string::npos || said.find("; charged") != std::string::npos);
    }

    bool darkMode()
    {
        @autoreleasepool
        {
            NSString* style = [[NSUserDefaults standardUserDefaults] stringForKey:@"AppleInterfaceStyle"];
            return style != nil && [style isEqualToString:@"Dark"];
        }
    }

    std::string clipboard()
    {
        @autoreleasepool
        {
            return utf8([[NSPasteboard generalPasteboard] stringForType:NSPasteboardTypeString]);
        }
    }

    bool setClipboard(const char* value)
    {
        @autoreleasepool
        {
            NSString* s = [NSString stringWithUTF8String:value];
            if (s == nil)
            {
                return false;
            }
            NSPasteboard* board = [NSPasteboard generalPasteboard];
            [board clearContents];
            return [board setString:s forType:NSPasteboardTypeString];
        }
    }
}
