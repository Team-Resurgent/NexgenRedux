#include "net/net.h"
#include "base/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace nx
{
    namespace
    {
        constexpr size_t kMostText = 512 * 1024;   // of an answer, or waiting to be read
    }

    bool Net::start()
    {
        if (!m_started && !m_failed)
        {
            m_started = netStart();
            m_failed = !m_started;
            if (m_failed)
            {
                NX_WARN("there is no network");
            }
        }
        return m_started;
    }

    void Net::shutdown()
    {
        m_ftp.stop();
        for (u32 i = 0; i < kWorkers; i++)
        {
            m_jobs[i].stop();
            m_working[i] = false;
        }
        if (m_started)
        {
            m_started = false;
            netStop();
        }
    }

    void Net::update(double now)
    {
        m_ftp.update(now);
        m_now = now;
        for (u32 i = 0; i < m_rooms.count(); i++)
        {
            m_rooms[i]->update(now);
        }
        for (u32 i = 0; i < m_connections.count(); i++)
        {
            m_connections[i]->update();
        }
    }

    // --- Names ---

    namespace
    {
        void runLookup(void* user)
        {
            Lookup* lookup = static_cast<Lookup*>(user);
            u32 ip = 0;
            const bool ok = netResolve(lookup->name.c_str(), ip);
            lookup->ip = ip;
            lookup->state.store(ok ? 1 : 2, std::memory_order_release);
            Net::letGo(lookup);
        }
    }

    Lookup* Net::lookUp(const char* name)
    {
        if (!start())
        {
            return nullptr;
        }
        Lookup* lookup = create<Lookup>(MemTag::General);
        if (lookup == nullptr)
        {
            return nullptr;
        }
        lookup->name = name;
        const u32 worker = m_nextWorker++ % kWorkers;
        m_working[worker] = m_working[worker] || m_jobs[worker].start();
        if (!m_working[worker] || !m_jobs[worker].add(runLookup, lookup))
        {
            runLookup(lookup);
        }
        return lookup;
    }

    void Net::letGo(Lookup* lookup)
    {
        if (lookup->users.fetch_sub(1) == 1)
        {
            destroy(lookup);
        }
    }

    // --- Requests ---

    Request::Request(Net& net, const char* url, const char* method, const std::string& body, const char* type)
    {
        if (!net.start())
        {
            return;
        }
        m_fetch = create<Fetch>(MemTag::General);
        if (m_fetch == nullptr)
        {
            return;
        }
        m_fetch->url = url;
        m_fetch->method = method;
        m_fetch->sent = body;
        m_fetch->type = type;
        m_fetch->users.store(2);
        const u32 worker = net.m_nextWorker++ % Net::kWorkers;
        net.m_working[worker] = net.m_working[worker] || net.m_jobs[worker].start();
        if (!net.m_working[worker] || !net.m_jobs[worker].add(run, m_fetch))
        {
            // With no thread to ask from, the game waits.
            run(m_fetch);
        }
    }

    Request::~Request()
    {
        if (m_fetch)
        {
            m_fetch->cancelled.store(true);
            letGo(m_fetch);
        }
    }

    void Request::letGo(Fetch* fetch)
    {
        if (fetch->users.fetch_sub(1) == 1)
        {
            destroy(fetch);
        }
    }

    void Request::run(void* user)
    {
        Fetch* fetch = static_cast<Fetch*>(user);
        const bool ok = !fetch->cancelled.load() && httpFetch(*fetch);
        fetch->state.store(ok ? 1 : 2, std::memory_order_release);
        letGo(fetch);
    }

    const std::string& Request::text() const
    {
        static const std::string nothing;
        return done() && m_fetch ? m_fetch->text : nothing;
    }

    float Request::progress() const
    {
        if (m_fetch == nullptr || done())
        {
            return 1;
        }
        const u32 all = m_fetch->expected.load(std::memory_order_relaxed);
        return all ? static_cast<float>(received()) / static_cast<float>(all) : 0;
    }

    // --- Datagrams ---

    bool Datagrams::open(u16 port)
    {
        return m_net.start() && m_socket.open(port, m_net.onlyHere());
    }

    bool Datagrams::send(const Endpoint& to, const std::string& text)
    {
        return text.size() <= 1400 && m_socket.send(to, text.data(), text.size());
    }

    bool Datagrams::receive(Endpoint& from, std::string& text)
    {
        char data[1500];
        const size_t size = m_socket.receive(from, data, sizeof(data));
        text.assign(data, size);
        return size > 0;
    }

    // --- Connections ---

    Connection::Connection(Net& net) : m_net(net)
    {
        net.m_connections.push(this);
    }

    Connection::~Connection()
    {
        Net::remove(m_net.m_connections, this);
    }

    bool Connection::connect(const Endpoint& to)
    {
        m_out.clear();
        m_in.clear();
        m_other = to;
        return m_net.start() && m_socket.connect(to);
    }

    bool Connection::listen(u16 port, bool onlyHere)
    {
        return m_net.start() && m_socket.listen(port, onlyHere || m_net.onlyHere());
    }

    Connection* Connection::accept()
    {
        if (!listening())
        {
            return nullptr;
        }
        Connection* other = create<Connection>(MemTag::General, m_net);
        if (other && m_socket.accept(other->m_socket, other->m_other))
        {
            return other;
        }
        if (other)
        {
            other->release();
        }
        return nullptr;
    }

    void Connection::close()
    {
        m_socket.close();
        m_out.clear();
    }

    bool Connection::send(const std::string& text)
    {
        if ((!isOpen() && !connecting()) || m_out.size() + text.size() > kMostText)
        {
            return false;
        }
        m_out += text;
        return true;
    }

    std::string Connection::receive()
    {
        std::string all;
        all.swap(m_in);
        return all;
    }

    bool Connection::receiveLine(std::string& line)
    {
        const size_t end = m_in.find('\n');
        if (end == std::string::npos)
        {
            return false;
        }
        line.assign(m_in, 0, end && m_in[end - 1] == '\r' ? end - 1 : end);
        m_in.erase(0, end + 1);
        return true;
    }

    void Connection::update()
    {
        if (m_socket.update() != TcpState::Open)
        {
            return;
        }
        if (!m_out.empty())
        {
            m_out.erase(0, m_socket.send(m_out.data(), m_out.size()));
        }
        char buffer[4096];
        for (u32 i = 0; i < 16 && m_in.size() < kMostText; i++)
        {
            const size_t got = m_socket.receive(buffer, sizeof(buffer));
            if (got == 0)
            {
                break;
            }
            m_in.append(buffer, got);
        }
    }
}
