#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 

#pragma once

#include "MeshUtility.h"
#include "IRenderingHelper.h"
#include "IWindowHelper.h"
#include "RenderStateManager.h"

#include <map>
#include <string>

namespace NexgenRedux
{
	class OpenGLRenderingHelper : public IRenderingHelper
	{		
	private:

		typedef struct TextureContainer
		{
			uint32_t texture;
			uint32_t width;
			uint32_t height;
		} TextureContainer;

		RenderStateManager* m_renderStateManager;
		IWindowHelper *m_windowHelper;

		bool m_initialized;
		std::map<std::string, std::map<std::string, uint32_t> > m_shaderValueMap;
		uint32_t m_dynamicBuffer;
		uint32_t m_dynamicBufferSize;
		uint32_t m_maxTextureContainerID;
		std::map<uint32_t, TextureContainer> m_textureContainerMap;

	public:

		OpenGLRenderingHelper(RenderStateManager* renderStateManager, IWindowHelper *windowHelper);
		~OpenGLRenderingHelper();

		bool Init() override;
		void SetShader(std::string shaderName) override;
		void SetModelMatrix(const MathUtility::Matrix4x4& matrix) override;
		void SetViewMatrix(const MathUtility::Matrix4x4& matrix) override;
		void SetProjectionMatrix(const MathUtility::Matrix4x4& matrix) override;
		void SetAmbientLight(const MathUtility::Color3F& color) override;
		void SetTexture(const uint32_t& textureID, const TextureFilter& filter) override;
		void SetTint(const MathUtility::Color4F& color) override;
		void SetBlend(const BlendOperation& operation) override;
		void SetBlendFactors(const BlendFactor& srcFactor, const BlendFactor& dstFactor) override;
		void SetCulling(const CullingOperation& operation) override;
		void SetDepth(const DepthOperation& operation) override;
		void SetLights(const LightsOperation& operation) override;
		void SetLight1(const LightOperation& operation) override;
		void SetLightInstance1(const MathUtility::Vec3F& position, const float& distance, const MathUtility::Color4F& diffuse) override;
		void SetLight2(const LightOperation& operation) override;
		void SetLightInstance2(const MathUtility::Vec3F& position, const float& distance, const MathUtility::Color4F& diffuse) override;
		void SetLight3(const LightOperation& operation) override;
		void SetLightInstance3(const MathUtility::Vec3F& position, const float& distance, const MathUtility::Color4F& diffuse) override;
		void SetLight4(const LightOperation& operation) override;
		void SetLightInstance4(const MathUtility::Vec3F& position, const float& distance, const MathUtility::Color4F& diffuse) override;
		void SetFog(const FogOperation& operation) override;
		void SetFogInstance(const MathUtility::Color3F& color, const float& start, const float& end, const float& density) override;
		void SetViewport(const MathUtility::RectI rect) override;
		void SetScissor(const ScissorOperation& operation, const MathUtility::RectI& rect) override;

		bool LoadTexture(std::wstring path, uint32_t& textureID) override;
		bool RenderMesh(uint32_t meshID) override;
		
	private:

		void ResizeDynamicBufferIfNeeded(uint32_t requestedSize);
		void CreateDynamicBuffer();
		void DeleteDynamicBuffer();
		TextureContainer* GetTextureContainer(uint32_t textureID);
		void DeleteTextures();
		uint32_t CompileShader(uint32_t type, const std::string& source);
		uint32_t CompileProgram(const std::string& vsSource, const std::string& fsSource);
		void CreateShaderLookup(std::string shaderName);
		bool AddShaderLookupKeyValue(std::string shaderName, std::string key, uint32_t value);
		bool GetShaderLookupValue(std::string shaderName, std::string key, uint32_t& value);
		void DeleteShaders();
	};
}

#endif