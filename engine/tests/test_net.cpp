#include "test.h"
#include "rig.h"

#include "base/thread.h"
#include "net/net.h"

#include <math.h>
#include <string.h>

using namespace nx;
using namespace nx::test;

// Everything here stays on this machine: nothing is asked of the network.
namespace
{
    struct Both
    {
        Net a, b;
        double now = 100;

        Both()
        {
            a.setOnlyHere(true);
            b.setOnlyHere(true);
        }

        void step(int count = 1)
        {
            for (int i = 0; i < count; i++)
            {
                now += 0.02;
                a.update(now);
                b.update(now);
                sleepMilliseconds(1);
            }
        }

        template <typename Fn>
        bool until(Fn fn, int most = 400)
        {
            bool ok = fn();
            for (int i = 0; i < most && !ok; i++)
            {
                step();
                ok = fn();
            }
            return ok;
        }
    };

    // A web server that says one thing.
    struct Server
    {
        Net net;
        Connection* listener;
        std::string asked;

        Server()
        {
            net.setOnlyHere(true);
            listener = create<Connection>(MemTag::General, net);
            listener->listen(0, true);
        }

        ~Server() { listener->release(); }

        std::string url(const char* path)
        {
            return "http://127.0.0.1:" + std::to_string(listener->port()) + path;
        }

        // Answers whoever asks, until a request is done.
        void answer(Request& request, const std::string& text)
        {
            Connection* other = nullptr;
            std::string got;
            for (int i = 0; i < 1500 && !request.done(); i++)
            {
                net.update(i * 0.01);
                if (other == nullptr)
                {
                    other = listener->accept();
                }
                if (other && other->isOpen())
                {
                    got += other->receive();
                    if (got.find("\r\n\r\n") != std::string::npos)
                    {
                        other->send(text);
                        net.update(i * 0.01);
                        other->close();
                    }
                }
                if (other && !other->isOpen())
                {
                    asked = got.empty() ? asked : got;
                    got.clear();
                    other->release();
                    other = nullptr;
                }
                sleepMilliseconds(2);
            }
            if (other)
            {
                other->release();
            }
        }
    };

    Request* ask(Net& net, const std::string& url)
    {
        return create<Request>(MemTag::General, net, url.c_str(), "GET", std::string(), "");
    }
}

NX_TEST(addressesAreText)
{
    u32 ip = 0;
    NX_EXPECT(netParseAddress("192.168.1.20", ip) && ip == 0xC0A80114);
    char text[16];
    netFormatAddress(ip, text);
    NX_EXPECT(strcmp(text, "192.168.1.20") == 0);
    for (const char* bad : { "", "1.2.3", "1.2.3.4.5", "1.2.3.256", "1.2.3.x", "a.b.c.d", " 1.2.3.4", "1..2.3" })
    {
        NX_EXPECT(!netParseAddress(bad, ip));
    }
}

NX_TEST(messagesComeBackAsTheyWent)
{
    Message m;
    m.writeInt(-123456);
    m.writeFloat(3.25f);
    m.writeText("hello");
    m.writeText("");
    NX_EXPECT(m.readInt() == -123456 && m.readFloat() == 3.25f && m.readText() == "hello" && m.readText().empty());
    NX_EXPECT(!m.more() && !m.short_);
    NX_EXPECT(m.readInt() == 0 && m.short_);
    Message big;
    big.writeText(std::string(2000, 'x'));
    NX_EXPECT(big.full);
    // What isn't a number never gets in.
    Message bad;
    bad.writeInt(0x7FC00000);
    NX_EXPECT(bad.readFloat() == 0);
    // A length that isn't so.
    Message lies;
    lies.data[0] = 200;
    lies.size = 4;
    NX_EXPECT(lies.readText().empty() && lies.short_);
}

