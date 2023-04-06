#include "SocketUtilityOld.h"
#include "StringUtility.h"
#include "DebugUtility.h"

using namespace Gensys;

bool SocketUtilityOld::CreateSocket(int af, int type, int protocol, uint64_t& result)
{
	result = socket(af, type, protocol);
	if (result < 0) 
	{
		DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, StringUtility::FormatString("Create socket failed: %i", WSAGetLastError()));
		return false;
	}
	return true;
}

bool SocketUtilityOld::ConnectSocket(uint64_t socket, sockaddr_in* socket_addr_in)
{
	const int result = connect((SOCKET)socket, (sockaddr*)socket_addr_in, sizeof(SOCKADDR_IN));
	if (result < 0)
	{
		DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, StringUtility::FormatString("Connect socket failed: %i", WSAGetLastError()));
		return false;
	}
	return true;
}

bool SocketUtilityOld::ConnectSocket(uint64_t socket, sockaddr* socket_addr)
{
	const int result = connect((SOCKET)socket, socket_addr, sizeof(sockaddr));
	if (result < 0)
	{
		DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, StringUtility::FormatString("Connect socket failed: %i", WSAGetLastError()));
		return false;
	}
	return true;
}

bool SocketUtilityOld::AcceptSocket(uint64_t socket, uint64_t& result)
{
	result = accept((SOCKET)socket, NULL, 0);
	if (result < 0)
	{
		DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, StringUtility::FormatString("Accept socket failed: %i", WSAGetLastError()));
		return false;
	}
	return true;
}

bool SocketUtilityOld::AcceptSocket(uint64_t socket, sockaddr_in* socket_addr_in, uint64_t& result)
{
	int dw = sizeof(sockaddr_in);
	result = accept((SOCKET)socket, (sockaddr*)socket_addr_in, &dw);
	if (result < 0)
	{
		DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, StringUtility::FormatString("Accept socket failed: %i", WSAGetLastError()));
		return false;
	}
	return true;
}

bool SocketUtilityOld::AcceptSocket(uint64_t socket, sockaddr* socket_addr, uint64_t& result)
{
	int dw = sizeof(sockaddr);
	result = accept((SOCKET)socket, socket_addr, &dw);
	if (result < 0)
	{
		DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, StringUtility::FormatString("Accept socket failed: %i", WSAGetLastError()));
		return false;
	}
	return true;
}

bool SocketUtilityOld::SetSocketOptions(uint64_t socket)
{
	bool success = true;

#if defined NEXGEN_360

	BOOL bBroadcast = TRUE;
	int result = setsockopt((SOCKET)socket, SOL_SOCKET, 0x5802, (PCSTR)&bBroadcast, sizeof(BOOL));
	if (result < 0)
	{
		DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "Set socket option 5802 failed: %i", WSAGetLastError());
		success = false;
	}

	result = setsockopt((SOCKET)socket, SOL_SOCKET, 0x5801, (PCSTR)&bBroadcast, sizeof(BOOL));
	if (result < 0)
	{
		DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "Set socket option 5801 failed: %i", WSAGetLastError());
		success = false;
	}

#endif

	return success;
}


bool SocketUtilityOld::SetSocketRecvSize(uint64_t socket, int &recv_size)
{	
	int recvBufferSize = RECV_SOCKET_BUFFER_SIZE;
	int result = setsockopt((SOCKET)socket, SOL_SOCKET, SO_RCVBUF, (char*)&recvBufferSize, sizeof(int));
	if (result < 0)
	{
		DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, StringUtility::FormatString("Set socket option SO_RCVBUF failed: %i", WSAGetLastError()));
	}

	int isize = sizeof(recvBufferSize);
	result = getsockopt((SOCKET)socket, SOL_SOCKET, SO_RCVBUF, (char*)&recvBufferSize, (int*)&isize);
	if (result < 0)
	{
		recvBufferSize = 8192;
	}

	recv_size = recvBufferSize;
	return true;
}

bool SocketUtilityOld::SetSocketSendSize(uint64_t socket, int &send_size)
{	
	int sendBufferSize = SEND_SOCKET_BUFFER_SIZE;
	int result = setsockopt((SOCKET)socket, SOL_SOCKET, SO_SNDBUF, (char*)&sendBufferSize, sizeof(int));
	if (result < 0)
	{
		DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, StringUtility::FormatString("Set socket option SO_SNDBUF failed: %i", WSAGetLastError()));
	}

	int isize = sizeof(sendBufferSize);
	result = getsockopt((SOCKET)socket, SOL_SOCKET, SO_SNDBUF, (char*)&sendBufferSize, (int*)&isize);
	if (result < 0)
	{
		sendBufferSize = 8192;
	}

	send_size = sendBufferSize;
	return true;
}

bool SocketUtilityOld::GetReadQueueLength(uint64_t socket, int &queue_length)
{
	DWORD temp;
	int result = ioctlsocket((SOCKET)socket, FIONREAD, &temp);
	if (result < 0)
	{
		DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, StringUtility::FormatString("Get read queue length failed: %i", WSAGetLastError()));
		queue_length = 0;
		return false;
	}
	queue_length = temp;
	return true;
}

bool SocketUtilityOld::BindSocket(uint64_t socket, sockaddr_in* socket_addr_in)
{
	int result = bind((SOCKET)socket, (sockaddr*)socket_addr_in, sizeof(sockaddr_in));
	if (result < 0)
	{
		DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, StringUtility::FormatString("Bind socket failed: %i", WSAGetLastError()));
		return false;
	}
	return true;
}

bool SocketUtilityOld::BindSocket(uint64_t socket, sockaddr* socket_addr)
{
	int result = bind((SOCKET)socket, socket_addr, sizeof(sockaddr));
	if (result < 0)
	{
		DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, StringUtility::FormatString("Bind socket failed: %i", WSAGetLastError()));
		return false;
	}
	return true;
}

bool SocketUtilityOld::ListenSocket(uint64_t socket, int count)
{
	int result = listen((SOCKET)socket, count);
	if (result < 0)
	{
		DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, StringUtility::FormatString("Listen socket failed: %i", WSAGetLastError()));
		return false;
	}
	return true;
}

bool SocketUtilityOld::CloseSocket(uint64_t& socket)
{
	if (!socket) 
	{
		DebugUtility::LogMessage(DebugUtility::LOGLEVEL_WARNING, "Close socket called on null socket.");
		return true;
	}
	int result = closesocket((SOCKET)socket);
	if (result < 0)
	{
		DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, StringUtility::FormatString("Close socket failed: %i", WSAGetLastError()));
		socket = 0;
		return false;
	}
	socket = 0;
	return true;
}

bool SocketUtilityOld::GetSocketName(uint64_t socket, sockaddr_in* socket_addr_in)
{
	int size = sizeof(sockaddr_in);
	int result = getsockname((SOCKET)socket, (sockaddr*)socket_addr_in, &size);
	if (result < 0)
	{
		DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, StringUtility::FormatString("Get socket name failed: %i", WSAGetLastError()));
		return false;
	}
	return true;
}

bool SocketUtilityOld::GetSocketName(uint64_t socket, sockaddr* socket_addr)
{
	int size = sizeof(sockaddr);
	const int result = getsockname((SOCKET)socket, socket_addr, &size);
	if (result < 0)
	{
		DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, StringUtility::FormatString("Get socket name failed: %i", WSAGetLastError()));
		return false;
	}
	return true;
}
