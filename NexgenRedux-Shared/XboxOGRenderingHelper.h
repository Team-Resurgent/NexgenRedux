#if defined NEXGEN_OG

#pragma once

#include "RenderStateManager.h"
#include "IRenderingHelper.h"

#include <Gensys/Int.h>

#include <xtl.h>
#include <string>

namespace NexgenRedux
{
	class XboxOGRenderingHelper : public IRenderingHelper
	{		
	private:

		typedef struct TextureContainer
		{
			IDirect3DTexture8* texture;
			uint32_t width;
			uint32_t height;
			std::wstring key;
			uint32_t refCount;
		} TextureContainer;

		RenderStateManager* m_renderStateManager;

		bool m_initialized;
		D3DVertexBuffer* m_dynamicBuffer;
		uint32_t m_dynamicBufferSize;

		uint32_t m_maxTextureContainerID;
		std::map<uint32_t, XboxOGRenderingHelper::TextureContainer> m_textureContainerMap;

	public:

		XboxOGRenderingHelper(RenderStateManager* renderStateManager);
		~XboxOGRenderingHelper();

		bool Init();
		void SetShader(std::string shaderName);
		void SetModelMatrix(const MathUtility::Matrix4x4& matrix);
		void SetViewMatrix(const MathUtility::Matrix4x4& matrix);
		void SetProjectionMatrix(const MathUtility::Matrix4x4& matrix);
		void SetAmbientLight(const MathUtility::Color3F& color);
		void SetTexture(const uint32_t& textureID, const TextureFilter& filter);
		void SetTint(const MathUtility::Color4F& color);
		void SetBlend(const BlendOperation& operation);
		void SetBlendFactors(const BlendFactor& srcFactor, const BlendFactor& dstFactor);
		void SetCulling(const CullingOperation& operation);
		void SetDepth(const DepthOperation& operation);
		void SetLights(const LightsOperation& operation);
		void SetLight1(const LightOperation& operation);
		void SetLightInstance1(const MathUtility::Vec3F& position, const float& distance, const MathUtility::Color4F& diffuse);
		void SetLight2(const LightOperation& operation);
		void SetLightInstance2(const MathUtility::Vec3F& position, const float& distance, const MathUtility::Color4F& diffuse);
		void SetLight3(const LightOperation& operation);
		void SetLightInstance3(const MathUtility::Vec3F& position, const float& distance, const MathUtility::Color4F& diffuse);
		void SetLight4(const LightOperation& operation);
		void SetLightInstance4(const MathUtility::Vec3F& position, const float& distance, const MathUtility::Color4F& diffuse);
		void SetFog(const FogOperation& operation);
		void SetFogInstance(const MathUtility::Color3F& color, const float& start, const float& end, const float& density);
		void SetViewport(const MathUtility::RectI rect);
		void SetScissor(const ScissorOperation& operation, const MathUtility::RectI& rect);

		bool LoadTexture(const std::wstring& path, uint32_t& textureID);
		bool LoadOrReplaceTextureData(const uint8_t* data, const uint32_t& width, const uint32_t& height, uint32_t& textureID);
		void DeleteTexture(const uint32_t& textureID);
		bool RenderMesh(const std::vector<MeshUtility::Vertex>& mesh);
		void Clear(const MathUtility::Color4F& color);

	private:

		void ResizeDynamicBufferIfNeeded(uint32_t requestedSize);
		void CreateDynamicBuffer();
		void DeleteDynamicBuffer();
		TextureContainer* GetTextureContainer(uint32_t textureID);
		void DeleteTextures();
	};
}

#endif