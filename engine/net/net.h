#pragma once

#include "base/grow_array.h"
#include "base/math.h"
#include "base/thread.h"
#include "net/ftp.h"
#include "net/socket.h"
#include "platform/platform.h"

#include <atomic>
#include <string>

namespace nx
{
    class Net;

    // --- Messages: numbers and text, packed small ---

    struct Message
    {
        static constexpr u32 kMost = 1000;

        u8 data[kMost];
        u32 size = 0;
        u32 at = 0;       // reading
        int from = -1;    // the player who sent it
        bool full = false, short_ = false;
        int refs = 1;

        void addRef() { refs++; }
        void release()
        {
            if (--refs == 0)
            {
                destroy(this);
            }
        }
        void write(const void* bytes, u32 count);
        bool read(void* bytes, u32 count);
        void writeInt(i32 v);
        void writeFloat(float v);
        void writeText(const std::string& v);
        i32 readInt();
        float readFloat();
        std::string readText();
        bool more() const { return at < size; }
    };

    // --- Rooms: a few players, one of whom is the host, on the same network ---

    constexpr u32 kRoomPlayers = 8;
    constexpr u16 kRoomPort = 27310;   // and the few after it

    enum class RoomState : u8 { Idle, Finding, Joining, In, Lost };

    class Room : NonCopyable
    {
    public:
        explicit Room(Net& net);
        ~Room();

        void addRef() { m_refs++; }
        void release()
        {
            if (--m_refs == 0)
            {
                destroy(this);
            }
        }

        bool host(const char* game, const char* name, u32 most);
        // Asks everyone on the network, once a second, until something else is done.
        bool find(const char* game);
        bool join(const char* game, const Endpoint& where, const char* name);
        void leave();

        RoomState state() const { return m_state; }
        bool hosting() const { return m_hosting; }
        int me() const { return m_state == RoomState::In ? m_me : -1; }
        u32 most() const { return m_most; }
        u32 players() const;
        bool present(int player) const { return player >= 0 && player < static_cast<int>(kRoomPlayers) && m_state == RoomState::In && m_players[player].present; }
        const char* playerName(int player) const { return present(player) ? m_players[player].name : ""; }
        // Milliseconds there and back, as far as is known.
        int ping(int player) const;

        struct Found
        {
            Endpoint where;
            char name[24];
            u32 players, most;
            double heard;
        };
        const GrowArray<Found, MemTag::General>& found() const { return m_found; }

        // To one player, or to everyone else with -1. A sure one arrives, and in order; the other is sent
        // once. False if it can't be.
        bool send(const Message& message, bool sure, int to);
        // The next that has come, or nullptr. The caller releases it.
        Message* receive();
        // A player who has come or gone since it was last asked, or -1.
        int arrived() { return take(m_arrived); }
        int departed() { return take(m_departed); }

        u16 port() const { return m_socket.port(); }

        void update(double now);

    private:
        friend class Net;

        struct Player
        {
            bool present;
            char name[24];
        };

        struct Out
        {
            Message* message;
            u16 number;
            u8 origin, to;
            bool internal;
            double sent;
        };

        // To a player, from the host; to the host, from a player.
        struct Link
        {
            bool used;
            Endpoint where;
            u32 token;
            double heard, sent;
            float ping;
            u32 theirTime;        // of the last packet, to send back
            double theirTimeAt;
            u16 nextOut, nextIn;  // numbers of sure messages
            GrowArray<Out, MemTag::General>* sure;
            Message* early[32];   // that came before their turn
            u8 earlyOrigin[32], earlyTo[32];
            bool earlyInternal[32];
        };

        struct Reader;
        struct Writer;

        static int take(u8& bits)
        {
            for (int i = 0; i < 8; i++)
            {
                if (bits & (1 << i))
                {
                    bits = static_cast<u8>(bits & ~(1 << i));
                    return i;
                }
            }
            return -1;
        }

        bool open(u16 port);
        void reset();
        void closeLink(Link& link);
        void handle(const Endpoint& from, const u8* data, size_t size);
        void handleData(Link& link, u32 index, Reader& in);
        void accept(Link& link, u32 index, Message* message, u8 origin, u8 to, bool internal);
        void deliver(Message* message, int from);
        void enqueue(Link& link, const Message& message, u8 origin, u8 to, bool internal);
        void flush(Link& link, const Message* quick, u8 origin, u8 to);
        void sendPlayers();
        void setPlayers(Message& message);
        void drop(u32 index);
        u32 random();

        Net& m_net;
        int m_refs = 1;
        UdpSocket m_socket;
        RoomState m_state = RoomState::Idle;
        bool m_hosting = false;
        u32 m_game = 0, m_most = 4;
        int m_me = -1;
        char m_name[24] = {};
        Endpoint m_server;
        u32 m_nonce = 0;
        double m_now = 0, m_asked = -10, m_began = 0;
        u32 m_random = 0;
        Player m_players[kRoomPlayers] = {};
        Link m_links[kRoomPlayers] = {};   // by player for the host; the first only, for the others
        GrowArray<Message*, MemTag::General> m_inbox;
        GrowArray<Found, MemTag::General> m_found;
        u8 m_arrived = 0, m_departed = 0;
    };

