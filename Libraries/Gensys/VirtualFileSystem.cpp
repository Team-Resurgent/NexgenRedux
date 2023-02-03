//#include "VirtualFileSystem.h"
//#include "DriveManager.h"
//#include "StringUtility.h"
//#include "FileSystem.h"
//#include <algorithm>
//#include <vector>
//#include <time.h>
//#include "TreeNode.h"
//#include "TimeUtility.h"
//
//using namespace Gensys;
//
//VirtualFileSystem::VirtualFileSystem()
//{
//	this->Mount(L"/", L"");
//	std::vector<std::wstring> mountPoints = DriveManager::GetAllMountPoints();
//	for (size_t i = 0; i < mountPoints.size(); i++)
//	{
//		this->Mount((L"/" + mountPoints.at(i)).c_str(),  (mountPoints.at(i) + L":").c_str());
//	}
//}
//
//void VirtualFileSystem::Mount(const wchar_t *virtualPath, const wchar_t *localPath)
//{
//	TreeNode *treeNode, *parent;
//
//	treeNode = &m_root;
//	size_t i = 0;
//	std::wstring dir;
//	while (virtualPath[i] != 0) {
//		++i;
//		if (virtualPath[i] == 0) break;
//		size_t j = wcscspn(virtualPath + i, L"/");
//		dir.assign(virtualPath + i, j);
//		parent = treeNode;
//		treeNode = treeNode->m_down;
//		while (treeNode && _wcsicmp(treeNode->virtualPath.c_str(), dir.c_str())) treeNode = treeNode->m_right;
//		if (!treeNode) {
//			treeNode = new TreeNode(parent);
//			treeNode->virtualPath = dir;
//		}
//		i += j;
//	}
//	treeNode->localPath = localPath;
//}
//
//void VirtualFileSystem::Unmount(const wchar_t *virtualPath)
//{
//	
//}
//
//bool VirtualFileSystem::GetDirectoryListing(const wchar_t *virtualPath, DWORD isNLST, ListingType &listing)
//{
//	DriveManager::Refresh();
//
//	wchar_t szLine[512];
//	const wchar_t* pszMonthAbbr[] = { L"Jan",L"Feb",L"Mar",L"Apr",L"May",L"Jun",L"Jul",L"Aug",L"Sep",L"Oct",L"Nov",L"Dec" };
//	FindData findData;
//
//	if (IsFolder(virtualPath)) {
//		std::wstring str;
//		ResolveRelative(virtualPath, L"*", str);
//		if (!FindFirst(str.c_str(), findData))
//		{
//			return false;
//		}
//	}
//	else {
//		if (!FindFirst(virtualPath, findData))
//		{
//			return false;
//		}
//	}
//	do {
//		FileSystem::FindInfoDetails findInfoDetails;
//		if (!FileSystem::FileFindInfoDetails(findData.findInfo, findInfoDetails))
//		{
//			continue;
//		}
//		if (!wcscmp(findInfoDetails.name.c_str(), L".") || !wcscmp(findInfoDetails.name.c_str(), L"..")) {
//			continue;
//		}
//		if (isNLST) {
//			wcscpy(szLine, findInfoDetails.name.c_str());
//			if (findInfoDetails.isDir) {
//				wcscat(szLine, L"/");
//			}
//		} else {
//
//			tm timeDetail = TimeUtility::GetTimeInfo(findInfoDetails.lastWriteTime);
//			tm nowTimeDetail = TimeUtility::GetNow();
//			
//			swprintf(szLine, L"%c--------- 1 ftp ftp %llu %s %2i ", findInfoDetails.isDir ? L'd' : L'-', findInfoDetails.fileSize, pszMonthAbbr[timeDetail.tm_mon], timeDetail.tm_mday);
//			if (nowTimeDetail.tm_year == timeDetail.tm_year) {
//				swprintf(szLine + wcslen(szLine), L"%.2i:%.2i ", timeDetail.tm_hour, timeDetail.tm_min);
//			} else {
//				swprintf(szLine + wcslen(szLine), L"%5u ", 1900 + timeDetail.tm_year);
//			}
//			wcscat(szLine, findInfoDetails.name.c_str());
//		}
//		wcscat(szLine, L"\r\n");
//		ListingType::iterator it = listing.find(findInfoDetails.name.c_str());
//		if (it != listing.end()) {
//			it->second = szLine;
//		} else {
//			listing.insert(std::make_pair(findInfoDetails.name.c_str(), szLine));
//		}
//	} while (FindNext(findData));
//	FindClose(findData);
//	return true;
//}
//
//bool VirtualFileSystem::FileExists(const wchar_t *virtualPath)
//{	
//	FindData findData;
//	if (FindFirst(virtualPath, findData)) {
//		FindClose(findData);
//		return true;
//	} else {
//		return false;
//	}
//}
//
//bool VirtualFileSystem::IsFolder(const wchar_t *virtualPath)
//{
//	std::wstring strLocal;
//	DWORD dw;
//
//	if (FindMountPoint(virtualPath, &m_root)) {
//		return true;
//	}
//	if (!Map(virtualPath, strLocal, &m_root)) {
//		return true;
//	}
//	dw = GetFileAttributesA(StringUtility::ToString(strLocal).c_str());
//	return ((dw != -1) && (dw & FILE_ATTRIBUTE_DIRECTORY));
//}
//
//bool VirtualFileSystem::FindFirst(const wchar_t* virtualPath, FindData& findData)
//{
//
//	const wchar_t* psz;
//	std::wstring str;
//
//	psz = wcsrchr(virtualPath, L'/');
//	if (psz == NULL) return NULL;
//	str.assign(virtualPath, psz);
//
//	findData.findInfo.dir = NULL;
//	findData.virtualPath = virtualPath;
//	findData.fileSpec = psz + 1;
//	findData.treeNode = FindMountPoint(str.c_str(), &m_root);
//	if (findData.treeNode) {
//		findData.treeNode = findData.treeNode->m_down;
//	}
//
//	return FindNext(findData);
//}
//
//bool VirtualFileSystem::FindNext(FindData &findData)
//{
//	std::wstring str;
//
//	while (findData.treeNode) {
//		str = findData.treeNode->virtualPath;
//
//#if defined XBOX_OG || defined XBOX_360
//		if (findData.virtualPath == L"/*" && !DriveManager::IsMounted(str)) {
//			findData.treeNode = findData.treeNode->m_right;
//			continue;
//		}
//#endif
//		
//		if (str.find_first_of(L'.') == std::wstring::npos) {
//			str.push_back(L'.');
//		}
//		
//		if (WildcardMatch(findData.fileSpec.c_str(), str.c_str())) {
//			GetMountPointFindData(findData.treeNode, findData.findInfo);
//			findData.treeNode = findData.treeNode->m_right;
//			return true;
//		}
//		findData.treeNode = findData.treeNode->m_right;
//	}
//
//	if (findData.findInfo.dir != NULL) {
//		return FileSystem::FileFindNext(findData.findInfo);
//	} else {
//		if (!Map(findData.virtualPath.c_str(), str, &m_root)) {
//			return false;
//		}
//		if (str.length() != 0) {
//			return FileSystem::FileFindFirst(str, findData.findInfo);
//		} else {
//			return false;
//		}
//	}
//}
//
//void VirtualFileSystem::FindClose(FindData &findData)
//{
//	FileSystem::FileFindClose(findData.findInfo);
//}
//
//bool VirtualFileSystem::FileCreate(const wchar_t *virtualPath, FileSystem::FileAccessMode fileAccessMode, FileSystem::FileCreateMode fileCreateMode, FileSystem::FileInfo &fileInfo)
//{
//	std::wstring strLocal;
//	if (Map(virtualPath, strLocal, &m_root)) {
//		return FileSystem::FileCreate(strLocal, fileAccessMode, fileCreateMode, fileInfo);
//	} 
//	return false;
//}
//
//bool VirtualFileSystem::FileDelete(const wchar_t *virtualPath)
//{
//	std::wstring strLocal;
//	return (
//		Map(virtualPath, strLocal, &m_root) &&
//		FileSystem::FileDelete(strLocal)
//	);
//}
//
//bool VirtualFileSystem::FileMove(const wchar_t *pszOldVirtual, const wchar_t *pszNewVirtual)
//{
//	std::wstring strOldLocal;
//	std::wstring strNewLocal;
//	return (
//		Map(pszOldVirtual, strOldLocal, &m_root) &&
//		Map(pszNewVirtual, strNewLocal, &m_root) &&
//		FileSystem::FileMove(strOldLocal, strNewLocal)
//	);
//}
//
//bool VirtualFileSystem::DirectoryCreate(const wchar_t *virtualPath)
//{
//	std::wstring strLocal;
//	return (
//		Map(virtualPath, strLocal, &m_root) &&
//		FileSystem::DirectoryCreate(strLocal)
//	);
//}
//
//bool VirtualFileSystem::DirectoryDelete(const wchar_t *virtualPath)
//{
//	std::wstring strLocal;
//	return (
//		Map(virtualPath, strLocal, &m_root) &&
//		FileSystem::DirectoryDelete(strLocal, false)
//	);
//}
//
//void VirtualFileSystem::ResolveRelative(const wchar_t* currentVirtual, const wchar_t* relativeVirtual, std::wstring& newVirtual)
//{
//	if (*relativeVirtual != L'/') {
//		newVirtual = currentVirtual;
//		newVirtual += L"/";
//		newVirtual += relativeVirtual;
//		CleanVirtualPath(newVirtual.c_str(), newVirtual);
//	}
//	else {
//		CleanVirtualPath(relativeVirtual, newVirtual);
//	}
//}
//
//void VirtualFileSystem::CleanVirtualPath(const wchar_t* virtualPath, std::wstring& newVirtual)
//{
//	const wchar_t* in = virtualPath;
//	wchar_t* buf = new wchar_t[wcslen(virtualPath) + 4];
//	buf[0] = L'\0';
//	buf[1] = L'\0';
//	buf[2] = L'\0';
//	wchar_t* out = buf + 3;
//	do {
//		*out = *in;
//		if (*out == L'\\') *out = L'/'; // convert backslashes to forward slashes
//		if ((*out == L'\0') || (*out == L'/')) {
//			if (out[-1] == L'.') { // output ends with "."
//				if (out[-2] == L'\0') --out; // entire output is "."
//				else if (out[-2] == L'/') { // output ends with "/."
//					if (out[-3] == L'\0') --out; // entire output is "/."
//					else out -= 2;
//				}
//				else if (out[-2] == L'.') { // output ends with ".."
//					if (out[-3] == L'\0') out -= 2; // entire output is ".."
//					else if (out[-3] == L'/') { // output ends with "/.."
//						if (out[-4] == L'\0') out -= 2; // entire output is "/.."
//						else {
//							out -= 3;
//							while ((out[-1] != L'\0') && (out[-1] != L'/')) --out;
//						}
//					}
//				}
//				else ++in;
//			}
//			else {
//				++in;
//				if (out[-1] != L'/') ++out;
//			}
//		}
//		else ++in, ++out;
//	} while (in[-1] != L'\0');
//	newVirtual = buf + 3;
//	delete[] buf;
//}
//
//bool VirtualFileSystem::Map(const wchar_t* virtualPath, std::wstring& localPath, TreeNode* treeNode)
//{
//	const wchar_t* psz;
//	UINT_PTR dwLen;
//
//	psz = wcschr(virtualPath, L'/');
//	if (psz) dwLen = psz - virtualPath;
//	else dwLen = wcslen(virtualPath);
//	while (treeNode) {
//		if ((treeNode->virtualPath.length() == dwLen) && (!dwLen || !_wcsnicmp(virtualPath, treeNode->virtualPath.c_str(), dwLen))) {
//			if (psz) {
//				if (Map(psz + 1, localPath, treeNode->m_down)) return 1;
//				else {
//					if (treeNode->localPath.length() != 0) {
//						localPath = treeNode->localPath;
//						localPath += psz;
//						replace(localPath.begin(), localPath.end(), '/', '\\');
//						return true;
//					}
//					else {
//						return false;
//					}
//				}
//			}
//			else {
//				localPath = treeNode->localPath;
//				return true;
//			}
//		}
//		else {
//			treeNode = treeNode->m_right;
//		}
//	}
//	localPath.clear();
//	return false;
//}
//
//TreeNode* VirtualFileSystem::FindMountPoint(const wchar_t* virtualPath, TreeNode* treeNode)
//{
//	const wchar_t* psz;
//	UINT_PTR dwLen;
//
//	if (!wcscmp(virtualPath, L"/")) {
//		return treeNode;
//	}
//	psz = wcschr(virtualPath, L'/');
//	if (psz) {
//		dwLen = psz - virtualPath;
//	}
//	else {
//		dwLen = wcslen(virtualPath);
//	}
//	while (treeNode) {
//		if ((treeNode->virtualPath.length() == dwLen) && (!dwLen || !_wcsnicmp(virtualPath, treeNode->virtualPath.c_str(), dwLen))) {
//			if (psz) {
//				return FindMountPoint(psz + 1, treeNode->m_down);
//			}
//			else {
//				return treeNode;
//			}
//		}
//		else {
//			treeNode = treeNode->m_right;
//		}
//	}
//	return 0;
//}
//
//bool VirtualFileSystem::WildcardMatch(const wchar_t *filespec, const wchar_t* filename)
//{
//	if (*filespec == 0) {
//		return true;
//	}
//	while (*filespec) {
//		if (*filespec == L'*') {
//			filespec++;
//			do {
//				if (WildcardMatch(filespec, filename)) return true;
//			} while (*filename++);
//			return false;
//		}
//		else if (((*filespec | 0x20) != (*filename | 0x20)) && (*filespec != L'?')) {
//			return false;
//		}
//		filespec++;
//		filename++;
//	}
//	if (!*filespec && !*filename) {
//		return true;
//	}
//	else {
//		return false;
//	}
//}
//
//void VirtualFileSystem::GetMountPointFindData(TreeNode* treeNode, FileSystem::FindInfo& findInfo)
//{
//	HANDLE hFind = INVALID_HANDLE_VALUE;
//	SYSTEMTIME st = { 1980, 1, 2, 1, 0, 0, 0, 0 };
//
//	FileSystem::FileFindFirst(treeNode->localPath, findInfo);
//
//	if ((treeNode->localPath.length() != 0) && (FileSystem::FileFindFirst(treeNode->localPath, findInfo))) {
//		FileSystem::FileFindClose(findInfo);
//	}
//	else {
//		memset(&findInfo, 0, sizeof(FileSystem::FindInfo));
//	}
//}