NX_TEST(roomsAreFoundAndJoined)
{
    Both n;
    Room host(n.a), guest(n.b);
    NX_EXPECT(host.host("test", "Host\nname", 3));
    NX_EXPECT(host.me() == 0 && host.players() == 1);
    NX_EXPECT(guest.find("test"));
    NX_EXPECT(n.until([&]() { return guest.found().count() == 1; }));
    NX_EXPECT(strcmp(guest.found()[0].name, "Host name") == 0 && guest.found()[0].players == 1 && guest.found()[0].most == 3);
    NX_EXPECT(guest.join("test", guest.found()[0].where, "Guest"));
    NX_EXPECT(n.until([&]() { return guest.state() == RoomState::In && guest.players() == 2 && host.players() == 2; }));
    NX_EXPECT(guest.me() == 1 && strcmp(guest.playerName(0), "Host name") == 0 && strcmp(host.playerName(1), "Guest") == 0);
    NX_EXPECT(host.arrived() == 1 && host.arrived() == -1);
    // Another game isn't theirs.
    Room other(n.b);
    NX_EXPECT(other.find("something else"));
    n.step(80);
    NX_EXPECT(other.found().count() == 0);
    other.leave();
    guest.leave();
    NX_EXPECT(n.until([&]() { return host.players() == 1; }));
    NX_EXPECT(host.departed() == 1);
}

NX_TEST(sureMessagesArriveInOrder)
{
    Both n;
    Room host(n.a), one(n.b), two(n.b);
    NX_EXPECT(host.host("order", "host", 4));
    const Endpoint where = { kLoopback, host.port() };
    NX_EXPECT(one.join("order", where, "one") && two.join("order", where, "two"));
    NX_EXPECT(n.until([&]() { return one.players() == 3 && two.players() == 3 && host.players() == 3; }));
    // From one guest to everyone, by way of the host.
    for (i32 i = 0; i < 200; i++)
    {
        Message m;
        m.writeInt(i);
        NX_EXPECT(one.send(m, true, -1));
        if (i % 10 == 0)
        {
            n.step();
        }
    }
    i32 atHost = 0, atTwo = 0;
    const auto take = [&](Room& room, i32& next, int from)
    {
        while (Message* m = room.receive())
        {
            NX_EXPECT(m->from == from && m->readInt() == next);
            next++;
            m->release();
        }
    };
    NX_EXPECT(n.until([&]() { take(host, atHost, one.me()); take(two, atTwo, one.me()); return atHost == 200 && atTwo == 200; }));
    // To one of them only.
    Message secret;
    secret.writeText("for two");
    NX_EXPECT(host.send(secret, true, two.me()));
    NX_EXPECT(!host.send(secret, true, 7));
    n.step(40);
    NX_EXPECT(one.receive() == nullptr);
    Message* got = two.receive();
    NX_EXPECT(got && got->from == 0 && got->readText() == "for two");
    if (got)
    {
        got->release();
    }
    // Once, and perhaps not at all; but not here.
    Message quick;
    quick.writeInt(5);
    NX_EXPECT(two.send(quick, false, -1));
    NX_EXPECT(n.until([&]() { Message* m = one.receive(); const bool ok = m && m->from == two.me(); if (m) m->release(); return ok; }));
    NX_EXPECT(host.ping(one.me()) >= 0);
    // Everyone leaves when the host does.
    host.leave();
    NX_EXPECT(n.until([&]() { return one.state() == RoomState::Lost && two.state() == RoomState::Lost; }));
}

NX_TEST(roomsFillUp)
{
    Both n;
    Room host(n.a), one(n.b), two(n.b);
    NX_EXPECT(host.host("full", "host", 2));
    const Endpoint where = { kLoopback, host.port() };
    NX_EXPECT(one.join("full", where, "one"));
    NX_EXPECT(n.until([&]() { return one.state() == RoomState::In; }));
    NX_EXPECT(two.join("full", where, "two"));
    NX_EXPECT(n.until([&]() { return two.state() == RoomState::Lost; }));
    NX_EXPECT(host.players() == 2);
    // Nobody at home.
    Room lost(n.b);
    NX_EXPECT(lost.join("full", { kLoopback, 9 }, "x"));
    NX_EXPECT(n.until([&]() { return lost.state() == RoomState::Lost; }, 800));
}

