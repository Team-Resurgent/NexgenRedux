#pragma once

#define _WINSOCKAPI_
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include "Int.h"

#include <fcntl.h>
#include <cctype>

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

        enum SockState
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
        };

        SocketUtility();
        ~SocketUtility();

        bool Create();
        bool Create(uint32_t Protocol);
        bool Create(uint32_t Protocol, uint32_t Type);
        void Drop();
        bool GetValid();
        uint32_t GetLastCode();
        bool GetBlocking();
        void SetBlocking(bool blocking);
        bool Bind(uint16_t port);
        //bool Bind(const char* host, unsigned short port);
        bool Listen();
        bool Accept(SocketUtility* socket);
        //int Connect(const char* host, unsigned short port);
        void Close();

        SockState GetState();
        void SetState(SockState state);

		void DnsLookup();
		
		//struct hostent* GetHostByName(const char* name)
        struct sockaddr_in GetAddress();
        uint64_t GetUAddress();
        bool IsError();

        bool CanRead();
        bool CanWrite();

        uint32_t GetSock();
        void SetSock(uint32_t sock);
        int Receive(const void* buffer, uint32_t size, uint32_t spos = 0);
        int SendRaw(const void* data, uint32_t dataSize);
        int SendUDP(const void* buffer, uint32_t size, sockaddr_in* to);
        int ReceiveUDP(const void* buffer, uint32_t size, sockaddr_in* from);

    private:

        bool Check();

    private:

        bool m_blocking;
        bool m_valid;
        uint32_t m_lastCode;
        struct sockaddr_in m_addr;

    #if defined NEXGEN_WIN || defined NEXGEN_OG
        WSADATA m_wsda;
    #endif
        uint32_t m_maxcon;
        SockState m_state;
        fd_set m_scks;
        timeval m_times;
        uint32_t m_totaldata;
        uint32_t m_sock;
    };
}