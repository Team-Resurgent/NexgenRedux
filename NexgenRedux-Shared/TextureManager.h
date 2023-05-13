#pragma once

#include <Gensys/Int.h>

#include <string>
#include <map>

namespace NexgenRedux
{
	class TextureManager
	{		
	public:

    	typedef struct TextureRequest
		{
            std::wstring path;
			bool processed;
			uint32_t textureID;
			uint8_t* data;
			int width;
			int height;
		} TextureRequest;

		static void Init();
		static void Close();
        static void ProcessThread();
        static void Request(const std::wstring& path, uint32_t textureID);
		static void ApplyTextures();

	};
}
