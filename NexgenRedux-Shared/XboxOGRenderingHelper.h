#if defined NEXGEN_OG

#pragma once

#include <Gensys/Int.h>

#include <string>

namespace NexgenRedux
{
	class XboxOGRenderingHelper
	{		
	public:
		static void Close(void);

		static bool Init();
		static bool SetShader(std::string shaderName);
		static bool LoadTexture(std::wstring path, uint32_t& textureID);
	};
}

#endif