#include "SocketUtility.h" 

#if defined NEXGEN_WIN
#pragma comment(lib,"wsock32.lib")
typedef int socklen_t;
#elif defined NEXGEN_OG
typedef int socklen_t;
#else
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <cstring>
#endif

#if !defined(SOCKET_ERROR)
#define SOCKET_ERROR -1
#endif

#if !defined(SOCKET_NONE)
#define SOCKET_NONE 0
#endif

#if !defined(INVALID_SOCKET)
#define INVALID_SOCKET -1
#endif

using namespace Gensys;

//WSACleanup( );

SocketUtility::SocketUtility()
{
	m_maxcon = 64;
	memset(&m_addr, 0, sizeof(m_addr));

#if defined NEXGEN_WIN || defined NEXGEN_OG
	WSAStartup(MAKEWORD(1, 1), &m_wsda);
#endif

	m_sock = (uint32_t)INVALID_SOCKET;
	m_blocking = false;
	m_valid = false;
	m_times.tv_sec = 0;
	m_times.tv_usec = 0;
	m_state = skDISCONNECTED;
	m_totaldata = 0;
}

SocketUtility::~SocketUtility()
{
	if (Check()) 
    {
		Close();
	}
}

bool SocketUtility::Create()
{
	return this->Create(IPPROTO_TCP, SOCK_STREAM);
}

bool SocketUtility::Create(uint32_t Protocol)
{
	switch (Protocol) 
    {
        case IPPROTO_UDP: 
            return this->Create(IPPROTO_UDP, SOCK_DGRAM);
        default:          
            return this->Create(IPPROTO_TCP, SOCK_STREAM);
	}
}

bool SocketUtility::Create(uint32_t Protocol, uint32_t Type)
{
	if (this->Check()) 
    {
		return false;
	}

	m_state = skDISCONNECTED;
	m_sock = (int)::socket(AF_INET, Type, Protocol);
	m_lastCode = m_sock;

	return m_sock > SOCKET_NONE;
}

void SocketUtility::Drop()
{
	m_sock = SOCKET_NONE;
}

uint32_t SocketUtility::GetLastCode()
{
    return m_lastCode;
}

bool SocketUtility::GetValid()
{
    return m_valid;
}

bool SocketUtility::GetBlocking()
{
    return m_blocking;
}

void SocketUtility::SetBlocking(bool blocking)
{
	if (Check() == false) 
    {
		if (Create() == false) 
        {
			return;
		}
	}

	u_long nonblocking = (blocking ? 0 : 1);
#if defined NEXGEN_WIN || defined NEXGEN_OG
	ioctlsocket(m_sock, FIONBIO, &nonblocking);
#else
	ioctl(m_sock, O_NONBLOCK, &nonblocking);
#endif
	m_blocking = blocking;
}

bool SocketUtility::Bind(uint16_t port)
{
	if (Check() == false && Create() == false) 
    {
		return false;
	}

	m_addr.sin_family = AF_INET;
	m_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	m_addr.sin_port = htons(port);
	m_lastCode = ::bind(m_sock, (struct sockaddr*)&m_addr, sizeof(m_addr));

	return !m_lastCode;
}

//bool SocketUtility::Bind(const char* host, unsigned short port)
//{
//	if (Check() == false && Create() == false) 
//    {
//		return false;
//	}
//
//	struct hostent* phe;
//	phe = gethostbyname(host);
//	if (phe == NULL) {
//		return false;
//	}
//
//	memcpy(&m_addr.sin_addr, phe->h_addr, sizeof(struct in_addr));
//
//	m_addr.sin_family = AF_INET;
//	m_addr.sin_port = htons(port);
//	m_lastCode = ::bind(m_sock, (struct sockaddr*)&m_addr, sizeof(m_addr));
//
//	return !m_lastCode;
//}

bool SocketUtility::Listen()
{
	m_lastCode = ::listen(m_sock, m_maxcon);
	if (m_lastCode == SOCKET_ERROR) 
    {
		return false;
	}

	m_state = skLISTENING;
	m_valid = true;
	return true;
}

bool SocketUtility::Accept(SocketUtility* socket)
{
	if (m_blocking == false && CanRead() == false) 
    {
		return false;
	}

	int length = sizeof(socket->GetAddress());
	socket->SetSock((int)::accept(m_sock, (struct sockaddr*) &socket->m_addr, (socklen_t*)&length));

	m_lastCode = socket->GetSock();
	if (socket->GetSock() == SOCKET_ERROR) 
    {
		return false;
	}

	socket->SetState(skCONNECTED);
	return true;
}

