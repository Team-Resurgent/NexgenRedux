//#pragma once
//
//#include "SocketUtility.h"
//#include "FileSystem.h"
//#include <cstdio>
//#if defined UWP_ANGLE || defined NEXGEN_WIN
//#include <WinSock2.h>
//#include <Windows.h>
//#elif defined NEXGEN_360 || defined NEXGEN_OG
//#include <xtl.h>
//#include <winsockx.h>
//#endif	
//
//namespace Gensys
//{
//	class FtpServer
//	{
//	public:
//
//		typedef enum _ReceiveStatus {
//			ReceiveStatus_OK = 1,
//			ReceiveStatus_Network_Error,
//			ReceiveStatus_Timeout,
//			ReceiveStatus_Invalid_Data,
//			ReceiveStatus_Insufficient_Buffer
//		} ReceiveStatus;
//
//#ifndef MAC_ANGLE
//
//		static bool WINAPI ConnectionThread(uint64_t pParam);
//
//		static bool WINAPI ListenThread(LPVOID lParam);
//
//#endif // !MAC_ANGLE
//
//#ifndef MAC_ANGLE
//		static bool Init();
//#endif
//		static bool Close();
//		static bool SocketSendString(uint64_t s, const wchar_t *psz);
//		static ReceiveStatus SocketReceiveString(uint64_t s, wchar_t *psz, uint32_t dwMaxChars, uint32_t* pdwCharsReceived);
//		static ReceiveStatus SocketReceiveLetter(uint64_t s, wchar_t* pch, uint32_t dwMaxChars, uint32_t* pdwCharsReceived);
//		static ReceiveStatus SocketReceiveData(uint64_t s, char *psz, uint32_t dwBytesToRead, uint32_t* pdwBytesRead);
//		static uint64_t EstablishDataConnection(sockaddr_in* psaiData, uint64_t* psPasv);
//		static bool ReceiveSocketFile(uint64_t sCmd, uint64_t sData, FileSystem::FileInfo& fileInfo);
//		static bool SendSocketFile(uint64_t sCmd, uint64_t sData, FileSystem::FileInfo& fileInfo, uint32_t* pdwAbortFlag);
//	};
//}
