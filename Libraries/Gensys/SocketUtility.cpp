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

#if defined NEXGEN_OG

struct hostent 
{
	char FAR * h_name;
	char FAR * FAR * h_aliases;
	short h_addrtype;
	short h_length;
	char FAR * FAR * h_addr_list;
	#define h_addr h_addr_list[0]
};

typedef struct 
{
	struct hostent server;
	char name[128];
	char addr[16];
	char* addr_list[4];
} HostEnt;

struct hostent* gethostbyname(const char* name)
{
	HostEnt* server = (HostEnt*)malloc(sizeof(HostEnt));

	strcpy(server->name, "xbox");
	if(!strcmp(server->name, name))
	{
		XNADDR xna;
		DWORD dwState;
		do
		{
			dwState = XNetGetTitleXnAddr(&xna);
		} while (dwState==XNET_GET_XNADDR_PENDING);

		server->addr_list[0] = server->addr;
		memcpy(server->addr, &(xna.ina.s_addr), 4);
		server->server.h_name = server->name;
		server->server.h_aliases = 0;
		server->server.h_addrtype = AF_INET;
		server->server.h_length = 4;
		server->server.h_addr_list = (char**)malloc(sizeof(char*)*4);
		server->server.h_addr_list[0] = server->addr_list[0];
		server->server.h_addr_list[1] = 0;
		return (struct hostent*)server;
	}

	WSAEVENT hEvent = WSACreateEvent();
	XNDNS* pDns = NULL;
	INT err = XNetDnsLookup(name, hEvent, &pDns);
	WaitForSingleObject(hEvent, INFINITE);
	if (pDns && pDns->iStatus == 0)
	{
		strcpy(server->name, name);
		server->addr_list[0] = server->addr;
		memcpy(server->addr, &(pDns->aina[0].s_addr), 4);
		server->server.h_name = server->name;
		server->server.h_aliases = 0;
		server->server.h_addrtype = AF_INET;
		server->server.h_length = 4;
		server->server.h_addr_list = (char**)malloc(sizeof(char*)*4);

		server->server.h_addr_list[0] = server->addr_list[0];
		server->server.h_addr_list[1] = 0;
		XNetDnsRelease(pDns);
		WSACloseEvent(hEvent);
		return (struct hostent*)server;
	}

	if (pDns)
	{
		XNetDnsRelease(pDns);
	}

	if (hEvent)
	{
		WSACloseEvent(hEvent);
	}

	free(server);
	return NULL;
}

#endif

//WSACleanup( );