NX_TEST(nonsenseFromTheNetworkIsIgnored)
{
    Both n;
    Room host(n.a), guest(n.b);
    NX_EXPECT(host.host("noise", "host", 4));
    const Endpoint where = { kLoopback, host.port() };
    NX_EXPECT(guest.join("noise", where, "guest"));
    NX_EXPECT(n.until([&]() { return guest.state() == RoomState::In && guest.players() == 2; }));
    UdpSocket socket;
    NX_EXPECT(socket.open(0, true));
    u32 random = 12345;
    u8 data[1400];
    for (u32 i = 0; i < 4000; i++)
    {
        const size_t size = 1 + random % (i % 3 == 0 ? 1400 : 60);
        for (size_t k = 0; k < size; k++)
        {
            random = random * 1664525u + 1013904223u;
            data[k] = static_cast<u8>(random >> 24);
        }
        if (i % 2)
        {
            // Dressed up as one of ours.
            const u32 magic = 0x3252584E;
            memcpy(data, &magic, size < 4 ? size : 4);
            u32 hash = 2166136261u;
            for (const char* c = "noise"; *c; c++)
            {
                hash = (hash ^ static_cast<u8>(*c)) * 16777619u;
            }
            if (size > 9)
            {
                memcpy(data + 4, &hash, 4);
                data[8] = static_cast<u8>(i / 2 % 8);
            }
        }
        socket.send(where, data, size);
        if (i % 64 == 0)
        {
            n.step();
        }
    }
    n.step(20);
    NX_EXPECT(host.state() == RoomState::In && guest.state() == RoomState::In);
    NX_EXPECT(guest.receive() == nullptr && host.receive() == nullptr);
}

NX_TEST(connectionsCarryText)
{
    Both n;
    Connection* listener = create<Connection>(MemTag::General, n.a);
    NX_EXPECT(listener->listen(0, true) && listener->listening());
    Connection* out = create<Connection>(MemTag::General, n.b);
    NX_EXPECT(out->connect({ kLoopback, listener->port() }));
    Connection* in = nullptr;
    NX_EXPECT(n.until([&]() { in = in ? in : listener->accept(); return in && out->isOpen(); }));
    NX_EXPECT(out->send("hello\r\nthere\nand more"));
    std::string line;
    NX_EXPECT(n.until([&]() { return in->receiveLine(line); }) && line == "hello");
    NX_EXPECT(in->receiveLine(line) && line == "there" && !in->receiveLine(line));
    NX_EXPECT(in->receive() == "and more");
    const std::string lots(200000, 'q');
    NX_EXPECT(in->send(lots));
    std::string got;
    NX_EXPECT(n.until([&]() { got += out->receive(); return got.size() == lots.size(); }, 2000) && got == lots);
    in->close();
    NX_EXPECT(n.until([&]() { return !out->isOpen(); }));
    in->release();
    out->release();
    listener->release();
    Datagrams* a = create<Datagrams>(MemTag::General, n.a);
    Datagrams* b = create<Datagrams>(MemTag::General, n.b);
    NX_EXPECT(a->open(0) && b->open(0));
    NX_EXPECT(a->send({ kLoopback, b->port() }, "ping"));
    Endpoint from;
    std::string text;
    NX_EXPECT(n.until([&]() { return b->receive(from, text); }) && text == "ping" && from.port == a->port());
    NX_EXPECT(!a->send({ kLoopback, b->port() }, std::string(5000, 'x')));
    a->release();
    b->release();
}