void SocketUtility::Close()
{
	m_state = skDISCONNECTED;

#if defined NEXGEN_WIN || defined NEXGEN_OG
	::closesocket(m_sock);
#else
	::shutdown(m_sock, SHUT_RDWR);
	::close(m_sock);
#endif

	m_sock = (int)INVALID_SOCKET;
}

SocketUtility::SockState SocketUtility::GetState()
{
    return m_state;
}

void SocketUtility::SetState(SocketUtility::SockState state)
{
    m_state = state;
}

//struct hostent* SocketUtility::GetHostByName(const char* name)
//{
//	XNDNS *pxndns;
//	static HOSTENT host;
//	HOSTENT *rvalue;
//
//	if(XNetDnsLookup(name, NULL, &pxndns) != 0)
//		return NULL;
//
//	while (pxndns->iStatus == WSAEINPROGRESS)
//	{
//		msleep(5);
//	}
//
//	if ((pxndns->iStatus == 0) && (pxndns->cina > 0))
//	{
//		static char * ipPtrs[2];
//		static IN_ADDR ip;
//
//		host.h_name = (char*)name;
//		host.h_aliases = NULL;
//		host.h_addrtype = AF_INET;
//		host.h_length = (gsi_u16)sizeof(IN_ADDR);
//		host.h_addr_list = (gsi_i8 **)ipPtrs;
//
//		ip = pxndns->aina[0];
//		ipPtrs[0] = (char *)&ip;
//		ipPtrs[1] = NULL;
//
//		rvalue = &host;
//	}
//	else
//	{
//		rvalue = NULL;
//	}
//	XNetDnsRelease(pxndns);
//
//	return rvalue;
//}

struct sockaddr_in SocketUtility::GetAddress()
{
    return m_addr;
}

uint64_t SocketUtility::GetUAddress()
{
	return m_addr.sin_addr.s_addr;
}

//int SocketUtility::Connect(const char* host, unsigned short port)
//{
//	if (Check() == false && Create() == false) 
//    {
//		return false;
//	}
//
//	struct hostent* phe;
//	phe = gethostbyname(host);
//	if (phe == NULL) 
//    {
//		return 2;
//	}
//
//	memcpy(&m_addr.sin_addr, phe->h_addr, sizeof(struct in_addr));
//
//	m_addr.sin_family = AF_INET;
//	m_addr.sin_port = htons(port);
//
//	if (::connect(m_sock, (struct sockaddr*)&m_addr, sizeof(m_addr)) == SOCKET_ERROR) 
//    {
//		if (m_blocking == true) 
//        {
//			return 3;
//		}
//	}
//
//	m_state = skCONNECTED;
//	m_valid = true;
//	return 0;
//}

bool SocketUtility::CanRead()
{
	FD_ZERO(&m_scks);
	FD_SET((unsigned)m_sock, &m_scks);

	return select((int)m_sock + 1, &m_scks, NULL, NULL, &m_times) > 0;
}

bool SocketUtility::CanWrite()
{
	FD_ZERO(&m_scks);
	FD_SET((unsigned)m_sock, &m_scks);

	return select((int)m_sock + 1, NULL, &m_scks, NULL, &m_times) > 0;
}

bool SocketUtility::IsError()
{
	if (m_state == skERROR || m_sock == -1) 
    {
		return true;
	}

	FD_ZERO(&m_scks);
	FD_SET((unsigned)m_sock, &m_scks);

	if (select((int)m_sock + 1, NULL, NULL, &m_scks, &m_times) >= 0) 
    {
		return false;
	}

	m_state = skERROR;
	return true;
}

uint32_t SocketUtility::GetSock()
{
    return m_sock;
}

void SocketUtility::SetSock(uint32_t sock)
{
    m_sock = sock;
}

int SocketUtility::ReceiveUDP(const void* buffer, uint32_t size, sockaddr_in* from)
{
#if defined NEXGEN_WIN || defined NEXGEN_OG
	int client_length = (int)sizeof(struct sockaddr_in);
#else
	unsigned int client_length = (unsigned int)sizeof(struct sockaddr_in);
#endif
	return recvfrom(m_sock, (char*)buffer, size, 0, (struct sockaddr*)from, &client_length);
}

int SocketUtility::Receive(const void* buffer, uint32_t size, uint32_t spos)
{
	return recv(m_sock, (char*)buffer + spos, size, 0);
}

int SocketUtility::SendUDP(const void* buffer, uint32_t size, sockaddr_in* to)
{
	return sendto(m_sock, (char*)buffer, size, 0, (struct sockaddr *)&to, sizeof(struct sockaddr_in));
}

int SocketUtility::SendRaw(const void* data, uint32_t dataSize)
{
	return send(m_sock, (char*)data, dataSize, 0);
}

// Privates

bool SocketUtility::Check()
{
	return m_sock > SOCKET_NONE;
}