SocketUtility::SocketUtility()
{
	m_maxcon = 64;
	memset(&m_addr, 0, sizeof(m_addr));

#if defined NEXGEN_WIN || defined NEXGEN_OG
	int result = WSAStartup(MAKEWORD(1, 1), &m_wsda);
	if (result != 0)
	{
		//error
	}
#endif

	m_sock = (uint32_t)INVALID_SOCKET;
	m_isBlocking = false;
	m_isValid = false;
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

bool SocketUtility::Create(uint32_t protocol)
{
	switch (protocol) 
    {
        case IPPROTO_TCP: 
            return this->Create(IPPROTO_TCP, SOCK_STREAM);
        default:          
            return this->Create(IPPROTO_UDP, SOCK_DGRAM);
	}
}

bool SocketUtility::Create(uint32_t protocol, uint32_t type)
{
	if (this->Check()) 
    {
		return false;
	}

	m_state = skDISCONNECTED;
	m_sock = (int)::socket(AF_INET, type, protocol);
	m_lastCode = m_sock;

	return m_sock > SOCKET_NONE;
}

bool SocketUtility::EnableBroadcast()
{
	if (this->Check() == false) 
    {
		return false;
	}

	BOOL bOptVal = TRUE;
    int bOptLen = sizeof(BOOL);
    return setsockopt(m_sock, SOL_SOCKET, SO_BROADCAST, (char*)&bOptVal, bOptLen) != SOCKET_ERROR;
}

void SocketUtility::Drop()
{
	m_sock = SOCKET_NONE;
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

bool SocketUtility::Bind(const char* host, uint16_t port)
{
	if (Check() == false && Create() == false) 
    {
		return false;
	}

	struct hostent* phe;
	phe = gethostbyname(host);
	if (phe == NULL) {
		return false;
	}

	memcpy(&m_addr.sin_addr, phe->h_addr, sizeof(struct in_addr));

	m_addr.sin_family = AF_INET;
	m_addr.sin_port = htons(port);
	m_lastCode = ::bind(m_sock, (struct sockaddr*)&m_addr, sizeof(m_addr));

	return !m_lastCode;
}

bool SocketUtility::Listen()
{
	m_lastCode = ::listen(m_sock, m_maxcon);
	if (m_lastCode == SOCKET_ERROR) 
    {
		return false;
	}

	m_state = skLISTENING;
	m_isValid = true;
	return true;
}

bool SocketUtility::Accept(SocketUtility* socket)
{
	if (m_isBlocking == false && CanRead() == false) 
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


int32_t SocketUtility::Connect(const char* host, uint16_t port)
{
	if (Check() == false && Create() == false) 
    {
		return 1;
	}

	struct hostent* phe;
	phe = gethostbyname(host);
	if (phe == NULL) 
    {
		return 2;
	}

	memcpy(&m_addr.sin_addr, phe->h_addr, sizeof(struct in_addr));

	m_addr.sin_family = AF_INET;
	m_addr.sin_port = htons(port);

	if (::connect(m_sock, (struct sockaddr*)&m_addr, sizeof(m_addr)) == SOCKET_ERROR) 
    {
		if (m_isBlocking == true) 
        {
			return 3;
		}
	}

	m_state = skCONNECTED;
	m_isValid = true;
	return 0;
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

int32_t SocketUtility::Receive(const void* buffer, uint32_t size, uint32_t spos)
{
	return recv(m_sock, (char*)buffer + spos, size, 0);
}

int32_t SocketUtility::ReceiveUDP(const void* buffer, uint32_t size, sockaddr_in& from)
{
#if defined NEXGEN_WIN || defined NEXGEN_OG || defined NEXGEN_360 || defined NEXGEN_UWP
	int client_length = (int)sizeof(from);
#else
	unsigned int client_length = (unsigned int)sizeof(from);
#endif
	return recvfrom(m_sock, (char*)buffer, size, 0, (struct sockaddr*)&from, &client_length);
}

int32_t SocketUtility::Send(const void* data, uint32_t dataSize)
{
	return send(this->m_sock, (char*)data, dataSize, 0);
}

int32_t SocketUtility::SendUDP(const void* buffer, uint32_t size, sockaddr_in& to)
{
	return sendto(m_sock, (char*)buffer, size + 1, 0, (struct sockaddr *)&to, sizeof(to));
}

struct sockaddr_in SocketUtility::GetAddress()
{
    return m_addr;
}

uint64_t SocketUtility::GetUAddress()
{
	return m_addr.sin_addr.s_addr;
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

bool SocketUtility::IsValid()
{
    return m_isValid;
}

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

int32_t SocketUtility::GetLastCode()
{
    return m_lastCode;
}

int32_t SocketUtility::GetSock()
{
    return m_sock;
}

bool SocketUtility::GetIsBlocking()
{
    return m_isBlocking;
}

void SocketUtility::SetIsBlocking(bool blocking)
{
	if (Check() == false && Create() == false) 
    {
		return;
	}

	u_long nonblocking = (blocking ? 0 : 1);
#if defined NEXGEN_WIN || defined NEXGEN_OG
	ioctlsocket(m_sock, FIONBIO, &nonblocking);
#else
	ioctl(m_sock, O_NONBLOCK, &nonblocking);
#endif
	m_isBlocking = blocking;
}

SocketUtility::SockState SocketUtility::GetState()
{
    return m_state;
}

void SocketUtility::SetState(SocketUtility::SockState state)
{
    m_state = state;
}

// Privates

void SocketUtility::SetSock(int32_t sock)
{
    m_sock = sock;
}

bool SocketUtility::Check()
{
	return m_sock > SOCKET_NONE;
}
