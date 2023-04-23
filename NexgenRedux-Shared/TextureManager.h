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
			void* instance;
            void* callback;
		} TextureRequest;

        typedef void (*TextureLoadedCallback)(void* instance, const uint32_t& width, const uint32_t& height, uint8_t* data);

		static void Init();
		static void Close();
        static void TestThread();
        static void Request(const std::wstring& path, void* instance, TextureLoadedCallback textureLoadedCallback);
        static void FreeData(uint8_t* data);

	};
}
