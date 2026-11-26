#include "platform/machines.h"

#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <unistd.h>

#include <vector>

// Linux: what it is, xdg-open, other programs, and the battery as the kernel tells of it.
namespace nx::linux_
{
    namespace
    {
        std::string firstLine(const char* path)
        {
            FILE* f = fopen(path, "r");
            if (f == nullptr)
            {
                return {};
            }
            char line[256] = "";
            if (!fgets(line, sizeof(line), f))
            {
                line[0] = 0;
            }
            fclose(f);
            line[strcspn(line, "\r\n")] = 0;
            return line;
        }

        // Started on its own: a child that starts it and goes at once, so nothing is left waiting to be reaped.
        bool start(std::vector<std::string> words)
        {
            std::vector<char*> argv;
            for (std::string& w : words)
            {
                argv.push_back(w.data());
            }
            argv.push_back(nullptr);
            const pid_t child = fork();
            if (child < 0)
            {
                return false;
            }
            if (child == 0)
            {
                if (fork() == 0)
                {
                    setsid();
                    execvp(argv[0], argv.data());
                }
                _exit(0);
            }
            int status = 0;
            waitpid(child, &status, 0);
            return true;
        }

        // The first battery the kernel knows of.
        std::string batteryFile(const char* name)
        {
            for (int i = 0; i < 4; i++)
            {
                char path[96];
                snprintf(path, sizeof(path), "/sys/class/power_supply/BAT%d/%s", i, name);
                const std::string value = firstLine(path);
                if (!value.empty())
                {
                    return value;
                }
            }
            return {};
        }
    }

    bool available() { return true; }

    std::string text(Text what)
    {
        switch (what)
        {
        case Text::Distribution:
        {
            FILE* f = fopen("/etc/os-release", "r");
            std::string out = "Linux";
            char line[256];
            while (f && fgets(line, sizeof(line), f))
            {
                if (strncmp(line, "PRETTY_NAME=", 12) == 0)
                {
                    char* value = line + 12;
                    value[strcspn(value, "\r\n")] = 0;
                    const size_t length = strlen(value);
                    out = length >= 2 && value[0] == '"' ? std::string(value + 1, length - 2) : std::string(value);
                }
            }
            if (f)
            {
                fclose(f);
            }
            return out;
        }
        case Text::Kernel:
        {
            utsname name;
            return uname(&name) == 0 ? std::string(name.sysname) + " " + name.release : std::string();
        }
        case Text::Desktop:
        {
            const char* desktop = getenv("XDG_CURRENT_DESKTOP");
            return desktop ? desktop : "";
        }
        case Text::Computer:
        {
            char name[256] = "";
            return gethostname(name, sizeof(name)) == 0 ? name : "";
        }
        case Text::User:
        {
            const passwd* user = getpwuid(getuid());
            return user && user->pw_name ? user->pw_name : "";
        }
        default:
            return {};
        }
    }

    bool open(const char* what) { return start({ "xdg-open", what }); }

    bool run(const char* program, const char* arguments)
    {
        std::vector<std::string> words = { program };
        for (const char* at = arguments ? arguments : ""; *at;)
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
        return start(words);
    }

    int battery()
    {
        const std::string capacity = batteryFile("capacity");
        return capacity.empty() ? -1 : atoi(capacity.c_str());
    }

    bool charging()
    {
        const std::string status = batteryFile("status");
        return status == "Charging" || status == "Full";
    }
}
