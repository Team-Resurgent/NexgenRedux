#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 

#pragma once

#include "MeshUtility.h"
#include "RenderStateManager.h"

#include <map>
#include <string>

namespace NexgenRedux
{
	class OpenGLRenderingHelper
	{		
	public:

		typedef struct TextureContainer
		{
			uint32_t texture;
			uint32_t width;
			uint32_t height;
		} TextureContainer;

		static void Close(void);

		static bool Init();
		static void SetShader(std::string shaderName);
		static void SetModelMatrix(const MathUtility::Matrix4x4& matrix);
		static void SetViewMatrix(const MathUtility::Matrix4x4& matrix);
		static void SetProjectionMatrix(const MathUtility::Matrix4x4& matrix);
		static void SetAmbientLight(const MathUtility::Color3I& color);
		static void SetTexture(const uint32_t& textureID, const RenderStateManager::TextureFilter& filter);
		static void SetTint(const MathUtility::Color4I& color);
		static void SetBlend(const RenderStateManager::BlendOperation& operation);
		static void SetBlendFactors(const RenderStateManager::BlendFactor& srcFactor, const RenderStateManager::BlendFactor& dstFactor);
		static void SetCulling(const RenderStateManager::CullingOperation& operation);
		static void SetDepth(const RenderStateManager::DepthOperation& operation);
		static void SetLights(const RenderStateManager::LightsOperation& operation);
		static void SetLight1(const RenderStateManager::LightOperation& operation);
		static void SetLightInstance1(const MathUtility::Vec3F& position, const float& distance, const MathUtility::Color4I& diffuse);
		static void SetLight2(const RenderStateManager::LightOperation& operation);
		static void SetLightInstance2(const MathUtility::Vec3F& position, const float& distance, const MathUtility::Color4I& diffuse);
		static void SetLight3(const RenderStateManager::LightOperation& operation);
		static void SetLightInstance3(const MathUtility::Vec3F& position, const float& distance, const MathUtility::Color4I& diffuse);
		static void SetLight4(const RenderStateManager::LightOperation& operation);
		static void SetLightInstance4(const MathUtility::Vec3F& position, const float& distance, const MathUtility::Color4I& diffuse);
		static void SetFog(const RenderStateManager::FogOperation& operation);
		static void SetFogInstance(const MathUtility::Color3I& color, const float& start, const float& end, const float& density);
		static void SetViewport(const MathUtility::RectI rect);
		static void SetScissor(const RenderStateManager::ScissorOperation& operation, const MathUtility::RectI& rect);

		static bool LoadTexture(std::wstring path, uint32_t& textureID);
		static bool RenderMesh(uint32_t meshID);
	private:

		static void ResizeDynamicBufferIfNeeded(uint32_t requestedSize);
		static void CreateDynamicBuffer();
		static void DeleteDynamicBuffer();

		static OpenGLRenderingHelper::TextureContainer* GetTextureContainer(uint32_t textureID);
		static void DeleteTextures();

		static void CreateShaderLookup(std::string shaderName);
		static bool AddShaderLookupKeyValue(std::string shaderName, std::string key, uint32_t value);
		static bool GetShaderLookupValue(std::string shaderName, std::string key, uint32_t& value);
		static void DeleteShaders();
	};
}

#endif