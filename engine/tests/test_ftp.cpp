#include "test.h"

#include "base/thread.h"
#include "net/net.h"

#include <map>
#include <string.h>

using namespace nx;

// All on this machine.
namespace
{
    class Pretend final : public FtpFiles
    {
    public:
        std::map<std::string, std::string> files;   // a folder ends in /
        struct Open
        {
            std::string path;
            size_t at;
        };
        int opened = 0;

        bool list(const std::string& path, std::vector<FtpEntry>& out) override
        {
            const std::string under = path == "/" ? "/" : path + "/";
            for (const auto& f : files)
            {
                if (f.first.size() > under.size() && f.first.compare(0, under.size(), under) == 0)
                {
                    const std::string rest = f.first.substr(under.size());
                    const size_t slash = rest.find('/');
                    if (slash == std::string::npos || slash == rest.size() - 1)
                    {
                        out.push_back({ rest.substr(0, slash), f.second.size(), slash != std::string::npos });
                    }
                }
            }
            return true;
        }
        bool find(const std::string& path, FtpEntry& out) override
        {
            out.folder = files.count(path + "/") != 0;
            out.size = files.count(path) ? files[path].size() : 0;
            return out.folder || files.count(path);
        }
        void* open(const std::string& path, bool write) override
        {
            if (write)
            {
                files[path].clear();
            }
            if (files.count(path) == 0)
            {
                return nullptr;
            }
            opened++;
            return new Open{ path, 0 };
        }
        bool seek(void* f, u64 at) override { static_cast<Open*>(f)->at = static_cast<size_t>(at); return true; }
        size_t read(void* f, void* data, size_t size) override
        {
            Open& o = *static_cast<Open*>(f);
            const std::string& s = files[o.path];
            const size_t left = o.at < s.size() ? s.size() - o.at : 0, n = left < size ? left : size;
            memcpy(data, s.data() + o.at, n);
            o.at += n;
            return n;
        }
        size_t write(void* f, const void* data, size_t size) override
        {
            files[static_cast<Open*>(f)->path].append(static_cast<const char*>(data), size);
            return size;
        }
        void close(void* f) override
        {
            opened--;
            delete static_cast<Open*>(f);
        }
        bool remove(const std::string& path, bool folder) override { return files.erase(folder ? path + "/" : path) != 0; }
        bool makeFolder(const std::string& path) override { files[path + "/"]; return true; }
        bool rename(const std::string& from, const std::string& to) override
        {
            files[to] = files[from];
            return files.erase(from) != 0;
        }
    };

    struct Session
    {
        FtpServer server;
        Pretend files;
        TcpSocket control;
        std::string in;
        double now = 1;

        explicit Session(bool write)
        {
            files.files = { { "/E/", "" }, { "/E/Games/", "" }, { "/E/Games/a.xbe", std::string(100000, 'x') }, { "/E/note.txt", "hello" } };
            netStart();
            NX_EXPECT(server.start(files, "me", "secret", write, 0, true));
            control.connect({ kLoopback, server.port() });
            reply();
        }
        ~Session()
        {
            control.close();
            server.stop();
            netStop();
        }
        void step(double seconds = 0.02)
        {
            now += seconds;
            server.update(now);
            control.update();
            sleepMilliseconds(1);
        }
        std::string reply()
        {
            for (int i = 0; i < 400; i++)
            {
                step();
                char b[512];
                const size_t got = control.receive(b, sizeof(b));
                in.append(b, got);
                const size_t end = in.find("\r\n");
                if (end != std::string::npos)
                {
                    const std::string line = in.substr(0, end);
                    in.erase(0, end + 2);
                    return line;
                }
            }
            return "";
        }
        std::string ask(const std::string& text)
        {
            const std::string line = text + "\r\n";
            control.send(line.data(), line.size());
            return reply();
        }
        bool says(const std::string& text, const char* code) { const std::string r = ask(text); return r.compare(0, 3, code) == 0; }
        bool enter() { return says("USER me", "331") && says("PASS secret", "230"); }
        // Everything a transfer sends, or what it is sent.
        std::string data(const std::string& command, const std::string* give = nullptr)
        {
            const std::string p = ask("EPSV");
            const size_t at = p.find("|||");
            if (at == std::string::npos)
            {
                return "!";
            }
            TcpSocket s;
            s.connect({ kLoopback, static_cast<u16>(atoi(p.c_str() + at + 3)) });
            if (ask(command).compare(0, 3, "150") != 0)
            {
                return "!";
            }
            std::string got;
            size_t put = 0;
            for (int i = 0; i < 2000; i++)
            {
                step();
                s.update();
                char b[4096];
                if (give && s.state() == TcpState::Open)
                {
                    put += s.send(give->data() + put, give->size() - put);
                    if (put == give->size())
                    {
                        s.close();
                        break;
                    }
                }
                else if (!give)
                {
                    const size_t n = s.receive(b, sizeof(b));
                    got.append(b, n);
                    if (n == 0 && s.state() == TcpState::Closed)
                    {
                        break;
                    }
                }
            }
            return reply().compare(0, 3, "226") == 0 ? got : "!";
        }
    };
}

