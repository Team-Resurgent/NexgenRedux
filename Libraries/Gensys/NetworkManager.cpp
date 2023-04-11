#include "NetworkManager.h"
#include "DebugUtility.h"
#include "StringUtility.h"

#if defined NEXGEN_WIN
#include <winsock2.h>
#elif defined NEXGEN_OG || defined NEXGEN_360
#include <xtl.h>
#include <winsockx.h>
#endif

#define RECV_SOCKET_BUFFER_SIZE_IN_K 64
#define RECV_SOCKET_BUFFER_SIZE RECV_SOCKET_BUFFER_SIZE_IN_K * 1024
#define SEND_SOCKET_BUFFER_SIZE_IN_K 64
#define SEND_SOCKET_BUFFER_SIZE SEND_SOCKET_BUFFER_SIZE_IN_K * 1024

using namespace Gensys;

bool NetworkManager::Init()
{
#if defined NEXGEN_OG || defined NEXGEN_360

	if (!(XNetGetEthernetLinkStatus() & XNET_ETHERNET_LINK_ACTIVE))
	{
		DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "Ethernet not connected.");
		return false;
	}

	XNetStartupParams xnsp;
	memset(&xnsp, 0, sizeof(xnsp));
	xnsp.cfgSizeOfStruct = sizeof(XNetStartupParams);
	xnsp.cfgFlags = XNET_STARTUP_BYPASS_SECURITY;

#if defined NEXGEN_OG 
	xnsp.cfgPrivatePoolSizeInPages = 64;
	xnsp.cfgEnetReceiveQueueLength = 16;
	xnsp.cfgIpFragMaxSimultaneous = 16;
	xnsp.cfgIpFragMaxPacketDiv256 = 32;
	xnsp.cfgSockMaxSockets = 64;
#endif

	xnsp.cfgSockDefaultRecvBufsizeInK = RECV_SOCKET_BUFFER_SIZE_IN_K;
	xnsp.cfgSockDefaultSendBufsizeInK = SEND_SOCKET_BUFFER_SIZE_IN_K;

	XNetStartup(&xnsp);

	XNADDR addr;
	DWORD dwState;
	do
	{
		dwState = XNetGetTitleXnAddr(&addr);
		Sleep(500);
	} while (dwState == XNET_GET_XNADDR_PENDING);

	if (XNetStartup(&xnsp))
	{
		DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "XNet startup startup failed.");
		return false;
	}

	DebugUtility::LogMessage(DebugUtility::LOGLEVEL_INFO, StringUtility::FormatString("IP = %i.%i.%i.%i", 
		addr.ina.S_un.S_un_b.s_b1,
		addr.ina.S_un.S_un_b.s_b2,
		addr.ina.S_un.S_un_b.s_b3,
		addr.ina.S_un.S_un_b.s_b4
	));

#endif
	
#if defined NEXGEN_WIN || defined NEXGEN_OG
    WSADATA wsData;
	int result = WSAStartup(MAKEWORD(2, 2), &wsData);
	if (result != 0)
	{
		DebugUtility::LogMessage(DebugUtility::LOGLEVEL_INFO, "WSA Startup failed.");
		return false;
	}
#endif

	return true;
}

void NetworkManager::Close()
{
#if defined NEXGEN_WIN || defined NEXGEN_OG
    WSACleanup();
#endif
}