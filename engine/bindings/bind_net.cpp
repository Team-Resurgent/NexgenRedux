#include "bindings/script_refs.h"
#include "net/net.h"
#include "net/websocket.h"
#include "scene/sharing.h"
#include "script/script_host.h"

#include <stdlib.h>
#include <string.h>

namespace nx
{
    namespace
    {
        Net& net() { return *services().net; }

        Message* makeMessage() { return create<Message>(MemTag::Script); }
        Room* makeRoom() { return create<Room>(MemTag::Script, net()); }
        Connection* makeConnection() { return create<Connection>(MemTag::Script, net()); }
        WebSocket* makeWebSocket(const std::string& url) { return create<WebSocket>(MemTag::Script, url.c_str()); }
        bool webSocketOpen(const WebSocket* w) { return w->state() == WebSocket::State::Open; }
        bool webSocketConnecting(const WebSocket* w) { return w->state() == WebSocket::State::Connecting; }
        bool webSocketClosed(const WebSocket* w) { return w->state() == WebSocket::State::Closed; }
        std::string webSocketError(const WebSocket* w) { return w->error(); }
        bool webSocketSend(const std::string& text, WebSocket* w) { return w->send(text); }
        bool webSocketReceive(std::string& out, WebSocket* w) { return w->receive(out); }
        Datagrams* makeDatagrams() { return create<Datagrams>(MemTag::Script, net()); }

        // "192.168.1.2" or "192.168.1.2:27310". Names are looked up only by Net::get, which needn't wait.
        bool parse(const std::string& text, Endpoint& out, u16 port)
        {
            const size_t colon = text.find(':');
            out.port = colon == std::string::npos ? port : static_cast<u16>(atoi(text.c_str() + colon + 1));
            return netParseAddress(text.substr(0, colon).c_str(), out.ip) && out.port;
        }

        std::string format(const Endpoint& e)
        {
            char address[16];
            netFormatAddress(e.ip, address);
            return std::string(address) + ":" + std::to_string(e.port);
        }

        // --- Messages ---

        void messageVec2(const Vec2& v, Message* m)
        {
            m->writeFloat(v.x);
            m->writeFloat(v.y);
        }
        void messageVec3(const Vec3& v, Message* m)
        {
            m->writeFloat(v.x);
            m->writeFloat(v.y);
            m->writeFloat(v.z);
        }
        Vec2 readVec2(Message* m)
        {
            const float x = m->readFloat();
            return { x, m->readFloat() };
        }
        Vec3 readVec3(Message* m)
        {
            const float x = m->readFloat(), y = m->readFloat();
            return { x, y, m->readFloat() };
        }
        bool messageOk(const Message* m) { return !m->full && !m->short_; }

        // --- Rooms ---

        bool roomHost(const std::string& game, const std::string& name, u32 most, Room* r) { return r->host(game.c_str(), name.c_str(), most); }
        bool roomFind(const std::string& game, Room* r) { return r->find(game.c_str()); }
        bool roomJoinFound(const std::string& game, u32 index, const std::string& name, Room* r)
        {
            if (index >= r->found().count())
            {
                return false;
            }
            const Room::Found found = r->found()[index];
            return r->joinFound(game.c_str(), found, name.c_str());
        }
        bool roomJoinAddress(const std::string& game, const std::string& address, const std::string& name, Room* r) { return r->join(game.c_str(), address.c_str(), name.c_str()); }
        bool roomMeet(const std::string& place, Room* r) { return r->meet(place.c_str()); }
        bool roomFoundFar(u32 i, const Room* r) { return i < r->found().count() && r->found()[i].far; }
        u32 roomFoundCount(const Room* r) { return r->found().count(); }
        std::string roomFoundName(u32 i, const Room* r) { return i < r->found().count() ? r->found()[i].name : ""; }
        std::string roomFoundAddress(u32 i, const Room* r) { return i < r->found().count() ? format(r->found()[i].where) : ""; }
        u32 roomFoundPlayers(u32 i, const Room* r) { return i < r->found().count() ? r->found()[i].players : 0; }
        u32 roomFoundMost(u32 i, const Room* r) { return i < r->found().count() ? r->found()[i].most : 0; }
        std::string roomPlayerName(int player, const Room* r) { return r->playerName(player); }
        bool roomSend(const Message* m, bool sure, int to, Room* r)
        {
            if (m == nullptr)
            {
                scriptError("null message");
                return false;
            }
            return r->send(*m, sure, to);
        }

        // --- Entities that are shared ---

