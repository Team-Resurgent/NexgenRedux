#include "net/net.h"
#include "base/log.h"
#include "net/secure.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Asking a web server for something, and reading its answer. On a worker thread, which may wait.
namespace nx
{
    namespace
    {
        constexpr double kPatience = 15;   // with nothing happening

        bool sameWord(const char* a, const char* b, size_t length)
        {
            for (size_t i = 0; i < length; i++)
            {
                const char x = a[i] >= 'A' && a[i] <= 'Z' ? static_cast<char>(a[i] + 32) : a[i];
                if (x != b[i])
                {
                    return false;
                }
            }
            return true;
        }

        struct Place
        {
            bool secure;
            std::string host, path;
            u16 port;
        };

        bool parse(const std::string& url, Place& out)
        {
            const bool plain = url.compare(0, 7, "http://") == 0;
            out.secure = url.compare(0, 8, "https://") == 0;
            // Nothing that could end a line early and add one of its own.
            for (const char c : url)
            {
                if (static_cast<u8>(c) <= 32 || c == 127)
                {
                    return false;
                }
            }
            if (!plain && !out.secure)
            {
                return false;
            }
            const size_t start = out.secure ? 8 : 7;
            const size_t end = url.find_first_of("/?#", start);
            std::string host = url.substr(start, end == std::string::npos ? std::string::npos : end - start);
            if (host.find('@') != std::string::npos)
            {
                return false;
            }
            out.port = out.secure ? 443 : 80;
            const size_t colon = host.find(':');
            if (colon != std::string::npos)
            {
                const long port = strtol(host.c_str() + colon + 1, nullptr, 10);
                if (port < 1 || port > 65535)
                {
                    return false;
                }
                out.port = static_cast<u16>(port);
                host.resize(colon);
            }
            if (host.empty() || host.size() > 200)
            {
                return false;
            }
            out.host = host;
            out.path = end == std::string::npos ? "/" : url.substr(end, url.find('#', end) - end);
            if (out.path.empty() || out.path[0] != '/')
            {
                out.path = "/" + out.path;
            }
            return true;
        }

        // Either kind of stream.
        class Stream : NonCopyable
        {
        public:
            explicit Stream(Fetch& fetch) : m_fetch(fetch) {}

            bool open(const Place& place)
            {
                m_secure = place.secure;
                u32 ip = 0;
                if (!netResolve(place.host.c_str(), ip))
                {
                    m_fetch.error = "there is nothing of that name";
                    return false;
                }
                if (!m_socket.connect({ ip, place.port }))
                {
                    m_fetch.error = "it could not be reached";
                    return false;
                }
                for (double waited = 0; m_socket.update() == TcpState::Connecting; waited += 0.004)
                {
                    if (waited > kPatience || m_fetch.cancelled.load())
                    {
                        m_fetch.error = "it took too long";
                        return false;
                    }
                    sleepMilliseconds(4);
                }
                if (m_socket.state() != TcpState::Open)
                {
                    m_fetch.error = "it could not be reached";
                    return false;
                }
                if (m_secure && !m_tls.start(m_socket, place.host.c_str(), m_fetch.cancelled, kPatience))
                {
                    m_fetch.error = m_tls.error();
                    return false;
                }
                return true;
            }

            bool send(const std::string& text)
            {
                if (m_secure)
                {
                    return m_tls.send(text.data(), text.size());
                }
                size_t at = 0;
                double waited = 0;
                while (at < text.size())
                {
                    const size_t sent = m_socket.send(text.data() + at, text.size() - at);
                    at += sent;
                    if (sent == 0)
                    {
                        if (m_socket.state() != TcpState::Open || waited > kPatience || m_fetch.cancelled.load())
                        {
                            return false;
                        }
                        sleepMilliseconds(4);
                        waited += 0.004;
                    }
                }
                return true;
            }

