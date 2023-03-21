#pragma once

#include "GlobalTypes.h"
#include "MeshUtility.h"
#include "RenderStateManager.h"

#include <map>
#include <string>

namespace NexgenRedux
{
	class IRenderingHelper
	{		
	public:

		virtual ~IRenderingHelper() {}
		
		virtual bool Init() = 0;
		virtual void SetShader(std::string shaderName) = 0;
		virtual void SetModelMatrix(const MathUtility::Matrix4x4& matrix) = 0;
		virtual void SetViewMatrix(const MathUtility::Matrix4x4& matrix) = 0;
		virtual void SetProjectionMatrix(const MathUtility::Matrix4x4& matrix) = 0;
		virtual void SetAmbientLight(const MathUtility::Color3F& color) = 0;
		virtual void SetTexture(const uint32_t& textureID, const TextureFilter& filter) = 0;
		virtual void SetTint(const MathUtility::Color4F& color) = 0;
		virtual void SetBlend(const BlendOperation& operation) = 0;
		virtual void SetBlendFactors(const BlendFactor& srcFactor, const BlendFactor& dstFactor) = 0;
		virtual void SetCulling(const CullingOperation& operation) = 0;
		virtual void SetDepth(const DepthOperation& operation) = 0;
		virtual void SetLights(const LightsOperation& operation) = 0;
		virtual void SetLight1(const LightOperation& operation) = 0;
		virtual void SetLightInstance1(const MathUtility::Vec3F& position, const float& distance, const MathUtility::Color4F& diffuse) = 0;
		virtual void SetLight2(const LightOperation& operation) = 0;
		virtual void SetLightInstance2(const MathUtility::Vec3F& position, const float& distance, const MathUtility::Color4F& diffuse) = 0;
		virtual void SetLight3(const LightOperation& operation) = 0;
		virtual void SetLightInstance3(const MathUtility::Vec3F& position, const float& distance, const MathUtility::Color4F& diffuse) = 0;
		virtual void SetLight4(const LightOperation& operation) = 0;
		virtual void SetLightInstance4(const MathUtility::Vec3F& position, const float& distance, const MathUtility::Color4F& diffuse) = 0;
		virtual void SetFog(const FogOperation& operation) = 0;
		virtual void SetFogInstance(const MathUtility::Color3F& color, const float& start, const float& end, const float& density) = 0;
		virtual void SetViewport(const MathUtility::RectI rect) = 0;
		virtual void SetScissor(const ScissorOperation& operation, const MathUtility::RectI& rect) = 0;
		virtual bool LoadTexture(std::wstring path, uint32_t& textureID) = 0;
		virtual bool RenderMesh(uint32_t meshID) = 0;
	};
}