NX_TEST(webAnswersAreRead)
{
    Server server;
    Net net;
    net.setOnlyHere(true);
    Request* plain = ask(net, server.url("/a/b?c=d#e"));
    server.answer(*plain, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 5\r\n\r\nhello");
    NX_EXPECT(plain->done() && !plain->failed() && plain->status() == 200 && plain->text() == "hello");
    NX_EXPECT(server.asked.compare(0, 20, "GET /a/b?c=d HTTP/1.") == 0);
    plain->release();

    Request* chunked = ask(net, server.url("/"));
    server.answer(*chunked, "HTTP/1.1 200 OK\r\ntransfer-encoding: chunked\r\n\r\n5\r\nhello\r\n7;x=y\r\n, world\r\n0\r\n\r\n");
    NX_EXPECT(!chunked->failed() && chunked->text() == "hello, world");
    chunked->release();

    Request* missing = ask(net, server.url("/none"));
    server.answer(*missing, "HTTP/1.1 404 Not Found\r\nContent-Length: 4\r\n\r\nnone");
    NX_EXPECT(!missing->failed() && missing->status() == 404);
    missing->release();

    Request* cut = ask(net, server.url("/"));
    server.answer(*cut, "HTTP/1.1 200 OK\r\nContent-Length: 500\r\n\r\nshort");
    NX_EXPECT(cut->failed() && cut->text().empty() && cut->error()[0]);
    cut->release();

    Request* odd = ask(net, server.url("/"));
    server.answer(*odd, "220 this is not the web\r\n\r\n");
    NX_EXPECT(odd->failed());
    odd->release();

    Request* round = ask(net, server.url("/"));
    server.answer(*round, "HTTP/1.1 302 Found\r\nLocation: /again\r\nContent-Length: 0\r\n\r\n");
    NX_EXPECT(round->failed());
    round->release();

    Request* post = create<Request>(MemTag::General, net, server.url("/post").c_str(), "POST", std::string("a=1"), "text/x");
    server.answer(*post, "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok");
    NX_EXPECT(post->text() == "ok" && server.asked.find("Content-Length: 3\r\n") != std::string::npos);
    post->release();

    for (const char* bad : { "ftp://x/", "http://", "http://a b/", "http://x/\r\nEvil: 1", "http://user@host/", "http://x:99999/", "x" })
    {
        Request* r = ask(net, bad);
        for (int i = 0; i < 500 && !r->done(); i++)
        {
            sleepMilliseconds(2);
        }
        NX_EXPECT(r->failed());
        r->release();
    }
    // Let go of before it is over.
    Request* gone = ask(net, server.url("/slow"));
    gone->release();
    net.shutdown();
}

NX_TEST(whatIsntSecureIsRefused)
{
    // A server that doesn't speak it at all.
    Rig rig;
    Server server;
    const std::string url = "https://127.0.0.1:" + std::to_string(server.listener->port()) + "/";
    Request* r = ask(rig.net, url);
    server.answer(*r, "HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nhello");
    for (int i = 0; i < 3000 && !r->done(); i++)
    {
        sleepMilliseconds(2);
    }
    NX_EXPECT(r->done() && r->failed() && r->text().empty());
    r->release();
}

NX_TEST(scriptsUseTheNetwork)
{
    Rig r;
    NX_EXPECT(r.ok);
    r.platform.extraPath = "showcase/shapes/main.as";
    r.platform.extraText = R"(
        class A : Screen
        {
            Room@ host = Room();
            Room@ guest = Room();
            int got = 0;
            void onEnter()
            {
                if (!Net::available) print("no network");
                host.host("script", "Host", 2);
                guest.find("script");
            }
            void update(float dt)
            {
                if (guest.state == RoomFinding && guest.found > 0) guest.join("script", 0, "Guest");
                if (guest.state == RoomIn && guest.players == 2 && got == 0)
                {
                    Message@ m = Message();
                    m.writeInt(42);
                    m.writeVec3(Vec3(1, 2, 3));
                    m.writeText("hi");
                    guest.send(m);
                    got = 1;
                }
                Message@ m = host.receive();
                if (m !is null && m.from == 1 && m.readInt() == 42 && m.readVec3() == Vec3(1, 2, 3) && m.readText() == "hi" && m.whole && !m.more)
                {
                    print("network works");
                    Screens::home();
                }
            }
            void draw(Canvas@ c) {}
        }
    )";
    NX_EXPECT(r.screens.init(r.services, "showcase", "main"));
    r.step(2);
    r.screens.push("shapes");
    for (int i = 0; i < 400 && (i < 5 || r.screens.depth() > 1); i++)
    {
        r.step();
        sleepMilliseconds(1);
    }
    NX_EXPECT(!r.screens.failed() && r.screens.depth() == 1);
}
