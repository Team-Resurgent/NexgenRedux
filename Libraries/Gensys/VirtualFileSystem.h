//#pragma once
//
//#include "FileSystem.h"
//#include "TreeNode.h"
//
//#if defined UWP_ANGLE || defined WIN_ANGLE
//#include <WinSock2.h>
//#include <Windows.h>
//#elif defined XBOX_360 || defined XBOX_OG
//#include <xtl.h>
//#include <winsockx.h>
//#endif	
//
//#include <map>
//#include <string>
//
//using namespace Gensys;
//
//class VirtualFileSystem
//{
//public:
//
//	typedef struct FindData {
//		std::wstring virtualPath;
//		std::wstring fileSpec;
//		TreeNode* treeNode;
//	} FindData;
//	
//	typedef std::map<std::wstring, std::wstring> ListingType;
//	
//	VirtualFileSystem();
//	void Mount(const wchar_t *virtualPath, const wchar_t *localPath);
//	void Unmount(const wchar_t *virtualPath);
//	bool GetDirectoryListing(const wchar_t *virtualPath, DWORD isNLST, ListingType &listing);
//	bool FileExists(const wchar_t *virtualPath);
//	bool IsFolder(const wchar_t *virtualPath);
//	bool FindFirst(const wchar_t* virtualPath, FindData &findData);
//	bool FindNext(FindData &findData);
//	void FindClose(FindData &findData);
//    bool FileCreate(const wchar_t* virtualPath, FileSystem::FileAccessMode fileAccessMode, FileSystem::FileCreateMode fileCreateMode, FileSystem::FileInfo& fileInfo);
//	bool FileDelete(const wchar_t *virtualPath);
//	bool FileMove(const wchar_t *oldVirtual, const wchar_t *newVirtual);
//	bool DirectoryCreate(const wchar_t *virtualPath);
//	bool DirectoryDelete(const wchar_t *virtualPath);
//	void ResolveRelative(const wchar_t *currentVirtual, const wchar_t *relativeVirtual, std::wstring &newVirtual);
//private:
//	TreeNode m_root;
//	void CleanVirtualPath(const wchar_t* virtualPath, std::wstring &newVirtual);
//	bool Map(const wchar_t *virtualPath, std::wstring &localPath, TreeNode *treeNode);
//	TreeNode* FindMountPoint(const wchar_t *virtualPath, TreeNode *treeNode);
//	bool WildcardMatch(const wchar_t *filespec, const wchar_t *filename);
//	void GetMountPointFindData(TreeNode *treeNode, FileSystem::FindInfo& findInfo);
//};
