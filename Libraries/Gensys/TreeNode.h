#pragma once

#if defined UWP_ANGLE || defined NEXGEN_WIN
#include <WinSock2.h>
#include <Windows.h>
#elif defined NEXGEN_360 || defined NEXGEN_OG
#include <xtl.h>
#include <winsockx.h>
#endif	
#include <string>

namespace Gensys
{
	class TreeNode
	{
	public:
		TreeNode *m_up;
		TreeNode *m_left;
		TreeNode *m_right;
		TreeNode *m_down;
		std::wstring virtualPath;
		std::wstring localPath;

		TreeNode();
		TreeNode(TreeNode *parent);
		~TreeNode();
	};
}