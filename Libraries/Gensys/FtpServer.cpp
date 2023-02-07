//#include "FtpServer.h"
//#include "SocketUtility.h"
//#include "FileSystem.h"
//#include "DriveManager.h"
//#include "crc32.h"
//#include <algorithm>
//#include <string>
//#include "VirtualFileSystem.h"
//#include "TreeNode.h"
//#include "DebugUtility.h"
//#include "StringUtility.h"
//
//using namespace Gensys;
//
//#define SERVERID L"Nexgen FTP, shall we play a game?"
//
//namespace {
//
//#if defined NEXGEN_OG || defined NEXGEN_360
//	LONG activeConnections = 0;
//#else
//	unsigned int activeConnections = 0;
//#endif
//
//	int mMaxConnections;
//	int mCommandTimeout;
//	int mConnectTimeout;
//
//	bool mStopped;
//
//	uint64_t sListen;
//	sockaddr_in saiListen;
//
//	int IncrementConnections()
//	{
//		return (int)_InterlockedIncrement(&activeConnections);
//	}
//
//	void DecrementConnections()
//	{
//		_InterlockedDecrement(&activeConnections);
//	}
//
//}
//
//#ifndef MAC_ANGLE
//
//bool WINAPI FtpServer::ConnectionThread(uint64_t sCmd)
//{
//	uint64_t sData = 0;
//	uint64_t sPasv = 0;
//	wchar_t szPeerName[64];
//	wchar_t szOutput[1024];
//	wchar_t szCmd[512];
//	wchar_t *pszParam;
//	std::wstring strUser;
//	std::wstring strCurrentVirtual;
//	std::wstring strNewVirtual;
//	std::wstring strRnFr;
//	uint32_t dw = 0;
//	uint32_t dwRestOffset = 0;
//	ReceiveStatus status = ReceiveStatus_OK;
//	bool isLoggedIn = false;
//	FileSystem::FileInfo fileInfo;
//	SYSTEMTIME st;
//	FILETIME ft;
//	VirtualFileSystem *pVFS = new VirtualFileSystem();
//	VirtualFileSystem::ListingType listing;
//	UINT_PTR i;
//
//	SOCKADDR_IN saiCmd;
//	memset(&saiCmd, 0, sizeof(SOCKADDR_IN));
//	SOCKADDR_IN saiCmdPeer;
//	memset(&saiCmdPeer, 0, sizeof(SOCKADDR_IN));
//	SOCKADDR_IN saiPasv;
//	memset(&saiPasv, 0, sizeof(SOCKADDR_IN));
//	SOCKADDR_IN saiData;
//	memset(&saiData, 0, sizeof(SOCKADDR_IN));
//
//	// Get peer address
//	dw=sizeof(SOCKADDR_IN);
//	getpeername((SOCKET)sCmd, (SOCKADDR *)&saiCmdPeer, (int *)&dw);
//	swprintf(szPeerName, L"%u.%u.%u.%u",
//		saiCmdPeer.sin_addr.S_un.S_un_b.s_b1,
//		saiCmdPeer.sin_addr.S_un.S_un_b.s_b2, 
//		saiCmdPeer.sin_addr.S_un.S_un_b.s_b3, 
//		saiCmdPeer.sin_addr.S_un.S_un_b.s_b4);
//
//	// Send greeting
//	swprintf(szOutput, L"220-%s\r\n220-You are connecting from %s:%u.\r\n220 Proceed with login.\r\n", SERVERID, szPeerName, ntohs(saiCmdPeer.sin_port));
//	SocketSendString(sCmd, szOutput);
//
//	SocketUtility::GetSocketName(sCmd, &saiCmd);
//
//	// Command processing loop
//	for (;;) {
//
//		status = SocketReceiveString(sCmd, szCmd, 512, &dw);
//
//		if (status == ReceiveStatus_Network_Error) {
//			SocketSendString(sCmd, L"421 Network error.\r\n");
//			break;
//		} else if (status == ReceiveStatus_Timeout) {
//			SocketSendString(sCmd, L"421 Connection timed out.\r\n");
//			break;
//		} else if (status == ReceiveStatus_Invalid_Data) {
//			SocketSendString(sCmd, L"500 Malformed request.\r\n");
//			continue;
//		} else if (status == ReceiveStatus_Insufficient_Buffer) {
//			SocketSendString(sCmd, L"500 Command line too long.\r\n");
//			continue;
//		}
//
//		if (pszParam = wcschr(szCmd, L' ')) {
//			*(pszParam++) = 0;
//		} else {
//			pszParam = szCmd + wcslen(szCmd);
//		}
//
//		if (!_wcsicmp(szCmd, L"USER")) {
//			if (!*pszParam) {
//				SocketSendString(sCmd, L"501 Syntax error in parameters or arguments.\r\n");
//				continue;
//			} else if (isLoggedIn) {
//				SocketSendString(sCmd, L"503 Already logged in. Use REIN to change users.\r\n");
//				continue;
//			} else {
//				strUser = pszParam;
//				if (strUser == L"xbox") {
//					swprintf(szOutput, L"331 Need password for user \"%s\".\r\n", strUser.c_str());
//					SocketSendString(sCmd, szOutput);
//					continue;
//				}
//			}
//		}
//
//		if (!_wcsicmp(szCmd, L"PASS")) {
//			if (strUser.empty()) {
//				SocketSendString(sCmd, L"503 Bad sequence of commands. Send USER first.\r\n");
//			} else if (isLoggedIn) {
//				SocketSendString(sCmd, L"503 Already logged in. Use REIN to change users.\r\n");
//			} else {
//				if (strUser == L"xbox" && std::wstring(pszParam) == L"xbox") {
//					if (IncrementConnections() <= mMaxConnections) {
//						isLoggedIn = true;
//						strCurrentVirtual = L"/";
//						swprintf(szOutput, L"230 User \"%s\" logged in.\r\n", strUser.c_str());
//						SocketSendString(sCmd, szOutput);
//					} else {
//						DecrementConnections();
//						SocketSendString(sCmd, L"421 Your login was refused due to a server connection limit.\r\n");
//						break;
//					}
//				} else {
//					SocketSendString(sCmd, L"530 Incorrect password.\r\n");
//				}
//			}
//		}
//
//		else if (!_wcsicmp(szCmd, L"REIN")) {
//			if (isLoggedIn) {
//				isLoggedIn = false;
//				DecrementConnections();
//				swprintf(szOutput, L"220-User \"%s\" logged out.\r\n", strUser.c_str());
//				SocketSendString(sCmd, szOutput);
//				strUser.clear();
//			}
//			SocketSendString(sCmd, L"220 REIN command successful.\r\n");
//		}
//
//		else if (!_wcsicmp(szCmd, L"HELP")) {
//			SocketSendString(sCmd, L"214 For help, please cry to mom.\r\n");
//		}
//
//		else if (!_wcsicmp(szCmd, L"FEAT")) {
//			SocketSendString(sCmd, L"211-Extensions supported:\r\n SIZE\r\n REST STREAM\r\n MDTM\r\n TVFS\r\n XCRC\r\n211 END\r\n");
//		}
//
//		else if (!_wcsicmp(szCmd, L"SYST")) {
//			swprintf(szOutput, L"215 WIN32 Type: L8 Version: %s\r\n", SERVERID);
//			SocketSendString(sCmd, szOutput);
//		}
//
//		else if (!_wcsicmp(szCmd, L"QUIT")) {
//			if (isLoggedIn) {
//				isLoggedIn = false;
//				DecrementConnections();
//				swprintf(szOutput, L"221-User \"%s\" logged out.\r\n", strUser.c_str());
//				SocketSendString(sCmd, szOutput);
//			}
//			SocketSendString(sCmd, L"221 Goodbye!\r\n");
//			break;
//		}
//
//		else if (!_wcsicmp(szCmd, L"NOOP")) {
//			SocketSendString(sCmd, L"200 NOOP command successful.\r\n");
//		}
//
//		else if (!_wcsicmp(szCmd, L"PWD") || !_wcsicmp(szCmd, L"XPWD")) {
//			if (!isLoggedIn) {
//				SocketSendString(sCmd, L"530 Not logged in.\r\n");
//			} else {
//				swprintf(szOutput, L"257 \"%s\" is current directory.\r\n", strCurrentVirtual.c_str());
//				SocketSendString(sCmd, szOutput);
//			}
//		}
//
//		else if (!_wcsicmp(szCmd, L"CWD") || !_wcsicmp(szCmd, L"XCWD")) {
//			if (!*pszParam) {
//				SocketSendString(sCmd, L"501 Syntax error in parameters or arguments.\r\n");
//			} else if (!isLoggedIn) {
//				SocketSendString(sCmd, L"530 Not logged in.\r\n");
//			} else {
//				pVFS->ResolveRelative(strCurrentVirtual.c_str(), pszParam, strNewVirtual);
//				if (pVFS->IsFolder(strNewVirtual.c_str())) {
//					strCurrentVirtual = strNewVirtual;
//					swprintf(szOutput, L"250 \"%s\" is now current directory.\r\n", strNewVirtual.c_str());
//					SocketSendString(sCmd, szOutput);
//				} else {
//					swprintf(szOutput, L"550 %s failed. \"%s\": directory not found.\r\n", szCmd, strNewVirtual.c_str());
//					SocketSendString(sCmd, szOutput);
//				}
//			}
//		}
//
//		else if (!_wcsicmp(szCmd, L"CDUP") || !_wcsicmp(szCmd, L"XCUP")) {
//			if (!isLoggedIn) {
//				SocketSendString(sCmd, L"530 Not logged in.\r\n");
//			} else {
//				pVFS->ResolveRelative(strCurrentVirtual.c_str(), L"..", strNewVirtual);
//				strCurrentVirtual = strNewVirtual;
//				swprintf(szOutput, L"250 \"%s\" is now current directory.\r\n", strCurrentVirtual.c_str());
//				SocketSendString(sCmd, szOutput);
//			}
//		}
//
//		else if (!_wcsicmp(szCmd, L"TYPE")) {
//			if (!*pszParam) {
//				SocketSendString(sCmd, L"501 Syntax error in parameters or arguments.\r\n");
//			} else if (!isLoggedIn) {
//				SocketSendString(sCmd, L"530 Not logged in.\r\n");
//			} else {
//				SocketSendString(sCmd, L"200 TYPE command successful.\r\n");
//			}
//		}
//
//		else if (!_wcsicmp(szCmd, L"REST")) {
//			if (!*pszParam || (!(dw = StringUtility::ToInt(pszParam)) && (*pszParam != L'0'))) {
//				SocketSendString(sCmd, L"501 Syntax error in parameters or arguments.\r\n");
//			} else if (!isLoggedIn) {
//				SocketSendString(sCmd, L"530 Not logged in.\r\n");
//			} else {
//				dwRestOffset = dw;
//				swprintf(szOutput, L"350 Ready to resume transfer at %u bytes.\r\n", dwRestOffset);
//				SocketSendString(sCmd, szOutput);
//			}
//		}
//
//		else if (!_wcsicmp(szCmd, L"PORT")) {
//			if (!*pszParam) {
//				SocketSendString(sCmd, L"501 Syntax error in parameters or arguments.\r\n");
//			} else if (!isLoggedIn) {
//				SocketSendString(sCmd, L"530 Not logged in.\r\n");
//			} else {
//				memset(&saiData, 0, sizeof(SOCKADDR_IN));
//				saiData.sin_family = AF_INET;
//
//				unsigned char h1 = 0;
//				unsigned char h2 = 0;
//				unsigned char h3 = 0;
//				unsigned char h4 = 0;
//				unsigned char p1 = 0;
//				unsigned char p2 = 0;
//
//				dw = wscanf(pszParam, "%hu,%hu,%hu,%hu,%hu,%hu", &h1, &h2, &h3, &h4, &p1, &p2);
//				
//				saiData.sin_addr.S_un.S_un_b.s_b1 = h1;
//				saiData.sin_addr.S_un.S_un_b.s_b2 = h2;
//				saiData.sin_addr.S_un.S_un_b.s_b3 = h3;
//				saiData.sin_addr.S_un.S_un_b.s_b4 = h4;
//				saiData.sin_port = (p2 << 8) + p1;
//
//				if (dw == 6) {
//					SocketUtility::CloseSocket(sPasv);
//					SocketSendString(sCmd, L"200 PORT command successful.\r\n");
//				} else {
//					SocketSendString(sCmd, L"501 Syntax error in parameters or arguments.\r\n");
//					memset(&saiData, 0, sizeof(SOCKADDR_IN));
//				}
//			}
//		}
//
//		else if (!_wcsicmp(szCmd, L"PASV")) {
//			if (!isLoggedIn) {
//				SocketSendString(sCmd, L"530 Not logged in.\r\n");
//			} else {
//
//				if (sPasv) {
//					SocketUtility::CloseSocket(sPasv);
//				}
//
//				static u_short portTest = 6000;
//
//#if defined NEXGEN_OG || defined NEXGEN_360
//
//				XNADDR addr;
//				XNetGetTitleXnAddr(&addr);
//
//				bool found = false;
//				while (!found) {			
//					memset(&saiPasv, 0, sizeof(SOCKADDR_IN));
//					saiPasv.sin_family = AF_INET;
//					saiPasv.sin_addr.s_addr = INADDR_ANY;
//					saiPasv.sin_port = portTest;		
//					if (SocketUtility::CreateSocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, sPasv))
//					{
//						DebugUtility::LogMessage(DebugUtility::LOGLEVEL_WARNING, L"Could not create socket on port %u, trying to find next available.\n", portTest);
//						found = true;
//					}
//					portTest++;
//					if (portTest > 6999)
//					{
//						portTest = 6000;
//					}
//				}
//
//				SocketUtility::SetSocketOptions(sPasv);
//				SocketUtility::BindSocket(sPasv, &saiPasv);
//				SocketUtility::ListenSocket(sPasv, 1);
//				SocketUtility::GetSocketName(sPasv, &saiPasv);
//
//				swprintf(szOutput, L"227 Entering Passive Mode (%u,%u,%u,%u,%u,%u)\r\n", 
//					addr.ina.S_un.S_un_b.s_b1,
//					addr.ina.S_un.S_un_b.s_b2,
//					addr.ina.S_un.S_un_b.s_b3,
//					addr.ina.S_un.S_un_b.s_b4,	
//					((unsigned char *)&saiPasv.sin_port)[0], 
//					((unsigned char *)&saiPasv.sin_port)[1]);
//				SocketSendString(sCmd, szOutput);
//
//#else
//
//				bool found = false;
//				while (!found) {
//					memset(&saiPasv, 0, sizeof(SOCKADDR_IN));
//					saiPasv.sin_family = AF_INET;
//					saiPasv.sin_addr.s_addr = INADDR_ANY;
//					saiPasv.sin_port = portTest;
//					if (SocketUtility::CreateSocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, sPasv))
//					{
//						DebugUtility::LogMessage(DebugUtility::LOGLEVEL_WARNING, L"Could not create socket on port %u, trying to find next available.\n", portTest);
//						found = true;
//					}
//					portTest++;
//					if (portTest > 6999)
//					{
//						portTest = 6000;
//					}
//				}
//
//				SocketUtility::CreateSocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, sPasv);
//				SocketUtility::SetSocketOptions(sPasv);
//				SocketUtility::BindSocket(sPasv, &saiPasv);
//				SocketUtility::ListenSocket(sPasv, 1);
//				SocketUtility::GetSocketName(sPasv, &saiPasv);
//
//				swprintf(szOutput, L"227 Entering Passive Mode (%u,%u,%u,%u,%u,%u)\r\n",
//					saiCmd.sin_addr.S_un.S_un_b.s_b1, 
//					saiCmd.sin_addr.S_un.S_un_b.s_b2, 
//					saiCmd.sin_addr.S_un.S_un_b.s_b3, 
//					saiCmd.sin_addr.S_un.S_un_b.s_b4, 
//					((unsigned char *)&saiPasv.sin_port)[0], 
//					((unsigned char *)&saiPasv.sin_port)[1]);
//				SocketSendString(sCmd, szOutput);
//
//#endif
//
//			}
//		}
//
//		else if (!_wcsicmp(szCmd, L"LIST") || !_wcsicmp(szCmd, L"NLST")) {
//			if (!isLoggedIn) {
//				SocketSendString(sCmd, L"530 Not logged in.\r\n");
//			} else {
//				if (*pszParam == L'-') if (pszParam = wcschr(pszParam, L' ')) pszParam++;
//				if (pszParam && *pszParam) {
//					pVFS->ResolveRelative(strCurrentVirtual.c_str(), pszParam, strNewVirtual);
//				}
//				else {
//					strNewVirtual = strCurrentVirtual;
//				}	
//				if (pVFS->GetDirectoryListing(strNewVirtual.c_str(), _wcsicmp(szCmd, L"LIST"), listing)) {
//					swprintf(szOutput, L"150 Opening %s mode data connection for listing of \"%s\".\r\n", sPasv ? L"passive" : L"active", strNewVirtual.c_str());
//					SocketSendString(sCmd, szOutput);
//					sData = EstablishDataConnection(&saiData, &sPasv);
//					if (sData) {
//						for (VirtualFileSystem::ListingType::const_iterator it = listing.begin(); it != listing.end(); ++it) {
//							SocketSendString(sData, it->second.c_str());
//						}
//						listing.clear();
//						SocketUtility::CloseSocket(sData);
//						swprintf(szOutput, L"226 %s command successful.\r\n", _wcsicmp(szCmd, L"NLST") ? L"LIST" : L"NLST");
//						SocketSendString(sCmd, szOutput);
//					} else {
//						listing.clear();
//						SocketSendString(sCmd, L"425 Can't open data connection.\r\n");
//					}
//				} else {
//					swprintf(szOutput, L"550 \"%s\": Path not found.\r\n", strNewVirtual.c_str());
//					SocketSendString(sCmd, szOutput);
//				}
//			}
//		}
//
//		else if (!_wcsicmp(szCmd, L"STAT")) {
//			if (!isLoggedIn) {
//				SocketSendString(sCmd, L"530 Not logged in.\r\n");
//			} else {
//				if (*pszParam == L'-') if (pszParam = wcschr(pszParam, L' ')) pszParam++;
//				if (pszParam && *pszParam) {
//					pVFS->ResolveRelative(strCurrentVirtual.c_str(), pszParam, strNewVirtual);
//				}
//				else {
//					strNewVirtual = strCurrentVirtual;
//				}
//				if (pVFS->GetDirectoryListing(strNewVirtual.c_str(), 1, listing)) {
//					swprintf(szOutput, L"212-Sending directory listing of \"%s\".\r\n", strNewVirtual.c_str());
//					SocketSendString(sCmd,szOutput);
//					for (VirtualFileSystem::ListingType::const_iterator it = listing.begin(); it != listing.end(); ++it) {
//						SocketSendString(sCmd, it->second.c_str());
//					}
//					listing.clear();
//					SocketSendString(sCmd, L"212 STAT command successful.\r\n");
//				} else {
//					swprintf(szOutput, L"550 \"%s\": Path not found.\r\n", strNewVirtual.c_str());
//					SocketSendString(sCmd, szOutput);
//				}
//			}
//		}
//
//		else if (!_wcsicmp(szCmd, L"RETR")) {
//			if (!*pszParam) {
//				SocketSendString(sCmd, L"501 Syntax error in parameters or arguments.\r\n");
//			} else if (!isLoggedIn) {
//				SocketSendString(sCmd, L"530 Not logged in.\r\n");
//			} else {
//				pVFS->ResolveRelative(strCurrentVirtual.c_str(), pszParam, strNewVirtual);
//				bool created = pVFS->FileCreate(strNewVirtual.c_str(), FileSystem::FileAccessMode::Read, FileSystem::FileCreateMode::OpenExisting, fileInfo);
//				if (!created) {
//					swprintf(szOutput, L"550 \"%s\": Unable to open file.\r\n", strNewVirtual.c_str());
//					SocketSendString(sCmd, szOutput);
//				} else {
//					if (dwRestOffset) {
//						SetFilePointer(fileInfo.file, dwRestOffset, 0, FILE_BEGIN);
//						dwRestOffset = 0;
//					}
//					swprintf(szOutput, L"150 Opening %s mode data connection for \"%s\".\r\n", sPasv ? L"passive" : L"active", strNewVirtual.c_str());
//					SocketSendString(sCmd, szOutput);
//					sData = EstablishDataConnection(&saiData, &sPasv);
//					if (sData) {
//						if (SendSocketFile(sCmd, sData, fileInfo, &dw)) {
//							swprintf(szOutput, L"226 \"%s\" transferred successfully.\r\n", strNewVirtual.c_str());
//							SocketSendString(sCmd, szOutput);
//						} else {
//							SocketSendString(sCmd, L"426 Connection closed; transfer aborted.\r\n");
//							if (dw) {
//								SocketSendString(sCmd, L"226 ABOR command successful.\r\n");
//							}
//						}
//						SocketUtility::CloseSocket(sData);
//					} else {
//						SocketSendString(sCmd, L"425 Can't open data connection.\r\n");
//					}
//					FileSystem::FileClose(fileInfo);
//				}
//
//			}
//		}
//
//		else if (!_wcsicmp(szCmd, L"STOR") || !_wcsicmp(szCmd, L"APPE")) {
//			if (!*pszParam) {
//				SocketSendString(sCmd, L"501 Syntax error in parameters or arguments.\r\n");
//			} else if (!isLoggedIn) {
//				SocketSendString(sCmd, L"530 Not logged in.\r\n");
//			} else {
//				pVFS->ResolveRelative(strCurrentVirtual.c_str(), pszParam, strNewVirtual);
//				bool opened = pVFS->FileCreate(strNewVirtual.c_str(), FileSystem::FileAccessMode::Write, FileSystem::FileCreateMode::OpenAlways, fileInfo);
//				if (!opened) {
//					swprintf(szOutput, L"550 \"%s\": Unable to open file.\r\n", strNewVirtual.c_str());
//					SocketSendString(sCmd, szOutput);
//				} else {
//					if (_wcsicmp(szCmd, L"APPE") == 0) {
//						SetFilePointer(fileInfo.file, 0, 0, FILE_END);
//					} else {
//						SetFilePointer(fileInfo.file, dwRestOffset, 0, FILE_BEGIN);
//						SetEndOfFile(fileInfo.file);
//					}
//					dwRestOffset = 0;
//					swprintf(szOutput, L"150 Opening %s mode data connection for \"%s\".\r\n", sPasv ? L"passive" : L"active", strNewVirtual.c_str());
//					SocketSendString(sCmd, szOutput);
//					sData = EstablishDataConnection(&saiData, &sPasv);
//					if (sData) {
//						if (ReceiveSocketFile(sCmd, sData, fileInfo)) {
//							swprintf(szOutput, L"226 \"%s\" transferred successfully.\r\n", strNewVirtual.c_str());
//							SocketSendString(sCmd, szOutput);
//						} else {
//							SocketSendString(sCmd, L"426 Connection closed; transfer aborted.\r\n");
//						}
//						SocketUtility::CloseSocket(sData);
//					} else {
//						SocketSendString(sCmd, L"425 Can't open data connection.\r\n");
//					}
//					FileSystem::FileClose(fileInfo);
//				}
//
//			}
//		}
//
//		else if (!_wcsicmp(szCmd, L"ABOR")) {
//		SocketUtility::CloseSocket(sPasv);
//			dwRestOffset = 0;
//			SocketSendString(sCmd, L"200 ABOR command successful.\r\n");
//		}
//
//		else if (!_wcsicmp(szCmd, L"SIZE")) {
//			if (!*pszParam) {
//				SocketSendString(sCmd, L"501 Syntax error in parameters or arguments.\r\n");
//			} else if (!isLoggedIn) {
//				SocketSendString(sCmd, L"530 Not logged in.\r\n");
//			} else {
//				pVFS->ResolveRelative(strCurrentVirtual.c_str(), pszParam, strNewVirtual);
//				bool opened = pVFS->FileCreate(strNewVirtual.c_str(), FileSystem::FileAccessMode::Read, FileSystem::FileCreateMode::OpenExisting, fileInfo);
//				if (opened) {
//					swprintf(szOutput, L"550 \"%s\": File not found.\r\n", strNewVirtual.c_str());
//					SocketSendString(sCmd, szOutput);
//				} else {
//					uint32_t fileSize = 0;
//					FileSystem::FileSize(fileInfo, fileSize);
//					swprintf(szOutput, L"213 %llu\r\n", fileSize);
//					SocketSendString(sCmd, szOutput);
//					FileSystem::FileClose(fileInfo);
//				}
//			}
//		}
//
//		else if (!_wcsicmp(szCmd, L"MDTM")) {
//			if (!*pszParam) {
//				SocketSendString(sCmd, L"501 Syntax error in parameters or arguments.\r\n");
//			} else if (!isLoggedIn) {
//				SocketSendString(sCmd, L"530 Not logged in.\r\n");
//			} else {
//				for (i = 0; i < 14; i++) {
//					if ((pszParam[i] < L'0') || (pszParam[i] > L'9')) {
//						break;
//					}
//				}
//				if ((i == 14) && (pszParam[14] == L' ')) {
//					wcsncpy(szOutput, pszParam, 4);
//					szOutput[4] = 0;
//					st.wYear = (WORD)StringUtility::ToInt(szOutput);
//					wcsncpy(szOutput, pszParam + 4, 2);
//					szOutput[2] = 0;
//					st.wMonth = (WORD)StringUtility::ToInt(szOutput);
//					wcsncpy(szOutput, pszParam + 6, 2);
//					st.wDay = (WORD)StringUtility::ToInt(szOutput);
//					wcsncpy(szOutput, pszParam + 8, 2);
//					st.wHour = (WORD)StringUtility::ToInt(szOutput);
//					wcsncpy(szOutput, pszParam + 10, 2);
//					st.wMinute = (WORD)StringUtility::ToInt(szOutput);
//					wcsncpy(szOutput, pszParam + 12, 2);
//					st.wSecond = (WORD)StringUtility::ToInt(szOutput);
//					pszParam += 15;
//					dw = 1;
//				} else {
//					dw = 0;
//				}
//				pVFS->ResolveRelative(strCurrentVirtual.c_str(), pszParam, strNewVirtual);
//				if (dw) {
//					bool opened = pVFS->FileCreate(strNewVirtual.c_str(), FileSystem::FileAccessMode::Write, FileSystem::FileCreateMode::OpenExisting, fileInfo);
//					if (!opened) {
//						swprintf(szOutput, L"550 \"%s\": File not found.\r\n", strNewVirtual.c_str());
//						SocketSendString(sCmd, szOutput);
//					} else {
//						SystemTimeToFileTime(&st, &ft);
//						SetFileTime(fileInfo.file, 0, 0, &ft);
//						FileSystem::FileClose(fileInfo);
//						SocketSendString(sCmd, L"250 MDTM command successful.\r\n");
//					}
//	
//				} else {
//		
//					bool opened = pVFS->FileCreate(strNewVirtual.c_str(), FileSystem::FileAccessMode::Read, FileSystem::FileCreateMode::OpenExisting, fileInfo);
//					if (!opened) {
//						swprintf(szOutput, L"550 \"%s\": File not found.\r\n", strNewVirtual.c_str());
//						SocketSendString(sCmd, szOutput);
//					} else {
//						GetFileTime(fileInfo.file, 0, 0, &ft);
//						FileSystem::FileClose(fileInfo);
//						FileTimeToSystemTime(&ft, &st);
//						swprintf(szOutput, L"213 %04u%02u%02u%02u%02u%02u\r\n", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
//						SocketSendString(sCmd, szOutput);
//					}
//
//				}
//			}
//		}
//
//		else if (!_wcsicmp(szCmd, L"DELE")) {
//			if (!*pszParam) {
//				SocketSendString(sCmd, L"501 Syntax error in parameters or arguments.\r\n");
//			} else if (!isLoggedIn) {
//				SocketSendString(sCmd, L"530 Not logged in.\r\n");
//			} else {
//				pVFS->ResolveRelative(strCurrentVirtual.c_str(), pszParam, strNewVirtual);
//
//				if (pVFS->FileExists(strNewVirtual.c_str())) {
//					if (pVFS->FileDelete(strNewVirtual.c_str())) {
//						swprintf(szOutput, L"250 \"%s\" deleted successfully.\r\n", strNewVirtual.c_str());
//						SocketSendString(sCmd, szOutput);
//					} else {
//						swprintf(szOutput, L"550 \"%s\": Unable to delete file.\r\n", strNewVirtual.c_str());
//						SocketSendString(sCmd, szOutput);
//					}
//				} else {
//					swprintf(szOutput, L"550 \"%s\": File not found.\r\n", strNewVirtual.c_str());
//					SocketSendString(sCmd, szOutput);
//				}
//
//			}
//		}
//
//		else if (!_wcsicmp(szCmd, L"RNFR")) {
//			if (!*pszParam) {
//				SocketSendString(sCmd, L"501 Syntax error in parameters or arguments.\r\n");
//			} else if (!isLoggedIn) {
//				SocketSendString(sCmd, L"530 Not logged in.\r\n");
//			} else {
//				pVFS->ResolveRelative(strCurrentVirtual.c_str(), pszParam, strNewVirtual);
//
//				if (pVFS->FileExists(strNewVirtual.c_str())) {
//					strRnFr = strNewVirtual;
//					swprintf(szOutput, L"350 \"%s\": File exists; proceed with RNTO.\r\n", strNewVirtual.c_str());
//					SocketSendString(sCmd, szOutput);
//				} else {
//					swprintf(szOutput, L"550 \"%s\": File not found.\r\n", strNewVirtual.c_str());
//					SocketSendString(sCmd, szOutput);
//				}
//	
//			}
//		}
//
//		else if (!_wcsicmp(szCmd, L"RNTO")) {
//			if (!*pszParam) {
//				SocketSendString(sCmd, L"501 Syntax error in parameters or arguments.\r\n");
//			} else if (!isLoggedIn) {
//				SocketSendString(sCmd, L"530 Not logged in.\r\n");
//			} else if (strRnFr.length() == 0) {
//				SocketSendString(sCmd, L"503 Bad sequence of commands. Send RNFR first.\r\n");
//			} else {
//				pVFS->ResolveRelative(strCurrentVirtual.c_str(), pszParam, strNewVirtual);
//
//					if (pVFS->FileMove(strRnFr.c_str(), strNewVirtual.c_str())) {
//						SocketSendString(sCmd, L"250 RNTO command successful.\r\n");
//						strRnFr.clear();
//					} else {
//						swprintf(szOutput, L"553 \"%s\": Unable to rename file.\r\n", strNewVirtual.c_str());
//						SocketSendString(sCmd, szOutput);
//					}
//	
//			}
//		}
//
//		else if (!_wcsicmp(szCmd, L"MKD") || !_wcsicmp(szCmd, L"XMKD")) {
//			if (!*pszParam) {
//				SocketSendString(sCmd, L"501 Syntax error in parameters or arguments.\r\n");
//			} else if (!isLoggedIn) {
//				SocketSendString(sCmd, L"530 Not logged in.\r\n");
//			} else {
//				pVFS->ResolveRelative(strCurrentVirtual.c_str(), pszParam, strNewVirtual);
//				if (pVFS->DirectoryCreate(strNewVirtual.c_str())) {
//					swprintf(szOutput, L"250 \"%s\" created successfully.\r\n", strNewVirtual.c_str());
//					SocketSendString(sCmd, szOutput);
//				} else {
//					swprintf(szOutput, L"550 \"%s\": Unable to create directory.\r\n", strNewVirtual.c_str());
//					SocketSendString(sCmd, szOutput);
//				}
//			}
//		}
//
//		else if (!_wcsicmp(szCmd, L"RMD") || !_wcsicmp(szCmd, L"XRMD")) {
//			if (!*pszParam) {
//				SocketSendString(sCmd, L"501 Syntax error in parameters or arguments.\r\n");
//			} else if (!isLoggedIn) {
//				SocketSendString(sCmd, L"530 Not logged in.\r\n");
//			} else {
//				pVFS->ResolveRelative(strCurrentVirtual.c_str(), pszParam, strNewVirtual);
//				if (pVFS->DirectoryDelete(strNewVirtual.c_str())) {
//					swprintf(szOutput, L"250 \"%s\" removed successfully.\r\n", strNewVirtual.c_str());
//					SocketSendString(sCmd, szOutput);
//				} else {
//					swprintf(szOutput, L"550 \"%s\": Unable to remove directory.\r\n", strNewVirtual.c_str());
//					SocketSendString(sCmd, szOutput);
//				}
//			}
//		}
//
//		else if (!_wcsicmp(szCmd, L"XCRC")) {
//			if (!*pszParam) {
//				SocketSendString(sCmd, L"501 Syntax error in parameters or arguments.\r\n");
//			} else if (!isLoggedIn) {
//				SocketSendString(sCmd, L"530 Not logged in.\r\n");
//			} else {
//				pVFS->ResolveRelative(strCurrentVirtual.c_str(), pszParam, strNewVirtual);							
//				if (pVFS->FileExists(strNewVirtual.c_str())) {			
//					bool opened = pVFS->FileCreate(strNewVirtual.c_str(), FileSystem::FileAccessMode::Read, FileSystem::FileCreateMode::OpenExisting, fileInfo);
//					if (!opened) {
//						swprintf(szOutput, L"550 \"%s\": Unable to open file.\r\n", strNewVirtual.c_str());
//						SocketSendString(sCmd, szOutput);
//					} else {
//						Crc32 *crc = new Crc32();
//						unsigned long checksum = crc->CalcCrc(fileInfo);
//						swprintf(szOutput, L"250 %08X\r\n", checksum);
//						delete crc;
//						SocketSendString(sCmd, szOutput);
//					}
//					FileSystem::FileClose(fileInfo);
//				} else {
//					swprintf(szOutput, L"550 \"%s\": File not found.\r\n", strNewVirtual.c_str());
//					SocketSendString(sCmd, szOutput);
//				}
//			}
//		}
//
//		else if (!_wcsicmp(szCmd, L"OPTS")) {
//			if (!*pszParam) {
//				SocketSendString(sCmd, L"501 Syntax error in parameters or arguments.\r\n");
//			} else if (!_wcsicmp(pszParam, L"UTF8 On")) {
//				SocketSendString(sCmd, L"200 Always in UTF8 mode.\r\n");
//			} else {
//				SocketSendString(sCmd, L"501 Option not understood.\r\n");
//			}
//		}
//
//		else if (!_wcsicmp(szCmd, L"AVBL")) {
//			if (!*pszParam) {				
//				SocketSendString(sCmd, L"550 Syntax error in parameters or arguments.\r\n");
//			} else {
//				std::wstring mountPoint = std::wstring(pszParam);
//				if (DriveManager::IsMounted(mountPoint)) {
//					ULONGLONG freeSpace = DriveManager::GetFreeNumberOfBytes(mountPoint);
//					swprintf(szOutput, L"213 %llu\r\n", freeSpace);
//					SocketSendString(sCmd, szOutput);
//				} else {
//					swprintf(szOutput, L"550 \"%s\": not found.\r\n", mountPoint.c_str());
//					SocketSendString(sCmd, szOutput);
//				}
//			}
//		}
//
//		else {
//			swprintf(szOutput, L"500 Syntax error, command \"%s\" unrecognized.\r\n",szCmd);
//			SocketSendString(sCmd,szOutput);
//		}
//
//	}
//
//	SocketUtility::CloseSocket(sPasv);
//	SocketUtility::CloseSocket(sCmd);
//
//	if (isLoggedIn) {
//		DecrementConnections();
//	}
//
//	return false;
//}
//
//bool WINAPI FtpServer::ListenThread(LPVOID lParam)
//{
//	uint64_t sIncoming = 0;
//
//	while (SocketUtility::AcceptSocket(sListen, sIncoming)) {
//		if (mStopped) {
//			return false;
//		}
//		DWORD threadId = 0;
//		HANDLE thread = CreateThread(0, 0, (LPTHREAD_START_ROUTINE)ConnectionThread, (void*)sIncoming, 0, &threadId);
//		if (thread) {
//			SetThreadPriority(thread, 2);
//			CloseHandle(thread);
//		}
//	}
//
//	SocketUtility::CloseSocket(sListen);
//	return false;
//}
//
//#endif
//
//#ifndef MAC_ANGLE
//
//bool FtpServer::Init()
//{
//	mStopped = false;
//
//#if defined NEXGEN_OG || defined NEXGEN_360
//	XSetFileCacheSize(4*1024*1024);
//#endif
//
//	memset(&saiListen, 0, sizeof(SOCKADDR_IN));
//	saiListen.sin_family = AF_INET;
//	saiListen.sin_addr.S_un.S_addr = INADDR_ANY;
//	saiListen.sin_port = htons(21);
//
//	mMaxConnections = 20;
//	mCommandTimeout = 300;
//	mConnectTimeout = 15;
//
//	SocketUtility::CreateSocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, sListen);
//	SocketUtility::SetSocketOptions(sListen);
//
//	if (!SocketUtility::BindSocket(sListen, &saiListen)) {
//		SocketUtility::CloseSocket(sListen);
//		return false;
//	}
//
//	SocketUtility::ListenSocket(sListen, SOMAXCONN);
//
//	DWORD threadId = 0;
//	HANDLE thread = CreateThread(0, 0, (LPTHREAD_START_ROUTINE)ListenThread, 0, 0, &threadId);
//	if (thread) {
//		SetThreadPriority(thread, 2);
//		CloseHandle(thread);
//	}
//
//	return true;
//}
//
//#endif
//
//bool FtpServer::Close()
//{
//	mStopped = true;
//	return true;
//}
//
//bool FtpServer::SocketSendString(uint64_t s, const wchar_t *psz)
//{		
//	DebugUtility::LogMessage(DebugUtility::LOGLEVEL_INFO, L"SocketSendString = %s\n", psz);
//
//	int nWideSize = (int)wcslen(psz);
//	int nUtf8Size;
//	char* buf;
//	bool bSuccess = false;
//
//	nUtf8Size = WideCharToMultiByte(CP_UTF8, 0, psz, nWideSize, NULL, 0, NULL, NULL);
//	if (nUtf8Size == 0) return false;
//
//	buf = new char[nUtf8Size];
//	nUtf8Size = WideCharToMultiByte(CP_UTF8, 0, psz, nWideSize, buf, nUtf8Size, NULL, NULL);
//	if (nUtf8Size != 0) {
//		bSuccess = send((SOCKET)s, buf, nUtf8Size, 0) != SOCKET_ERROR;
//	}
//
//	delete[] buf;
//	return bSuccess;
//}
//
//FtpServer::ReceiveStatus FtpServer::SocketReceiveString(uint64_t s, wchar_t *psz, uint32_t dwMaxChars, uint32_t* pdwCharsReceived)
//{
//	uint32_t dwChars = 0;
//	ReceiveStatus status, statusError;
//	wchar_t buf[2];
//	uint32_t dw;
//
//	for (;;) {
//		if (dwChars == dwMaxChars) {
//			statusError = ReceiveStatus_Insufficient_Buffer;
//			break;
//		}
//
//		status = SocketReceiveLetter(s, psz, dwMaxChars - dwChars, &dw);
//		if (status == ReceiveStatus_OK) {
//			dwChars += dw;
//			if (*psz == L'\r') *psz = 0;
//			else if (*psz == L'\n') {
//				*psz = 0;
//				*pdwCharsReceived = dwChars;
//				return ReceiveStatus_OK;
//			}
//			psz += dw;
//		}
//		else if (status == ReceiveStatus_Invalid_Data || status == ReceiveStatus_Insufficient_Buffer) {
//			statusError = status;
//			break;
//		}
//		else {
//			return status;
//		}
//	}
//
//	// A non-critical error occurred, read until end of line
//	for (;;) {
//		status = SocketReceiveLetter(s, buf, sizeof(buf) / sizeof(wchar_t), &dw);
//		if (status == ReceiveStatus_OK) {
//			if (*buf == L'\n') {
//				return statusError;
//			}
//		}
//		else if (status == ReceiveStatus_Invalid_Data || status == ReceiveStatus_Insufficient_Buffer) {
//			// Go on...
//		}
//		else {
//			return status;
//		}
//	}
//}
//
//FtpServer::ReceiveStatus FtpServer::SocketReceiveLetter(uint64_t s, wchar_t* pch, uint32_t dwMaxChars, uint32_t* pdwCharsReceived)
//{
//	char buf[4];
//	uint32_t dwCharLength;
//	uint32_t dw;
//	TIMEVAL tv;
//	fd_set fds;
//
//	tv.tv_sec = mCommandTimeout;
//	tv.tv_usec = 0;
//	FD_ZERO(&fds);
//	FD_SET((SOCKET)s, &fds);
//	dw = select(0, &fds, 0, 0, &tv);
//	if (dw == SOCKET_ERROR || dw == 0) return ReceiveStatus_Timeout;
//	dw = recv((SOCKET)s, &buf[0], 1, 0);
//	if (dw == SOCKET_ERROR || dw == 0) return ReceiveStatus_Network_Error;
//
//	if ((buf[0] & 0x80) == 0x00) { // 0xxxxxxx
//		dwCharLength = 1;
//	}
//	else if ((buf[0] & 0xE0) == 0xC0) { // 110xxxxx
//		dwCharLength = 2;
//	}
//	else if ((buf[0] & 0xF0) == 0xE0) { // 1110xxxx
//		dwCharLength = 3;
//	}
//	else if ((buf[0] & 0xF8) == 0xF0) { // 11110xxx
//		dwCharLength = 4;
//	}
//	else {
//		return ReceiveStatus_Invalid_Data;
//	}
//
//	if (dwCharLength > 1) {
//		dw = recv((SOCKET)s, &buf[1], dwCharLength - 1, 0);
//		if (dw == SOCKET_ERROR || dw == 0) return ReceiveStatus_Network_Error;
//	}
//
//	if (dwMaxChars == 0) {
//		return ReceiveStatus_Insufficient_Buffer;
//	}
//
//	dw = MultiByteToWideChar(CP_UTF8, 0, buf, dwCharLength, pch, dwMaxChars);
//	if (dw == 0) {
//		if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
//			return ReceiveStatus_Insufficient_Buffer;
//		}
//		else {
//			return ReceiveStatus_Invalid_Data;
//		}
//	}
//
//	*pdwCharsReceived = dw;
//	return ReceiveStatus_OK;
//}
//
//FtpServer::ReceiveStatus FtpServer::SocketReceiveData(uint64_t s, char *psz, uint32_t dwBytesToRead, uint32_t* pdwBytesRead)
//{
//	uint32_t dw;
//	TIMEVAL tv;
//	fd_set fds;
//
//	tv.tv_sec = mConnectTimeout;
//	tv.tv_usec = 0;
//	memset(&fds, 0, sizeof(fd_set));	
//	FD_SET((SOCKET)s, &fds);
//	dw = select(0, &fds, 0, 0, &tv);
//	if (dw == SOCKET_ERROR || dw == 0)  {
//		DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, L"SocketReceiveData failed with Timeout.\n");
//		return ReceiveStatus_Timeout;
//	}
//	dw = recv((SOCKET)s, psz, dwBytesToRead, 0);
//	if (dw == SOCKET_ERROR) {
//		DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, L"SocketReceiveData failed with Network Error.\n");
//		return ReceiveStatus_Network_Error;
//	}
//	*pdwBytesRead = dw;
//	return ReceiveStatus_OK;
//}
//
//uint64_t FtpServer::EstablishDataConnection(sockaddr_in *psaiData, uint64_t*psPasv)
//{
//	DebugUtility::LogMessage(DebugUtility::LOGLEVEL_INFO, L"EstablishDataConnection\n");
//
//	uint64_t sData;
//	uint32_t dw;
//	TIMEVAL tv;
//	fd_set fds;
//
//	if (psPasv && *psPasv) {
//		tv.tv_sec = mConnectTimeout;
//		tv.tv_usec = 0;
//		memset(&fds, 0, sizeof(fd_set));
//		FD_SET((SOCKET)*psPasv,&fds);
//		dw = select(0,&fds,0,0,&tv);
//		if (dw && dw != SOCKET_ERROR) {
//			SocketUtility::AcceptSocket(*psPasv, psaiData, sData);
//		} else {
//			sData=0;
//		}
//		SocketUtility::CloseSocket(*psPasv);
//		return sData;
//	} else {
//		
//		SocketUtility::CreateSocket(AF_INET,SOCK_STREAM, IPPROTO_TCP, sData);
//		SocketUtility::SetSocketOptions(sData);
//		if (!SocketUtility::ConnectSocket(sData, psaiData)) {
//			SocketUtility::CloseSocket(sData);
//			return INVALID_SOCKET;
//		} else {
//			return sData;
//		}
//	}
//}
//
//bool FtpServer::ReceiveSocketFile(uint64_t sCmd, uint64_t sData, FileSystem::FileInfo &fileInfo)
//{
//	DebugUtility::LogMessage(DebugUtility::LOGLEVEL_INFO, L"ReceiveSocketFile\n");
//
//	int socketBufferSize = RECV_SOCKET_BUFFER_SIZE;
//	SocketUtility::SetSocketRecvSize(sData, socketBufferSize);
//
//	int driveSectorBufferSize = 128 * 1024;
//	int combinedBufferSize = socketBufferSize + driveSectorBufferSize;
//	char *combinedBuffer = (char*)malloc(combinedBufferSize);
//
//	bool fileComplete = false;
//
//	uint32_t totalBytesToWrite = 0;
//
//	while (true)
//	{
//
//		while (!fileComplete && totalBytesToWrite < (uint32_t)driveSectorBufferSize)
//		{
//			uint32_t bytesRead = 0;
//			if (SocketReceiveData(sData, combinedBuffer + totalBytesToWrite, socketBufferSize, &bytesRead) != ReceiveStatus_OK)
//			{
//				free(combinedBuffer);
//				return false;
//			}
//			totalBytesToWrite += bytesRead;
//			if (bytesRead == 0) {
//				fileComplete = true;
//				break;
//			}
//		}
//		
//		if (fileComplete && totalBytesToWrite == 0)
//		{
//			free(combinedBuffer);
//			return true;
//		}
//
//		uint32_t bytesWritten = 0;
//		uint32_t bytesToWrite = min((uint32_t)driveSectorBufferSize, totalBytesToWrite);
//		if (!FileSystem::FileWrite(fileInfo, combinedBuffer, bytesToWrite, bytesWritten))
//		{
//			DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, L"WriteFile failed: %i", GetLastError());
//			free(combinedBuffer);
//			return false;
//		}
//
//		totalBytesToWrite -= bytesWritten;
//		memmove(combinedBuffer, combinedBuffer + bytesWritten, totalBytesToWrite);
//		
//	}
//}
//
//bool FtpServer::SendSocketFile(uint64_t sCmd, uint64_t sData, FileSystem::FileInfo& fileInfo, uint32_t* pdwAbortFlag)
//{
//	DebugUtility::LogMessage(DebugUtility::LOGLEVEL_INFO, L"SendSocketFile\n");
//
//	int bufferSize = SEND_SOCKET_BUFFER_SIZE;
//	SocketUtility::SetSocketSendSize(sData, bufferSize);
//	char *szBuffer = (char*)malloc(bufferSize);
//
//	while (true)
//	{
//		uint32_t bytesRead;
//		if (!FileSystem::FileRead(fileInfo, szBuffer, bufferSize, bytesRead))
//		{
//			DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, L"ReadFile failed: %i", GetLastError());
//			free(szBuffer);
//			return false;
//		}
//
//		if (bytesRead == 0)
//		{
//			free(szBuffer);
//			return true;
//		}
//
//		int bytesToSend = bytesRead;
//		int bufferOffset = 0;
//
//		while (bytesToSend > 0)
//		{
//			int sent = send((SOCKET)sData, szBuffer + bufferOffset, bytesToSend, 0);
//			int queueLength = 0;
//			SocketUtility::GetReadQueueLength(sCmd, queueLength);
//			if (queueLength > 0) {
//				wchar_t szCmd[512];
//				uint32_t bytes_received = 0;
//				if (SocketReceiveString(sCmd, szCmd, 512, &bytes_received) == ReceiveStatus_OK) {
//					if (!_wcsicmp(szCmd, L"ABOR")) {
//						*pdwAbortFlag = 1;
//						free(szBuffer);
//						return false;
//					}
//					else {
//						SocketSendString(sCmd, L"500 Only command allowed at this time is ABOR.\r\n");
//					}
//				}
//			}
//			bytesToSend -= sent;
//			bufferOffset += (int)sent;
//			if (sent < 1)
//			{
//				free(szBuffer);
//				return false;
//			}
//		}
//	}
//}
