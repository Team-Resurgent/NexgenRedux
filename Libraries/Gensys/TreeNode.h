#pragma once

#if defined UWP_ANGLE || defined WIN_ANGLE
#include <WinSock2.h>
#include <Windows.h>
#elif defined XBOX_360 || defined XBOX_OG
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