        bool roomShare(EntityRef* e, const std::string& kind, Room* r)
        {
            Sharing* s = e ? Sharing::of(*r) : nullptr;
            return s && s->share(*r, e, kind.c_str());
        }
        void roomUnshare(EntityRef* e, Room* r)
        {
            if (Sharing* s = Sharing::find(*r))
            {
                s->unshare(*r, e);
            }
        }
        int roomOwner(const EntityRef* e, const Room* r)
        {
            const Sharing* s = Sharing::find(*r);
            return s ? s->ownerOf(*r, e) : -1;
        }
        u32 roomCopies(const Room* r)
        {
            const Sharing* s = Sharing::find(*r);
            return s ? s->copies() : 0;
        }

        struct Making
        {
            Services* services;
            asIScriptFunction* maker;
        };

        EntityRef* makeCopy(void* user, const char* kind, int player)
        {
            Making& m = *static_cast<Making*>(user);
            std::string text = kind;
            void* made = nullptr;
            if (!m.services->host->callEvent(m.maker, &text, nullptr, &player, &made))
            {
                return nullptr;
            }
            return static_cast<EntityRef*>(made);
        }

        void roomSync(asIScriptFunction* maker, Room* r)
        {
            Sharing* s = Sharing::of(*r);
            if (s)
            {
                Making making = { &services(), maker };
                s->sync(*r, services().time, maker ? makeCopy : nullptr, &making);
            }
            if (maker)
            {
                maker->Release();
            }
        }

        // --- The web ---

        Request* webGet(const std::string& url) { return create<Request>(MemTag::Script, net(), url.c_str(), "GET", std::string(), ""); }
        Request* webPost(const std::string& url, const std::string& body, const std::string& type)
        {
            for (const char c : type)
            {
                if (static_cast<u8>(c) < 32)
                {
                    scriptError("that isn't a content type");
                    return nullptr;
                }
            }
            return create<Request>(MemTag::Script, net(), url.c_str(), "POST", body, type.c_str());
        }
        std::string requestText(const Request* r) { return r->text(); }
        std::string requestError(const Request* r) { return r->error(); }

        // --- Connections ---

        bool connectionOpen(const std::string& address, Connection* c)
        {
            Endpoint to;
            return parse(address, to, 0) && c->connect(to);
        }
        bool connectionListen(u32 port, Connection* c) { return port < 65536 && c->listen(static_cast<u16>(port), false); }
        std::string connectionOther(const Connection* c) { return format(c->other()); }
        u32 connectionPort(const Connection* c) { return c->port(); }

        bool datagramsOpen(u32 port, Datagrams* d) { return port < 65536 && d->open(static_cast<u16>(port)); }
        u32 datagramsPort(const Datagrams* d) { return d->port(); }
        bool datagramsSend(const std::string& address, const std::string& text, Datagrams* d)
        {
            Endpoint to;
            return parse(address, to, 0) && d->send(to, text);
        }
        bool datagramsBroadcast(u32 port, const std::string& text, Datagrams* d)
        {
            return port && port < 65536 && d->send({ net().onlyHere() ? kLoopback : kBroadcast, static_cast<u16>(port) }, text);
        }
        bool datagramsReceive(std::string& address, std::string& text, Datagrams* d)
        {
            Endpoint from;
            const bool any = d->receive(from, text);
            address = any ? format(from) : "";
            return any;
        }

        bool netAvailable() { return net().start(); }
        std::string netMeetingPlace() { return net().meetingPlace(); }
        void netSetMeetingPlace(const std::string& place) { net().setMeetingPlace(place); }
        std::string netAddress()
        {
            char text[16] = "";
            const u32 ip = net().start() ? netLocalAddress() : 0;
            if (ip)
            {
                netFormatAddress(ip, text);
            }
            return text;
        }
    }

