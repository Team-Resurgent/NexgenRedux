#include "net/ftp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace nx
{
    namespace
    {
        constexpr double kIdle = 300;          // seconds
        constexpr double kWaitForData = 20;
        constexpr size_t kPiece = 16384;
        constexpr u32 kPiecesAtOnce = 24;      // in an update, so that a frame is never long

        std::string upper(std::string text)
        {
            for (char& c : text)
            {
                c = c >= 'a' && c <= 'z' ? static_cast<char>(c - 32) : c;
            }
            return text;
        }

        // In the same time whatever the answer.
        bool same(const std::string& a, const std::string& b)
        {
            size_t different = a.size() ^ b.size();
            for (size_t i = 0; i < a.size(); i++)
            {
                different |= static_cast<size_t>(a[i] ^ b[i % (b.empty() ? 1 : b.size())]);
            }
            return different == 0;
        }
    }

    bool FtpServer::resolve(const std::string& from, const std::string& path, std::string& out)
    {
        std::vector<std::string> parts;
        const std::string whole = !path.empty() && path[0] == '/' ? path : from + "/" + path;
        if (whole.size() > 230)
        {
            return false;
        }
        size_t at = 0;
        while (at < whole.size())
        {
            const size_t end = whole.find('/', at);
            const std::string part = whole.substr(at, end == std::string::npos ? std::string::npos : end - at);
            at = end == std::string::npos ? whole.size() : end + 1;
            if (part.empty() || part == ".")
            {
                continue;
            }
            if (part == "..")
            {
                if (!parts.empty())
                {
                    parts.pop_back();
                }
                continue;
            }
            for (const char c : part)
            {
                if (static_cast<u8>(c) < 32 || strchr("\\:*?\"<>|", c))
                {
                    return false;
                }
            }
            parts.push_back(part);
        }
        out = "/";
        for (size_t i = 0; i < parts.size(); i++)
        {
            out += (i ? "/" : "") + parts[i];
        }
        return true;
    }

    bool FtpServer::local(u32 ip) const
    {
        const u32 a = ip >> 24, b = ip >> 16 & 255;
        return a == 127 || (!m_onlyHere && (a == 10 || (a == 172 && b >= 16 && b < 32) || (a == 192 && b == 168) || (a == 169 && b == 254)));
    }

    bool FtpServer::start(FtpFiles& files, const std::string& user, const std::string& password, bool mayWrite, u16 port, bool onlyHere)
    {
        stop();
        if (user.empty() || password.size() < 4 || !netStart())
        {
            return false;
        }
        m_counted = true;
        if (!m_listener.listen(port, onlyHere))
        {
            stop();
            return false;
        }
        m_files = &files;
        m_user = user;
        m_password = password;
        m_mayWrite = mayWrite;
        m_onlyHere = onlyHere;
        m_sent = m_received = 0;
        m_last = "waiting";
        return true;
    }

    void FtpServer::stop()
    {
        for (Client& c : m_clients)
        {
            drop(c);
        }
        m_listener.close();
        m_files = nullptr;
        if (m_counted)
        {
            m_counted = false;
            netStop();
        }
    }

    u32 FtpServer::clients() const
    {
        u32 count = 0;
        for (const Client& c : m_clients)
        {
            count += c.used;
        }
        return count;
    }

    void FtpServer::drop(Client& c)
    {
        if (c.file && m_files)
        {
            m_files->close(c.file);
        }
        c.file = nullptr;
        c.control.close();
        c.waiting.close();
        c.data.close();
        c.in.clear();
        c.out.clear();
        c.pending.clear();
        c.user.clear();
        c.renaming.clear();
        c.folder = "/";
        c.used = c.entered = c.closing = false;
        c.tries = 0;
        c.restart = 0;
        c.doing = Doing::Nothing;
    }

    void FtpServer::say(Client& c, const char* text)
    {
        c.out += text;
        c.out += "\r\n";
    }

    bool FtpServer::passive(Client& c, bool extended)
    {
        c.data.close();
        c.waiting.close();
        u32 ip = netLocalAddress();
        if (m_onlyHere || c.other.ip >> 24 == 127)
        {
            ip = kLoopback;
        }
        if (!c.waiting.listen(0, m_onlyHere))
        {
            return false;
        }
        char text[96];
        const unsigned port = c.waiting.port();
        if (extended)
        {
            snprintf(text, sizeof(text), "229 Ready (|||%u|)", port);
        }
        else
        {
            snprintf(text, sizeof(text), "227 Ready (%u,%u,%u,%u,%u,%u)", static_cast<unsigned>(ip >> 24), static_cast<unsigned>(ip >> 16 & 255), static_cast<unsigned>(ip >> 8 & 255), static_cast<unsigned>(ip & 255), port >> 8, port & 255);
        }
        say(c, text);
        return true;
    }

    void FtpServer::command(Client& c, const std::string& line)
    {
        const size_t space = line.find(' ');
        const std::string word = upper(line.substr(0, space));
        std::string rest = space == std::string::npos ? "" : line.substr(space + 1);
        if (word == "USER")
        {
            c.user = rest;
            c.entered = false;
            say(c, "331 And the password?");
            return;
        }
        if (word == "PASS")
        {
            // Wrong answers slow everyone down, so that guessing is no use.
            if (m_now < m_refuseUntil)
            {
                say(c, "421 Too soon after a wrong password");
                c.closing = true;
            }
            else if (same(c.user, m_user) && same(rest, m_password))
            {
                c.entered = true;
                m_last = "someone came in";
                say(c, "230 Come in");
            }
            else
            {
                c.tries++;
                m_refuseUntil = m_now + 2;
                m_last = "a wrong password";
                say(c, c.tries >= 3 ? "421 No" : "530 No");
                c.closing = c.tries >= 3;
            }
            return;
        }
        if (word == "QUIT")
        {
            say(c, "221 Goodbye");
            c.closing = true;
            return;
        }
        if (word == "SYST") { say(c, "215 UNIX Type: L8"); return; }
        if (word == "FEAT") { say(c, "211-What there is\r\n SIZE\r\n REST STREAM\r\n EPSV\r\n UTF8\r\n211 End"); return; }
        if (word == "NOOP" || word == "OPTS") { say(c, "200 Yes"); return; }
        if (!c.entered)
        {
            say(c, "530 Say who you are first");
            return;
        }
        std::string path;
        // Some programs ask for the listing with options.
        const bool named = resolve(c.folder, (word == "LIST" || word == "NLST") && !rest.empty() && rest[0] == '-' ? "" : rest, path);
        const bool changes = word == "STOR" || word == "DELE" || word == "MKD" || word == "RMD" || word == "RNFR" || word == "RNTO" || word == "XMKD" || word == "XRMD";
        if (!named)
        {
            say(c, "553 Not a path");
            return;
        }
        if (changes && !m_mayWrite)
        {
            say(c, "550 It can only be read");
            return;
        }
        // The drives themselves are not for changing.
        if (changes && path.find('/', 1) == std::string::npos)
        {
            say(c, "550 Not there");
            return;
        }
        FtpEntry entry;
        char text[320];
        if (word == "PWD" || word == "XPWD")
        {
            say(c, ("257 \"" + c.folder + "\"").c_str());
        }
        else if (word == "CWD" || word == "XCWD" || word == "CDUP" || word == "XCUP")
        {
            const bool ok = resolve(c.folder, word[0] == 'C' && word[1] == 'D' ? ".." : word == "XCUP" ? ".." : rest, path) && (path == "/" || (m_files->find(path, entry) && entry.folder));
            c.folder = ok ? path : c.folder;
            say(c, ok ? "250 Yes" : "550 There is no such folder");
        }
        else if (word == "TYPE" || word == "MODE" || word == "STRU" || word == "ALLO")
        {
            say(c, "200 Yes");
        }
        else if (word == "PASV" || word == "EPSV")
        {
            if (!passive(c, word == "EPSV"))
            {
                say(c, "425 It couldn't");
            }
        }
        else if (word == "REST")
        {
            c.restart = strtoull(rest.c_str(), nullptr, 10);
            say(c, "350 From there");
        }
        else if (word == "SIZE")
        {
            if (m_files->find(path, entry) && !entry.folder)
            {
                snprintf(text, sizeof(text), "213 %llu", static_cast<unsigned long long>(entry.size));
                say(c, text);
            }
            else
            {
                say(c, "550 There is no such file");
            }
        }
        else if (word == "LIST" || word == "NLST")
        {
            std::vector<FtpEntry> entries;
            if (c.doing != Doing::Nothing || !m_files->list(path, entries))
            {
                say(c, "550 It can't be listed");
                return;
            }
            c.pending.clear();
            for (const FtpEntry& e : entries)
            {
                if (word == "NLST")
                {
                    c.pending += e.name + "\r\n";
                    continue;
                }
                snprintf(text, sizeof(text), "%srwxr-xr-x 1 xbox xbox %llu Jan  1  2002 ", e.folder ? "d" : "-", static_cast<unsigned long long>(e.size));
                c.pending += text + e.name + "\r\n";
            }
            c.doing = Doing::Listing;
            c.asked = m_now;
            say(c, "150 Here it comes");
        }
        else if (word == "RETR" || word == "STOR")
        {
            const bool write = word == "STOR";
            if (c.doing != Doing::Nothing)
            {
                say(c, "450 One at a time");
                return;
            }
            c.file = m_files->open(path, write);
            if (c.file && c.restart && (write || !m_files->seek(c.file, c.restart)))
            {
                m_files->close(c.file);
                c.file = nullptr;
            }
            c.restart = 0;
            if (c.file == nullptr)
            {
                say(c, "550 It can't be opened");
                return;
            }
            c.doing = write ? Doing::Receiving : Doing::Sending;
            c.asked = m_now;
            c.pending.clear();
            m_last = (write ? "receiving " : "sending ") + path;
            say(c, "150 Ready");
        }
        else if (word == "DELE" || word == "RMD" || word == "XRMD")
        {
            const bool ok = m_files->remove(path, word != "DELE");
            m_last = ok ? "removed " + path : m_last;
            say(c, ok ? "250 Gone" : "550 It couldn't be");
        }
        else if (word == "MKD" || word == "XMKD")
        {
            say(c, m_files->makeFolder(path) ? ("257 \"" + path + "\"").c_str() : "550 It couldn't be");
        }
        else if (word == "RNFR")
        {
            const bool ok = m_files->find(path, entry);
            c.renaming = ok ? path : "";
            say(c, ok ? "350 And to what?" : "550 There is no such thing");
        }
        else if (word == "RNTO")
        {
            say(c, !c.renaming.empty() && m_files->rename(c.renaming, path) ? "250 Done" : "550 It couldn't be");
            c.renaming.clear();
        }
        else
        {
            // Not PORT: that would have it call on machines that someone else names.
            say(c, "502 It can't do that");
        }
    }

    void FtpServer::finish(Client& c, const char* text)
    {
        if (c.file)
        {
            m_files->close(c.file);
            c.file = nullptr;
        }
        c.data.close();
        c.waiting.close();
        c.pending.clear();
        c.doing = Doing::Nothing;
        say(c, text);
    }

    void FtpServer::transfer(Client& c, double now)
    {
        if (c.data.state() == TcpState::Closed)
        {
            // Only whoever asked.
            Endpoint from;
            if (c.waiting.state() == TcpState::Listening && c.waiting.accept(c.data, from) && from.ip != c.other.ip)
            {
                c.data.close();
            }
            if (c.data.state() == TcpState::Closed)
            {
                if (now - c.asked > kWaitForData)
                {
                    finish(c, "425 Nobody came for it");
                }
                return;
            }
            c.waiting.close();
        }
        c.heard = now;
        static char piece[kPiece];
        for (u32 i = 0; i < kPiecesAtOnce && c.doing != Doing::Nothing; i++)
        {
            if (c.doing == Doing::Receiving)
            {
                const size_t got = c.data.receive(piece, sizeof(piece));
                if (got && m_files->write(c.file, piece, got) != got)
                {
                    finish(c, "452 There is no room");
                    m_last = "no room";
                    return;
                }
                m_received += got;
                if (got == 0)
                {
                    if (c.data.state() != TcpState::Open)
                    {
                        finish(c, "226 All of it");
                        m_last = "received";
                    }
                    return;
                }
                continue;
            }
            if (c.pending.empty())
            {
                if (c.doing == Doing::Listing)
                {
                    finish(c, "226 That is all");
                    return;
                }
                const size_t got = m_files->read(c.file, piece, sizeof(piece));
                if (got == 0)
                {
                    finish(c, "226 That is all");
                    m_last = "sent";
                    return;
                }
                c.pending.assign(piece, got);
            }
            const size_t sent = c.data.send(c.pending.data(), c.pending.size());
            m_sent += sent;
            c.pending.erase(0, sent);
            if (c.data.state() != TcpState::Open)
            {
                finish(c, "426 They went");
                return;
            }
            if (sent == 0)
            {
                return;
            }
        }
    }

    void FtpServer::update(double now)
    {
        m_now = now;
        if (!running())
        {
            return;
        }
        for (u32 tries = 0; tries < 4; tries++)
        {
            TcpSocket fresh;
            Endpoint from;
            Client* room = nullptr;
            for (Client& c : m_clients)
            {
                room = c.used ? room : &c;
            }
            if (!m_listener.accept(room ? room->control : fresh, from))
            {
                break;
            }
            if (room == nullptr)
            {
                fresh.close();
                continue;
            }
            if (!local(from.ip))
            {
                room->control.close();
                continue;
            }
            room->used = true;
            room->other = from;
            room->heard = now;
            say(*room, "220 Nexgen");
        }
        for (Client& c : m_clients)
        {
            if (!c.used)
            {
                continue;
            }
            char piece[1024];
            const size_t got = c.control.receive(piece, sizeof(piece));
            if (got)
            {
                c.heard = now;
                c.in.append(piece, got);
            }
            size_t end;
            while (!c.closing && (end = c.in.find('\n')) != std::string::npos)
            {
                std::string line = c.in.substr(0, end);
                c.in.erase(0, end + 1);
                while (!line.empty() && (line.back() == '\r' || line.back() == ' '))
                {
                    line.pop_back();
                }
                command(c, line);
            }
            if (c.doing != Doing::Nothing)
            {
                transfer(c, now);
            }
            if (!c.out.empty())
            {
                c.out.erase(0, c.control.send(c.out.data(), c.out.size()));
            }
            const bool gone = c.control.state() != TcpState::Open;
            if (gone || (c.closing && c.out.empty()) || c.in.size() > 4096 || c.out.size() > 65536 || now - c.heard > kIdle)
            {
                drop(c);
            }
        }
    }
}
