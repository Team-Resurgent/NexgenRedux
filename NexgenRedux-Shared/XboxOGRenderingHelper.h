#if defined NEXGEN_OG

#pragma once

#include <Gensys/Int.h>

#include <xtl.h>
#include <string>

namespace NexgenRedux
{
	class XboxOGRenderingHelper
	{		
	public:

		typedef struct TextureContainer
		{
			IDirect3DTexture8* texture;
			uint32_t width;
			uint32_t height;
		} TextureContainer;

		static void Close(void);

		static bool Init();
		static bool SetShader(std::string shaderName);
		static bool LoadTexture(std::wstring path, uint32_t& textureID);
		static bool RenderMesh(uint32_t meshID);
	private:

		static void ResizeDynamicBufferIfNeeded(uint32_t requestedSize);
		static void CreateDynamicBuffer();
		static void DeleteDynamicBuffer();

		static XboxOGRenderingHelper::TextureContainer* GetTextureContainer(uint32_t textureID);
		static void DeleteTextures();
	};
}

#endif