    void bindNet(Binder& b)
    {
        b.refType("Message", 0, "Numbers and text to send, up to 1000 bytes. Read it in the order it was written.");
        b.behaviour(asBEHAVE_FACTORY, "Message@ f()", NX_FN(makeMessage));
        b.behaviour(asBEHAVE_ADDREF, "void f()", NX_METHOD(Message, addRef));
        b.behaviour(asBEHAVE_RELEASE, "void f()", NX_METHOD(Message, release));
        b.method("void writeInt(int value)", NX_METHOD(Message, writeInt), "Adds a whole number.");
        b.method("void writeFloat(float value)", NX_METHOD(Message, writeFloat), "Adds a number.");
        b.method("void writeText(const string &in value)", NX_METHOD(Message, writeText), "Adds some text.");
        b.method("void writeVec2(const Vec2 &in value)", NX_OBJ_LAST(messageVec2), "Adds a Vec2.");
        b.method("void writeVec3(const Vec3 &in value)", NX_OBJ_LAST(messageVec3), "Adds a Vec3.");
        b.method("int readInt()", NX_METHOD(Message, readInt), "The next whole number.");
        b.method("float readFloat()", NX_METHOD(Message, readFloat), "The next number. Always a real one.");
        b.method("string readText()", NX_METHOD(Message, readText), "The next text.");
        b.method("Vec2 readVec2()", NX_OBJ_LAST(readVec2), "The next Vec2.");
        b.method("Vec3 readVec3()", NX_OBJ_LAST(readVec3), "The next Vec3.");
        b.method("bool get_more() const property", NX_METHOD(Message, more), "Whether there is anything left to read.");
        b.method("bool get_whole() const property", NX_OBJ_LAST(messageOk), "False if more was written than fits, or read than there was.");
        b.property("const int from", offsetof(Message, from), "The player who sent it.");

        b.enumType("RoomState", "How a room is getting on.");
        b.enumValue("RoomIdle", static_cast<int>(RoomState::Idle), "Doing nothing.");
        b.enumValue("RoomFinding", static_cast<int>(RoomState::Finding), "Looking for others.");
        b.enumValue("RoomJoining", static_cast<int>(RoomState::Joining), "Asking to come in.");
        b.enumValue("RoomIn", static_cast<int>(RoomState::In), "In it, or hosting it.");
        b.enumValue("RoomLost", static_cast<int>(RoomState::Lost), "It couldn't be joined, or the host has gone.");

        b.funcdef("Entity@ Copy(const string &in kind, int player)", "Makes this game's copy of something another player shares, of the kind they named: an entity that looks like theirs, or null not to have one. It may be a method: `Copy(this.make)`.");
        b.refType("Room", 0, "A game for up to 8 players on the same network, or on the internet by way of a meeting place, one of whom is the host. Keep it in a field, and it lasts until the screen closes.");
        b.behaviour(asBEHAVE_FACTORY, "Room@ f()", NX_FN(makeRoom));
        b.behaviour(asBEHAVE_ADDREF, "void f()", NX_METHOD(Room, addRef));
        b.behaviour(asBEHAVE_RELEASE, "void f()", NX_METHOD(Room, release));
        b.method("bool host(const string &in game, const string &in name, uint most = 4)", NX_OBJ_LAST(roomHost), "Opens a room. Those who look for the same game find it under a name.");
        b.method("bool find(const string &in game)", NX_OBJ_LAST(roomFind), "Looks for rooms, until something else is done.");
        b.method("uint get_found() const property", NX_OBJ_LAST(roomFoundCount), "How many there are.");
        b.method("string foundName(uint index) const", NX_OBJ_LAST(roomFoundName), "The name of one.");
        b.method("string foundAddress(uint index) const", NX_OBJ_LAST(roomFoundAddress), "Where it is.");
        b.method("uint foundPlayers(uint index) const", NX_OBJ_LAST(roomFoundPlayers), "How many are in it.");
        b.method("uint foundMost(uint index) const", NX_OBJ_LAST(roomFoundMost), "How many could be.");
        b.method("bool join(const string &in game, uint found, const string &in name)", NX_OBJ_LAST(roomJoinFound), "Joins one that was found.");
        b.method("bool join(const string &in game, const string &in address, const string &in name)", NX_OBJ_LAST(roomJoinAddress), "Joins one whose address is known: `a.b.c.d`, `a.b.c.d:port`, or a name to look up, such as `games.example.com:27310`. A room on the internet needs its port let through the host's router; a meeting place doesn't.");
        b.method("bool meet(const string &in place)", NX_OBJ_LAST(roomMeet), "A meeting place on the internet that every player can reach, `name` or `name:port` (27300 if none is said), run with `--meeting`. Then a room that is hosted is listed there, `find` asks it as well as the network, and joining one found there gets through the routers on the way, or is passed on by the meeting place if that can't be done. Before host, find or join; empty for none.");
        b.method("bool get_met() const property", NX_METHOD(Room, met), "Whether the meeting place has been looked up and can be used.");
        b.method("bool foundFar(uint index) const", NX_OBJ_LAST(roomFoundFar), "Whether one was found at the meeting place, rather than on this network.");
        b.method("bool relayed(int player) const", NX_METHOD(Room, relayed), "Whether what goes to and from a player is passed on by the meeting place, because the routers wouldn't let it through.");
        b.method("void leave()", NX_METHOD(Room, leave), "Leaves, or closes it.");
        b.method("RoomState get_state() const property", NX_METHOD(Room, state), "How it is getting on.");
        b.method("bool get_hosting() const property", NX_METHOD(Room, hosting), "Whether it is this player's.");
        b.method("int get_me() const property", NX_METHOD(Room, me), "Which player this is, or -1. The host is 0.");
        b.method("uint get_players() const property", NX_METHOD(Room, players), "How many are in it.");
        b.method("uint get_most() const property", NX_METHOD(Room, most), "How many could be.");
        b.method("bool present(int player) const", NX_METHOD(Room, present), "Whether a player, from 0 to 7, is in it.");
        b.method("string playerName(int player) const", NX_OBJ_LAST(roomPlayerName), "What they are called.");
        b.method("int ping(int player) const", NX_METHOD(Room, ping), "Milliseconds there and back. For all but the host, to the host.");
        b.method("bool send(Message@+ message, bool sure = true, int to = -1)", NX_OBJ_LAST(roomSend), "To one player, or to everyone else. A sure one arrives, and in order; one that isn't is sent once, and is for what is soon out of date.");
        b.method("Message@ receive()", NX_METHOD(Room, receive), "The next that has come, or null.");
        b.method("int arrived()", NX_METHOD(Room, arrived), "A player who has come since it was last asked, or -1.");
        b.method("int departed()", NX_METHOD(Room, departed), "A player who has gone, or -1.");
        b.method("bool share(Entity@+ entity, const string &in kind)", NX_OBJ_LAST(roomShare), "Keeps an entity of this player's alike in every other copy of the game in the room: where it is, how it is turned and looks, and the animation it plays. Each has a copy of its own, made by `sync`, which follows it. The kind says what to make. False if it is shared already, or is a copy.");
        b.method("void unshare(Entity@+ entity)", NX_OBJ_LAST(roomUnshare), "No longer: the copies go, and it stays here as it is. Destroying it does the same.");
        b.method("void sync(Copy@ maker)", NX_OBJ_LAST(roomSync), "Once a frame, while in the room: says where this player's shared ones are, 20 times a second, and moves the copies of everyone else's smoothly after theirs. `maker` is asked for a copy of each that another player shares; those of a player who leaves go.");
        b.method("int ownerOf(Entity@+ entity) const", NX_OBJ_LAST(roomOwner), "Whose a shared entity is: this player's own number for one shared here, another's for a copy of theirs, -1 for neither.");
        b.method("uint get_copies() const property", NX_OBJ_LAST(roomCopies), "How many copies of others' there are here.");

        b.refType("Request", 0, "Something asked of a web server, over http:// or https://, whose certificate is checked.");
        b.behaviour(asBEHAVE_ADDREF, "void f()", NX_METHOD(Request, addRef));
        b.behaviour(asBEHAVE_RELEASE, "void f()", NX_METHOD(Request, release));
        b.method("bool get_done() const property", NX_METHOD(Request, done), "Whether it is over, one way or the other.");
        b.method("bool get_failed() const property", NX_METHOD(Request, failed), "Whether there was no answer. `status` may still say that the server refused.");
        b.method("int get_status() const property", NX_METHOD(Request, status), "200 is fine, 404 is not there, and so on; 0 until it is done.");
        b.method("string get_text() const property", NX_OBJ_LAST(requestText), "The answer, up to 4 MB; empty until it is done.");
        b.method("string get_error() const property", NX_OBJ_LAST(requestError), "Why it failed.");
        b.method("uint get_received() const property", NX_METHOD(Request, received), "Bytes that have come.");
        b.method("float get_progress() const property", NX_METHOD(Request, progress), "0 to 1, or 0 while nobody knows.");

        b.refType("WebSocket", 0, "Messages both ways with a web server, kept open: ws://, or wss:// whose certificate is checked. It connects by itself; what comes waits until it is read.");
        b.behaviour(asBEHAVE_FACTORY, "WebSocket@ f(const string &in url)", NX_FN(makeWebSocket));
        b.behaviour(asBEHAVE_ADDREF, "void f()", NX_METHOD(WebSocket, addRef));
        b.behaviour(asBEHAVE_RELEASE, "void f()", NX_METHOD(WebSocket, release));
        b.method("bool get_connecting() const property", NX_OBJ_LAST(webSocketConnecting), "Whether it is still connecting.");
        b.method("bool get_connected() const property", NX_OBJ_LAST(webSocketOpen), "Whether messages can go both ways.");
        b.method("bool get_closed() const property", NX_OBJ_LAST(webSocketClosed), "Whether it is over; error says why, unless it was closed here.");
        b.method("string get_error() const property", NX_OBJ_LAST(webSocketError), "Why it closed or never opened.");
        b.method("bool send(const string &in text)", NX_OBJ_LAST(webSocketSend), "Sends a message, once it is connected; in order. False once it has closed.");
        b.method("bool receive(string &out message)", NX_OBJ_LAST(webSocketReceive), "The next message that has come, oldest first. False if there is none.");
        b.method("void close()", NX_METHOD(WebSocket, close), "Says goodbye, and closes it.");

        b.refType("Connection", 0, "A stream of text to or from another program, over TCP. What is sent arrives, and in order.");
        b.behaviour(asBEHAVE_FACTORY, "Connection@ f()", NX_FN(makeConnection));
        b.behaviour(asBEHAVE_ADDREF, "void f()", NX_METHOD(Connection, addRef));
        b.behaviour(asBEHAVE_RELEASE, "void f()", NX_METHOD(Connection, release));
        b.method("bool connect(const string &in address)", NX_OBJ_LAST(connectionOpen), "Begins to connect to \"192.168.1.2:80\".");
        b.method("bool listen(uint port)", NX_OBJ_LAST(connectionListen), "Waits for others to connect; 0 is any port that is free.");
        b.method("Connection@ accept()", NX_METHOD(Connection, accept), "The next to have connected, or null.");
        b.method("void close()", NX_METHOD(Connection, close), "Ends it.");
        b.method("bool get_connected() const property", NX_METHOD(Connection, isOpen), "Whether it is connected.");
        b.method("bool get_connecting() const property", NX_METHOD(Connection, connecting), "Whether it is still trying to.");
        b.method("bool get_listening() const property", NX_METHOD(Connection, listening), "Whether it is waiting for others.");
        b.method("uint get_port() const property", NX_OBJ_LAST(connectionPort), "Where it is listening.");
        b.method("string get_address() const property", NX_OBJ_LAST(connectionOther), "Who is at the other end.");
        b.method("bool send(const string &in text)", NX_METHOD(Connection, send), "Sends some text. False if it isn't open, or too much is waiting.");
        b.method("string receive()", NX_METHOD(Connection, receive), "All that has come.");
        b.method("bool receiveLine(string &out line)", NX_METHOD(Connection, receiveLine), "The next whole line, without its end.");

        b.refType("Datagrams", 0, "Short texts to anyone, over UDP. They may not arrive, or not in order.");
        b.behaviour(asBEHAVE_FACTORY, "Datagrams@ f()", NX_FN(makeDatagrams));
        b.behaviour(asBEHAVE_ADDREF, "void f()", NX_METHOD(Datagrams, addRef));
        b.behaviour(asBEHAVE_RELEASE, "void f()", NX_METHOD(Datagrams, release));
        b.method("bool open(uint port = 0)", NX_OBJ_LAST(datagramsOpen), "Opens a port; 0 is any that is free.");
        b.method("void close()", NX_METHOD(Datagrams, close), "Closes it.");
        b.method("bool get_opened() const property", NX_METHOD(Datagrams, isOpen), "Whether it is open.");
        b.method("uint get_port() const property", NX_OBJ_LAST(datagramsPort), "The port it has.");
        b.method("bool send(const string &in address, const string &in text)", NX_OBJ_LAST(datagramsSend), "Sends up to 1400 bytes to \"192.168.1.2:9000\".");
        b.method("bool broadcast(uint port, const string &in text)", NX_OBJ_LAST(datagramsBroadcast), "Sends to everyone on the network who is listening there.");
        b.method("bool receive(string &out address, string &out text)", NX_OBJ_LAST(datagramsReceive), "The next to have come, and from whom.");

        b.section("Net", "The network: rooms for games on the same one or met on the internet, the web, and connections of your own.");
        b.names("Net");
        b.function("bool get_available() property", NX_FN(netAvailable), "Whether there is one.");
        b.function("string get_address() property", NX_FN(netAddress), "This machine's own, or empty while it has none.");
        b.function("string get_meetingPlace() property", NX_FN(netMeetingPlace), "The player's choice of meeting place on the internet, kept between runs: every room meets there, unless its own `meet` says otherwise. Empty for none.");
        b.function("void set_meetingPlace(const string &in) property", NX_FN(netSetMeetingPlace), "The player's choice of meeting place on the internet, `name` or `name:port`, kept between runs. Rooms made after it is set meet there.");
        b.function("Request@ get(const string &in url)", NX_FN(webGet), "Asks for a web page, or anything else.");
        b.function("Request@ post(const string &in url, const string &in body, const string &in type = \"application/x-www-form-urlencoded\")", NX_FN(webPost), "Sends something, and waits for the answer.");
        b.names("");
    }
}
