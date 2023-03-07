#pragma once

#include "Int.h"

#if defined UWP_ANGLE || defined NEXGEN_WIN || defined NEXGEN_UWP
#include <WinSock2.h>
#include <Windows.h>
#elif defined NEXGEN_360 || defined NEXGEN_OG
#include <xtl.h>
#include <winsockx.h>
#endif	

#define RECV_SOCKET_BUFFER_SIZE_IN_K 64
#define RECV_SOCKET_BUFFER_SIZE RECV_SOCKET_BUFFER_SIZE_IN_K * 1024
#define SEND_SOCKET_BUFFER_SIZE_IN_K 64
#define SEND_SOCKET_BUFFER_SIZE SEND_SOCKET_BUFFER_SIZE_IN_K * 1024

#ifdef MAC_ANGLE

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>

//https://handsonnetworkprogramming.com/articles/differences-windows-winsock-linux-unix-bsd-sockets-compatibility/

#endif // __unix__


namespace Gensys
{
	class SocketUtility
	{
	public:
		static bool CreateSocket(int af, int type, int protocol, uint64_t& result);
		static bool ConnectSocket(uint64_t socket, sockaddr_in* socket_addr_in);
		static bool ConnectSocket(uint64_t socket, sockaddr* socket_addr);
		static bool AcceptSocket(uint64_t socket, uint64_t&result);
		static bool AcceptSocket(uint64_t socket, sockaddr_in* socket_addr_in, uint64_t& result);
		static bool AcceptSocket(uint64_t socket, sockaddr* socket_addr, uint64_t& result);
		static bool SetSocketOptions(uint64_t socket);
		static bool SetSocketRecvSize(uint64_t socket, int &recv_size);
		static bool SetSocketSendSize(uint64_t socket, int &send_size);
		static bool GetReadQueueLength(uint64_t socket, int &queue_length);
		static bool BindSocket(uint64_t socket, sockaddr_in*socket_addr_in);
		static bool BindSocket(uint64_t socket, sockaddr* socket_addr);
		static bool ListenSocket(uint64_t socket, int count);
		static bool CloseSocket(uint64_t& socket);
		static bool GetSocketName(uint64_t socket, sockaddr_in* socket_addr_in);
		static bool GetSocketName(uint64_t socket, sockaddr* socket_addr);
	};
}
