#pragma once

#include "MathUtility.h"

#include "IRenderingHelper.h"
#include "IWindowHelper.h"

#include <Gensys/Int.h>

#include <string>
#include <vector>

namespace NexgenRedux
{
	class IRenderingHelper;
	class RenderStateManager
	{		
	public:
		
		static RenderStateManager* GetInstance();
		static void Close();

		void Init(void);

		RenderState* GetRenderState(void);
		bool CanBatch(void);
		bool LoadTexture(std::wstring path, uint32_t& textureID);
		void DeleteTexture(const uint32_t& textureID);
		bool RenderMesh(uint32_t meshID);
		void Clear(const MathUtility::Color4F color);

		void SetShader(const std::string& shader);
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
		void SetScissor(const ScissorOperation& operation, const MathUtility::RectI rect);
		void SetDrawMode(const DrawModeOperation& operation);

		void ApplyChanges(void);

	private:

		RenderStateManager();
		~RenderStateManager();

	private:

		IRenderingHelper* m_renderingHelper;
		bool m_initialized;
    	RenderState m_renderState;
	};
}