            // 0 at the end.
            size_t receive(char* data, size_t size)
            {
                if (m_secure)
                {
                    return m_tls.receive(data, size);
                }
                for (double waited = 0; waited < kPatience && !m_fetch.cancelled.load(); waited += 0.004)
                {
                    const size_t got = m_socket.receive(data, size);
                    if (got || m_socket.state() != TcpState::Open)
                    {
                        return got;
                    }
                    sleepMilliseconds(4);
                }
                return 0;
            }

        private:
            Fetch& m_fetch;
            TcpSocket m_socket;
            SecureStream m_tls;   // closed first
            bool m_secure = false;
        };

        struct Headers
        {
            long length = -1;
            bool chunked = false;
            std::string moved;
        };

        void readHeaders(const std::string& in, size_t end, Headers& out)
        {
            size_t line = in.find("\r\n") + 2;
            while (line < end + 2)
            {
                const size_t stop = in.find("\r\n", line);
                const char* text = in.c_str() + line;
                const size_t length = stop - line;
                const auto value = [&](size_t name)
                {
                    size_t from = name;
                    while (from < length && text[from] == ' ')
                    {
                        from++;
                    }
                    return std::string(text + from, length - from);
                };
                if (length > 15 && sameWord(text, "content-length:", 15))
                {
                    out.length = atol(value(15).c_str());
                }
                else if (length > 18 && sameWord(text, "transfer-encoding:", 18))
                {
                    out.chunked = value(18).find("chunked") != std::string::npos;
                }
                else if (length > 9 && sameWord(text, "location:", 9))
                {
                    out.moved = value(9);
                }
                line = stop + 2;
            }
        }

        // Pieces, each after its length. False if any of it is missing.
        bool unchunk(const std::string& in, size_t at, std::string& out)
        {
            for (;;)
            {
                const size_t end = in.find("\r\n", at);
                if (end == std::string::npos)
                {
                    return false;
                }
                const unsigned long size = strtoul(in.c_str() + at, nullptr, 16);
                at = end + 2;
                if (size == 0)
                {
                    return true;
                }
                if (size > in.size() - at)
                {
                    return false;
                }
                out.append(in, at, size);
                at += size + 2;
                if (at > in.size())
                {
                    return false;
                }
            }
        }
    }