    // --- The web ---

    // One question to a web server and its answer, on a worker thread.
    struct Fetch
    {
        std::string url, method = "GET", sent, type;
        std::string text;
        size_t most = 4 * 1024 * 1024;
        int status = 0;
        const char* error = "";
        std::atomic<u32> received{ 0 }, expected{ 0 };  // bytes, as they come; expected is 0 if not known
        std::atomic<int> state{ 0 };                     // 0 going, 1 done, 2 failed
        std::atomic<bool> cancelled{ false };
        std::atomic<int> users{ 1 };
    };

    // Waits. http:// or https://, following where it is sent on to.
    bool httpFetch(Fetch& fetch);
    // The same, for whoever only wants what is in a file.
    bool webRead(const char* url, FileData& out);
    // Calls off all of those, before closing.
    void webStop();

    class Request : NonCopyable
    {
    public:
        Request(Net& net, const char* url, const char* method, const std::string& body, const char* type);
        ~Request();

        void addRef() { m_refs++; }
        void release()
        {
            if (--m_refs == 0)
            {
                destroy(this);
            }
        }

        bool done() const { return m_fetch == nullptr || m_fetch->state.load(std::memory_order_acquire) != 0; }
        bool failed() const { return m_fetch == nullptr || m_fetch->state.load(std::memory_order_acquire) == 2; }
        int status() const { return done() && m_fetch ? m_fetch->status : 0; }
        // Empty until it is done.
        const std::string& text() const;
        const char* error() const { return done() && m_fetch ? m_fetch->error : m_fetch ? "" : "there is no network"; }
        u32 received() const { return m_fetch ? m_fetch->received.load(std::memory_order_relaxed) : 0; }
        // 0 to 1, or 0 while nobody knows.
        float progress() const;

    private:
        static void run(void* fetch);
        static void letGo(Fetch* fetch);

        int m_refs = 1;
        Fetch* m_fetch = nullptr;
    };

    // --- Datagrams: anything to anyone, which may not arrive ---

    class Datagrams : NonCopyable
    {
    public:
        explicit Datagrams(Net& net) : m_net(net) {}

        void addRef() { m_refs++; }
        void release()
        {
            if (--m_refs == 0)
            {
                destroy(this);
            }
        }

        bool open(u16 port);
        void close() { m_socket.close(); }
        bool isOpen() const { return m_socket.isOpen(); }
        u16 port() const { return m_socket.port(); }
        bool send(const Endpoint& to, const std::string& text);
        bool receive(Endpoint& from, std::string& text);

    private:
        Net& m_net;
        int m_refs = 1;
        UdpSocket m_socket;
    };

    // --- Connections: text either way, for servers of your own ---

    class Connection : NonCopyable
    {
    public:
        explicit Connection(Net& net);
        ~Connection();

        void addRef() { m_refs++; }
        void release()
        {
            if (--m_refs == 0)
            {
                destroy(this);
            }
        }

        bool connect(const Endpoint& to);
        bool listen(u16 port, bool onlyHere);
        // The next to have connected, or nullptr. The caller releases it.
        Connection* accept();
        void close();
        bool isOpen() const { return m_socket.state() == TcpState::Open; }
        bool listening() const { return m_socket.state() == TcpState::Listening; }
        bool connecting() const { return m_socket.state() == TcpState::Connecting; }
        u16 port() const { return m_socket.port(); }
        Endpoint other() const { return m_other; }
        bool send(const std::string& text);
        // All that has come.
        std::string receive();
        // Up to the end of a line, without it. False if a whole one hasn't come.
        bool receiveLine(std::string& line);

        void update();

    private:
        friend class Net;

        Net& m_net;
        int m_refs = 1;
        TcpSocket m_socket;
        Endpoint m_other;
        std::string m_out, m_in;
    };

    // Keeps all of them going, a little each frame. Nothing is opened until a script asks.
    class Net : NonCopyable
    {
    public:
        ~Net() { shutdown(); }

        // Starts the network when it is first needed. False where there is none.
        bool start();
        void shutdown();
        void update(double now);

        double now() const { return m_now; }
        FtpServer& ftp() { return m_ftp; }
        // Only this machine can be reached, and can reach it. For tests.
        void setOnlyHere(bool onlyHere) { m_onlyHere = onlyHere; }
        bool onlyHere() const { return m_onlyHere; }

    private:
        friend class Room;
        friend class Request;
        friend class Connection;

        template <typename T>
        static void remove(GrowArray<T*, MemTag::General>& list, T* item)
        {
            for (u32 i = 0; i < list.count(); i++)
            {
                if (list[i] == item)
                {
                    list.swapRemove(i);
                    return;
                }
            }
        }

        static constexpr u32 kWorkers = 3;

        bool m_started = false, m_failed = false, m_onlyHere = false;
        bool m_working[kWorkers] = {};
        u32 m_nextWorker = 0;
        double m_now = 0;
        JobQueue m_jobs[kWorkers];
        FtpServer m_ftp;
        GrowArray<Room*, MemTag::General> m_rooms;
        GrowArray<Connection*, MemTag::General> m_connections;
    };
}
