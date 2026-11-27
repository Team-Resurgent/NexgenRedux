#include "net/websocket.h"
#include "base/log.h"
#include "net/secure.h"
#include "net/socket.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// RFC 6455, as a client: an HTTP upgrade, then frames. What the client sends is masked; what comes is not.
namespace nx
{
    namespace
    {
        constexpr double kPatience = 15;
        constexpr size_t kMostMessage = 4 * 1024 * 1024;

        u32 g_random = 0x2545F491u;

        u32 randomBits()
        {
            g_random ^= g_random << 13;
            g_random ^= g_random >> 17;
            g_random ^= g_random << 5;
            return g_random;
        }

        std::string base64(const u8* data, size_t size)
        {
            static const char kDigits[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
            std::string out;
            for (size_t i = 0; i < size; i += 3)
            {
                const u32 n = (static_cast<u32>(data[i]) << 16) | (i + 1 < size ? static_cast<u32>(data[i + 1]) << 8 : 0) | (i + 2 < size ? data[i + 2] : 0);
                out += kDigits[(n >> 18) & 63];
                out += kDigits[(n >> 12) & 63];
                out += i + 1 < size ? kDigits[(n >> 6) & 63] : '=';
                out += i + 2 < size ? kDigits[n & 63] : '=';
            }
            return out;
        }

        // A frame from the client: final, of a kind, masked.
        std::string frame(int kind, const std::string& payload)
        {
            std::string out;
            out += static_cast<char>(0x80 | kind);
            const size_t n = payload.size();
            if (n < 126)
            {
                out += static_cast<char>(0x80 | n);
            }
            else if (n < 65536)
            {
                out += static_cast<char>(0x80 | 126);
                out += static_cast<char>(n >> 8);
                out += static_cast<char>(n & 0xFF);
            }
            else
            {
                out += static_cast<char>(0x80 | 127);
                for (int i = 7; i >= 0; i--)
                {
                    out += static_cast<char>((static_cast<u64>(n) >> (i * 8)) & 0xFF);
                }
            }
            const u32 key = randomBits();
            const u8 mask[4] = { static_cast<u8>(key), static_cast<u8>(key >> 8), static_cast<u8>(key >> 16), static_cast<u8>(key >> 24) };
            out.append(reinterpret_cast<const char*>(mask), 4);
            for (size_t i = 0; i < n; i++)
            {
                out += static_cast<char>(payload[i] ^ mask[i & 3]);
            }
            return out;
        }

        struct Place
        {
            bool secure = false;
            std::string host, path;
            u16 port = 80;
        };

        bool parse(const std::string& url, Place& out)
        {
            out.secure = url.compare(0, 6, "wss://") == 0;
            if (!out.secure && url.compare(0, 5, "ws://") != 0)
            {
                return false;
            }
            for (const char c : url)
            {
                if (static_cast<u8>(c) <= 32 || c == 127)
                {
                    return false;
                }
            }
            const size_t start = out.secure ? 6 : 5;
            const size_t end = url.find_first_of("/?#", start);
            std::string host = url.substr(start, end == std::string::npos ? std::string::npos : end - start);
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
            if (host.empty() || host.size() > 200 || host.find('@') != std::string::npos)
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

        // Either kind of stream, never waiting to read.
        struct Stream
        {
            TcpSocket socket;
            SecureStream tls;
            bool secure = false;
            std::atomic<bool> never{ false };

            bool open(const Place& place, std::string& error)
            {
                secure = place.secure;
                u32 ip = 0;
                if (!netResolve(place.host.c_str(), ip))
                {
                    error = "there is nothing of that name";
                    return false;
                }
                if (!socket.connect({ ip, place.port }))
                {
                    error = "it could not be reached";
                    return false;
                }
                for (double waited = 0; socket.update() == TcpState::Connecting; waited += 0.004)
                {
                    if (waited > kPatience)
                    {
                        error = "it took too long";
                        return false;
                    }
                    sleepMilliseconds(4);
                }
                if (socket.state() != TcpState::Open)
                {
                    error = "it could not be reached";
                    return false;
                }
                if (secure && !tls.start(socket, place.host.c_str(), never, kPatience))
                {
                    error = tls.error();
                    return false;
                }
                return true;
            }

            bool send(const std::string& text)
            {
                if (secure)
                {
                    return tls.send(text.data(), text.size());
                }
                size_t at = 0;
                for (double waited = 0; at < text.size();)
                {
                    const size_t sent = socket.send(text.data() + at, text.size() - at);
                    at += sent;
                    if (sent == 0)
                    {
                        if (socket.state() != TcpState::Open || waited > kPatience)
                        {
                            return false;
                        }
                        sleepMilliseconds(4);
                        waited += 0.004;
                    }
                }
                return true;
            }

            // What has come, if anything; false once it has ended.
            bool take(std::string& into)
            {
                char buffer[8192];
                for (;;)
                {
                    bool ended = false;
                    const size_t got = secure ? tls.receiveNow(buffer, sizeof(buffer), ended) : socket.receive(buffer, sizeof(buffer));
                    if (got == 0)
                    {
                        return secure ? !ended : socket.state() == TcpState::Open;
                    }
                    into.append(buffer, got);
                }
            }
        };
    }

    WebSocket::WebSocket(const char* url)
    {
        m_shared = create<Shared>(MemTag::General);
        if (m_shared == nullptr)
        {
            return;
        }
        m_shared->url = url ? url : "";
        m_net = netStart();
        g_random ^= static_cast<u32>(reinterpret_cast<uintptr_t>(this));
        m_thread = startThread(run, m_shared, ThreadPriority::Normal);
        if (!m_thread)
        {
            m_shared->error = "no thread to talk on";
            m_shared->state.store(static_cast<int>(State::Closed), std::memory_order_release);
        }
    }

    WebSocket::~WebSocket()
    {
        if (m_shared)
        {
            m_shared->closing.store(true);
            if (m_thread)
            {
                joinThread(m_thread);
            }
            destroy(m_shared);
        }
        if (m_net)
        {
            netStop();
        }
    }

    std::string WebSocket::error() const
    {
        if (m_shared == nullptr)
        {
            return "no memory";
        }
        Lock hold(m_shared->lock);
        return m_shared->error;
    }

    bool WebSocket::send(const std::string& text)
    {
        if (m_shared == nullptr || state() == State::Closed || m_shared->closing.load())
        {
            return false;
        }
        Lock hold(m_shared->lock);
        m_shared->outgoing += frame(1, text);
        return true;
    }

    bool WebSocket::receive(std::string& out)
    {
        if (m_shared == nullptr)
        {
            return false;
        }
        Lock hold(m_shared->lock);
        std::string& in = m_shared->incoming;
        size_t& at = m_shared->read;
        if (at + 4 > in.size())
        {
            return false;
        }
        const u32 length = static_cast<u32>(static_cast<u8>(in[at])) | static_cast<u32>(static_cast<u8>(in[at + 1])) << 8 |
                           static_cast<u32>(static_cast<u8>(in[at + 2])) << 16 | static_cast<u32>(static_cast<u8>(in[at + 3])) << 24;
        out.assign(in, at + 4, length);
        at += 4 + length;
        // What has been read goes, now and then.
        if (at > 65536 && at * 2 > in.size())
        {
            in.erase(0, at);
            at = 0;
        }
        return true;
    }

    void WebSocket::close()
    {
        if (m_shared)
        {
            m_shared->closing.store(true);
        }
    }

    void WebSocket::run(void* data)
    {
        Shared& s = *static_cast<Shared*>(data);
        const auto fail = [&](const char* why)
        {
            Lock hold(s.lock);
            if (s.error.empty() && !s.closing.load())
            {
                s.error = why;
            }
            s.state.store(static_cast<int>(State::Closed), std::memory_order_release);
        };
        Place place;
        if (!parse(s.url, place))
        {
            fail("not a ws:// or wss:// address");
            return;
        }
        Stream stream;
        std::string why;
        if (!stream.open(place, why))
        {
            fail(why.c_str());
            return;
        }
        // The upgrade.
        u8 nonce[16];
        for (u32 i = 0; i < 16; i += 4)
        {
            const u32 r = randomBits();
            memcpy(nonce + i, &r, 4);
        }
        char port[16] = "";
        if (place.port != (place.secure ? 443 : 80))
        {
            snprintf(port, sizeof(port), ":%u", static_cast<unsigned>(place.port));
        }
        const std::string hello = "GET " + place.path + " HTTP/1.1\r\nHost: " + place.host + port + "\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: " +
                                  base64(nonce, 16) + "\r\nSec-WebSocket-Version: 13\r\nUser-Agent: Nexgen\r\n\r\n";
        if (!stream.send(hello))
        {
            fail("it could not be reached");
            return;
        }
        std::string in;
        size_t headers = std::string::npos;
        for (double waited = 0; headers == std::string::npos; waited += 0.004)
        {
            if (!stream.take(in) || waited > kPatience || s.closing.load())
            {
                fail(s.closing.load() ? "closed" : "no answer");
                return;
            }
            headers = in.find("\r\n\r\n");
            if (headers == std::string::npos)
            {
                sleepMilliseconds(4);
            }
        }
        if (in.compare(0, 12, "HTTP/1.1 101") != 0)
        {
            const size_t line = in.find("\r\n");
            Lock hold(s.lock);
            s.error = "refused: " + in.substr(0, line);
            s.state.store(static_cast<int>(State::Closed), std::memory_order_release);
            return;
        }
        in.erase(0, headers + 4);
        s.state.store(static_cast<int>(State::Open), std::memory_order_release);

        std::string message;     // of frames that continue
        bool sentClose = false;
        for (;;)
        {
            // What the game has written.
            std::string out;
            {
                Lock hold(s.lock);
                out.swap(s.outgoing);
            }
            if (s.closing.load() && !sentClose)
            {
                out += frame(8, std::string("\x03\xE8", 2));   // 1000: done with
                sentClose = true;
            }
            if (!out.empty() && !stream.send(out))
            {
                fail("the connection broke");
                return;
            }
            if (sentClose)
            {
                // Its own close, a little while, and then whatever it is doing.
                for (u32 i = 0; i < 50 && stream.take(in); i++)
                {
                    sleepMilliseconds(4);
                }
                s.state.store(static_cast<int>(State::Closed), std::memory_order_release);
                return;
            }
            const size_t before = in.size();
            if (!stream.take(in))
            {
                fail("the server closed it");
                return;
            }
            // Each whole frame.
            for (;;)
            {
                if (in.size() < 2)
                {
                    break;
                }
                const u8 b0 = static_cast<u8>(in[0]), b1 = static_cast<u8>(in[1]);
                size_t at = 2;
                u64 n = b1 & 0x7F;
                if (n == 126)
                {
                    if (in.size() < 4)
                    {
                        break;
                    }
                    n = static_cast<u64>(static_cast<u8>(in[2])) << 8 | static_cast<u8>(in[3]);
                    at = 4;
                }
                else if (n == 127)
                {
                    if (in.size() < 10)
                    {
                        break;
                    }
                    n = 0;
                    for (int i = 0; i < 8; i++)
                    {
                        n = n << 8 | static_cast<u8>(in[2 + i]);
                    }
                    at = 10;
                }
                const bool masked = (b1 & 0x80) != 0;
                if (n > kMostMessage)
                {
                    fail("a message too large");
                    return;
                }
                if (in.size() < at + (masked ? 4 : 0) + n)
                {
                    break;
                }
                std::string payload = in.substr(at + (masked ? 4 : 0), static_cast<size_t>(n));
                if (masked)
                {
                    for (size_t i = 0; i < payload.size(); i++)
                    {
                        payload[i] = static_cast<char>(payload[i] ^ in[at + (i & 3)]);
                    }
                }
                in.erase(0, at + (masked ? 4 : 0) + static_cast<size_t>(n));
                const int kind = b0 & 0x0F;
                const bool last = (b0 & 0x80) != 0;
                if (kind == 8)
                {
                    stream.send(frame(8, payload.substr(0, 2)));
                    fail("the server closed it");
                    return;
                }
                if (kind == 9)
                {
                    stream.send(frame(10, payload));
                    continue;
                }
                if (kind == 10)
                {
                    continue;
                }
                message += payload;
                if (message.size() > kMostMessage)
                {
                    fail("a message too large");
                    return;
                }
                if (last)
                {
                    Lock hold(s.lock);
                    const u32 length = static_cast<u32>(message.size());
                    const char sizeBytes[4] = { static_cast<char>(length), static_cast<char>(length >> 8), static_cast<char>(length >> 16), static_cast<char>(length >> 24) };
                    s.incoming.append(sizeBytes, 4);
                    s.incoming += message;
                    message.clear();
                }
            }
            if (in.size() == before && out.empty())
            {
                sleepMilliseconds(4);
            }
        }
    }
}
