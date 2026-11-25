#pragma once

#include "net/socket.h"

#include <string>
#include <vector>

// Files over the local network, to and from any FTP program. Nothing listens until it is started, with a user
// and a password; only machines on the same network are answered; and nothing is written unless that was asked for.
namespace nx
{
    struct FtpEntry
    {
        std::string name;
        u64 size = 0;
        bool folder = false;
    };

    // What it serves. Paths are like "/E/Games/thing.xbe", without "." or "..", and "/" is what there is.
    class FtpFiles
    {
    public:
        virtual ~FtpFiles() = default;
        virtual bool list(const std::string& path, std::vector<FtpEntry>& out) = 0;
        virtual bool find(const std::string& path, FtpEntry& out) = 0;
        // 0 if it can't.
        virtual void* open(const std::string& path, bool write) = 0;
        virtual bool seek(void* file, u64 at) = 0;
        // How much of it; less at the end, or if it can't.
        virtual size_t read(void* file, void* data, size_t size) = 0;
        virtual size_t write(void* file, const void* data, size_t size) = 0;
        virtual void close(void* file) = 0;
        virtual bool remove(const std::string& path, bool folder) = 0;
        virtual bool makeFolder(const std::string& path) = 0;
        virtual bool rename(const std::string& from, const std::string& to) = 0;
    };

    class FtpServer : NonCopyable
    {
    public:
        ~FtpServer() { stop(); }

        // False without a user and a password of some length, or a network. Port 0 is any that is free.
        bool start(FtpFiles& files, const std::string& user, const std::string& password, bool mayWrite, u16 port, bool onlyHere);
        void stop();
        bool running() const { return m_listener.state() == TcpState::Listening; }
        u16 port() const { return m_listener.port(); }
        u32 clients() const;
        u64 sent() const { return m_sent; }
        u64 received() const { return m_received; }
        // The latest thing that happened, for showing.
        const std::string& last() const { return m_last; }

        void update(double now);

        // "a/../b" and the like from somewhere, made plain. False for what may not be in one.
        static bool resolve(const std::string& from, const std::string& path, std::string& out);

    private:
        enum class Doing : u8 { Nothing, Listing, Sending, Receiving };

        struct Client
        {
            TcpSocket control, waiting, data;
            Endpoint other;
            std::string in, out, user, folder = "/", renaming, pending;
            bool used = false, entered = false;
            u32 tries = 0;
            Doing doing = Doing::Nothing;
            void* file = nullptr;
            u64 restart = 0;
            double heard = 0;
            double asked = 0;
            bool closing = false;
        };

        static constexpr u32 kClients = 4;

        void say(Client& c, const char* text);
        void command(Client& c, const std::string& line);
        bool passive(Client& c, bool extended);
        void transfer(Client& c, double now);
        void finish(Client& c, const char* text);
        void drop(Client& c);
        bool local(u32 ip) const;

        FtpFiles* m_files = nullptr;
        std::string m_user, m_password, m_last;
        bool m_mayWrite = false, m_onlyHere = false, m_counted = false;
        TcpSocket m_listener;
        Client m_clients[kClients];
        u64 m_sent = 0, m_received = 0;
        double m_now = 0;
        double m_refuseUntil = 0;
    };

    // The console's drives, or nullptr where there are none.
    FtpFiles* consoleFiles();
}