    bool httpFetch(Fetch& fetch)
    {
        std::string url = fetch.url;
        for (u32 moves = 0; moves < 6; moves++)
        {
            Place place;
            if (!parse(url, place))
            {
                fetch.error = "not a web address";
                return false;
            }
            Stream stream(fetch);
            if (!stream.open(place))
            {
                return false;
            }
            char number[32] = "";
            if (place.port != (place.secure ? 443 : 80))
            {
                snprintf(number, sizeof(number), ":%u", static_cast<unsigned>(place.port));
            }
            std::string out = fetch.method + " " + place.path + " HTTP/1.1\r\nHost: " + place.host + number + "\r\nUser-Agent: Nexgen\r\nAccept: */*\r\nConnection: close\r\n";
            if (!fetch.sent.empty())
            {
                snprintf(number, sizeof(number), "%u", static_cast<unsigned>(fetch.sent.size()));
                out += "Content-Type: " + fetch.type + "\r\nContent-Length: " + number + "\r\n";
            }
            out += "\r\n" + fetch.sent;
            if (!stream.send(out))
            {
                fetch.error = "it stopped listening";
                return false;
            }
            std::string in;
            Headers headers;
            size_t body = 0;
            char buffer[4096];
            for (;;)
            {
                const size_t got = stream.receive(buffer, sizeof(buffer));
                if (got == 0)
                {
                    break;
                }
                in.append(buffer, got);
                if (in.size() > fetch.most + 65536)
                {
                    fetch.error = "the answer is too long";
                    return false;
                }
                if (body == 0)
                {
                    const size_t end = in.find("\r\n\r\n");
                    if (end == std::string::npos)
                    {
                        if (in.size() > 65536)
                        {
                            fetch.error = "that isn't a web server";
                            return false;
                        }
                        continue;
                    }
                    if (in.compare(0, 5, "HTTP/") != 0)
                    {
                        fetch.error = "that isn't a web server";
                        return false;
                    }
                    body = end + 4;
                    const size_t space = in.find(' ');
                    fetch.status = space < end ? atoi(in.c_str() + space + 1) : 0;
                    readHeaders(in, end, headers);
                    fetch.expected.store(headers.length > 0 ? static_cast<u32>(headers.length) : 0, std::memory_order_relaxed);
                }
                fetch.received.store(static_cast<u32>(in.size() - body), std::memory_order_relaxed);
                const bool whole = !headers.chunked && headers.length >= 0 && in.size() >= body + static_cast<size_t>(headers.length);
                const bool last = headers.chunked && in.size() >= body + 5 && in.compare(in.size() - 5, 5, "0\r\n\r\n") == 0;
                if (whole || last)
                {
                    break;
                }
            }
            if (fetch.cancelled.load())
            {
                fetch.error = "called off";
                return false;
            }
            if (body == 0)
            {
                fetch.error = "the answer was cut short";
                return false;
            }
            if (fetch.status >= 300 && fetch.status < 400 && !headers.moved.empty())
            {
                if (headers.moved[0] == '/')
                {
                    const bool usual = place.port == (place.secure ? 443 : 80);
                    snprintf(number, sizeof(number), ":%u", static_cast<unsigned>(place.port));
                    headers.moved = std::string(place.secure ? "https://" : "http://") + place.host + (usual ? "" : number) + headers.moved;
                }
                // From what is secure, only to what is secure.
                if (place.secure && headers.moved.compare(0, 8, "https://") != 0)
                {
                    fetch.error = "it sends on to somewhere that isn't secure";
                    return false;
                }
                url = headers.moved;
                continue;
            }
            if (headers.chunked)
            {
                if (!unchunk(in, body, fetch.text))
                {
                    fetch.error = "the answer was cut short";
                    return false;
                }
            }
            else
            {
                fetch.text = in.substr(body);
                if (headers.length >= 0 && fetch.text.size() < static_cast<size_t>(headers.length))
                {
                    fetch.text.clear();
                    fetch.error = "the answer was cut short";
                    return false;
                }
            }
            return true;
        }
        fetch.error = "it sends on for ever";
        return false;
    }

    namespace
    {
        Mutex g_webLock;
        Fetch* g_web[4] = {};
        bool g_webStarted = false, g_webStopped = false;
    }

    void webStop()
    {
        Lock lock(g_webLock);
        g_webStopped = true;
        for (Fetch* fetch : g_web)
        {
            if (fetch)
            {
                fetch->cancelled.store(true);
            }
        }
    }

    bool webRead(const char* url, FileData& out)
    {
        Fetch fetch;
        fetch.url = url;
        {
            Lock lock(g_webLock);
            if (g_webStopped)
            {
                return false;
            }
            g_webStarted = g_webStarted || netStart();
            for (Fetch*& slot : g_web)
            {
                if (slot == nullptr)
                {
                    slot = &fetch;
                    break;
                }
            }
        }
        const bool ok = g_webStarted && httpFetch(fetch) && fetch.status == 200;
        {
            Lock lock(g_webLock);
            for (Fetch*& slot : g_web)
            {
                slot = slot == &fetch ? nullptr : slot;
            }
        }
        if (!ok)
        {
            NX_WARN("%s: %s", url, fetch.error[0] ? fetch.error : "the server refused");
            return false;
        }
        u8* data = static_cast<u8*>(memAlloc(fetch.text.size() + 1, MemTag::Texture));
        if (data == nullptr)
        {
            return false;
        }
        memcpy(data, fetch.text.data(), fetch.text.size());
        out.take(data, fetch.text.size());
        return true;
    }
}
