#pragma once

#define _WINSOCKAPI_
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include "Int.h"

#include <fcntl.h>
#include <cctype>

#if defined NEXGEN_MAC
#include <sys/_types/_timeval.h> 
#endif

#if defined NEXGEN_WIN
#include <winsock2.h>
#elif defined NEXGEN_OG || defined NEXGEN_360
#include <xtl.h>
#include <winsockx.h>
#else
#include <netinet/in.h>
#include <sys/ioctl.h>
#endif

namespace Gensys
{
    class SocketUtility
    {
    public:

        typedef enum SockState
        {
            skDISCONNECTED = 0,
            skUNDEF1, //Not implemented
            skLISTENING,
            skUNDEF3, //Not implemented
            skUNDEF4, //Not implemented
            skUNDEF5, //Not implemented
            skUNDEF6, //Not implemented
            skCONNECTED,
            skERROR
        } SockState;

        SocketUtility();
        ~SocketUtility();

        bool Create();
        bool Create(uint32_t protocol);
        bool Create(uint32_t protocol, uint32_t type);
        bool EnableBroadcast();
        bool Bind(uint16_t port);
        bool Bind(const char* host, uint16_t port);
        bool Listen();
        bool Accept(SocketUtility* socketUtility);
        int32_t Connect(const char* host, uint16_t port);
        void Close();

        int32_t Receive(const void* buffer, uint32_t size, uint32_t spos = 0);
        int32_t ReceiveUDP(const void* buffer, uint32_t size, sockaddr_in& from);
        int32_t Send(const void* data, uint32_t dataSize);
        int32_t SendUDP(const void* buffer, uint32_t size, sockaddr_in& to);

        struct sockaddr_in GetAddress();
        uint64_t GetUAddress();
        bool IsError();
        bool IsValid();
        bool CanRead();
        bool CanWrite();
        SOCKET GetSock();
        bool GetIsBlocking();
        void SetIsBlocking(bool blocking);
        SockState GetState();
        void SetState(SockState state);

    private:

        void SetSock(SOCKET sock);
        bool Check();

    private:

        bool m_isBlocking;
        bool m_isValid;
        struct sockaddr_in m_addr;

    #if defined NEXGEN_WIN || defined NEXGEN_OG
        WSADATA m_wsda;
    #endif
        uint32_t m_maxcon;
        SockState m_state;
        fd_set m_scks;
        timeval m_times;
        uint32_t m_totaldata;
        SOCKET m_sock;
    };
}