NX_TEST(ftpPathsStayInside)
{
    std::string out;
    NX_EXPECT(FtpServer::resolve("/E", "Games/../x", out) && out == "/E/x");
    NX_EXPECT(FtpServer::resolve("/E", "../../../../", out) && out == "/");
    NX_EXPECT(FtpServer::resolve("/E", "/F//a/./b/", out) && out == "/F/a/b");
    for (const char* bad : { "C:", "a\\b", "x\ty", "..\\x", "a*", "q?" })
    {
        NX_EXPECT(!FtpServer::resolve("/E", bad, out));
    }
    NX_EXPECT(!FtpServer::resolve("/E", std::string(400, 'a'), out));
}

NX_TEST(ftpNeedsAPassword)
{
    Pretend files;
    FtpServer none;
    NX_EXPECT(!none.start(files, "", "secret", true, 0, true) && !none.start(files, "me", "", true, 0, true) && !none.start(files, "me", "abc", true, 0, true));
    Session s(false);
    NX_EXPECT(s.says("LIST", "530") && s.says("RETR /E/note.txt", "530") && s.says("PWD", "530"));
    NX_EXPECT(s.says("USER me", "331") && s.says("PASS wrong", "530"));
    // Straight after, even the right one.
    NX_EXPECT(s.says("PASS secret", "421"));
    s.step();
    s.step();
    NX_EXPECT(s.server.clients() == 0);
}

NX_TEST(ftpReads)
{
    Session s(false);
    NX_EXPECT(s.enter() && s.says("CWD /E/Games", "250") && s.ask("PWD") == "257 \"/E/Games\"");
    NX_EXPECT(s.data("LIST").find("a.xbe") != std::string::npos && s.data("LIST -al") != "!" && s.data("NLST ..") == "Games\r\nnote.txt\r\n");
    NX_EXPECT(s.data("RETR a.xbe") == std::string(100000, 'x') && s.ask("SIZE a.xbe") == "213 100000");
    NX_EXPECT(s.says("REST 99990", "350") && s.data("RETR a.xbe") == std::string(10, 'x'));
    NX_EXPECT(s.says("CDUP", "250") && s.data("RETR note.txt") == "hello" && s.says("CWD nowhere", "550"));
    const std::string give = "no";
    NX_EXPECT(s.data("STOR new.txt", &give) == "!" && s.says("DELE note.txt", "550") && s.says("MKD x", "550") && s.says("RNFR note.txt", "550"));
    // It won't be sent to call on others.
    NX_EXPECT(s.says("PORT 10,0,0,1,0,80", "502") && s.files.files.size() == 4 && s.files.opened == 0);
}

NX_TEST(ftpWritesOnlyIfItMay)
{
    Session s(true);
    NX_EXPECT(s.enter());
    const std::string give(70000, 'q');
    NX_EXPECT(s.data("STOR /E/new.bin", &give) != "!" && s.files.files["/E/new.bin"] == give);
    NX_EXPECT(s.says("MKD /E/made", "257") && s.says("RNFR /E/new.bin", "350") && s.says("RNTO /E/made/new.bin", "250") && s.files.files["/E/made/new.bin"].size() == 70000);
    NX_EXPECT(s.says("DELE /E/made/new.bin", "250") && s.says("RMD /E/made", "250"));
    // The drives themselves are safe.
    NX_EXPECT(s.says("RMD /E", "550") && s.says("DELE /E", "550") && s.says("STOR /x", "550") && s.says("MKD /Z", "550"));
    NX_EXPECT(s.says("REST 5", "350") && s.says("STOR /E/note.txt", "550"));
    // Nobody comes for it.
    s.ask("EPSV");
    NX_EXPECT(s.says("RETR /E/note.txt", "150"));
    s.step(30);
    NX_EXPECT(s.reply().compare(0, 3, "425") == 0 && s.files.opened == 0);
    s.step(400);
    s.step();
    NX_EXPECT(s.server.clients() == 0);
}
