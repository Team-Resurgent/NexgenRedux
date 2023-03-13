#pragma once

#include "MathUtility.h"

#include <Gensys/Int.h>

#include <string>
#include <vector>

namespace NexgenRedux
{
	class RenderStateManager
	{		
	public:

		typedef enum DrawModeOperation {
			DrawModeTriangles,
			DrawModeLines
		} DrawModeOperation;

		typedef enum TextureFilter {
			TextureFilterLinear,
			TextureFilterNearest
		} TextureFilter;

		typedef enum BlendOperation {
			BlendOperationDisabled,
			BlendOperationAdd,
			BlendOperationSubtract,
			BlendOperationReverseSubtract
		} BlendOperation;

		typedef enum BlendFactor {
			BlendFactorZero,
			BlendFactorOne,
			BlendFactorSrcColor,
			BlendFactorOneMinusSrcColor,
			BlendFactorDstColor,
			BlendFactorOneMinusDstColor,
			BlendFactorSrcAlpha,
			BlendFactorOneMinusSrcAlpha,
			BlendFactorDstAlpha,
			BlendFactorOneMinusDstAlpha,
			BlendFactorConstantColor,
			BlendFactorOneMinusConstantColor,
			BlendFactorConstantAlpha,
			BlendFactorOneMinusConstantAlpha,
			BlendFactorSrcAlphaSaturate
		} BlendFactor;

		typedef enum CullingOperation {
			CullingOperationDisabled,
			CullingOperationFront,
			CullingOperationBack
		} CullingOperation;

		typedef enum DepthOperation {
			DepthOperationDisabled,
			DepthOperationNever,
			DepthOperationLess,
			DepthOperationEqual,
			DepthOperationLessOrEqual,
			DepthOperationGreater,
			DepthOperationNotEqual,
			DepthOperationGreaterOrEqual,
			DepthOperationAlways
		} DepthOperation;

		typedef enum LightsOperation {
			LightsOperationDisabled,
			LightsOperationEnabled
		} LightsOperation;

		typedef enum LightOperation {
			LightOperationDisabled,
			LightOperationEnabled
		} LightOperation;

		typedef enum FogOperation {
			FogOperationDisabled,
			FogOperationExponent,
			FogOperationExponent2,
			FogOperationLinear
		} FogOperation;

		typedef enum ScissorOperation {
			ScissorOperationDisabled,
			ScissorOperationEnabled
		} ScissorOperation;

		typedef struct ShaderState
		{
			std::string shader;
			bool isDirty;
		} ShaderState;

		typedef struct ModelMatrixState
		{
			MathUtility::Matrix4x4 matrix;
			bool isDirty;
		} ModelMatrixState;

		typedef struct ViewMatrixState
		{
			MathUtility::Matrix4x4 matrix;
			bool isDirty;
		} ViewMatrixState;

		typedef struct ProjectionMatrixState
		{
			MathUtility::Matrix4x4 matrix;
			bool isDirty;
		} ProjectionMatrixState;

		typedef struct AmbientLightState
		{
			MathUtility::Color3I color;
			bool isDirty;
		} AmbientLightState;

		typedef struct TextureState
		{
			uint32_t textureId;
			TextureFilter filter;
			bool isDirty;
		} TextureState;

		typedef struct TintState
		{
			MathUtility::Color4I color;
			bool isDirty;
		} TintState;

		typedef struct BlendState
		{
			BlendOperation operation;
			bool isDirty;
		} BlendState;

		typedef struct BlendFactorsState
		{
			BlendFactor srcFactor;
			BlendFactor dstFactor;
			bool isDirty;
		} BlendFactorsState;

		typedef struct CullingState
		{
			CullingOperation operation;
			bool isDirty;
		} CullingState;

		typedef struct DepthState
		{
			DepthOperation operation;
			bool isDirty;
		} DepthState;

		typedef struct LightsState
		{
			LightsOperation operation;
			bool isDirty;
		} LightsState;

		typedef struct LightState
		{
			LightOperation operation;
			bool isDirty;
		} LightState;

		typedef struct LightInstanceState
		{
			MathUtility::Vec3F position;
			float distance;
			MathUtility::Color4I diffuse;
			bool isDirty;
		} LightInstanceState;

		typedef struct FogState
		{
			FogOperation operation;
			bool isDirty;
		} FogState;

		typedef struct FogInstanceState
		{
			MathUtility::Color3I color;
			float start;
			float end;
			float density;
			bool isDirty;
		} FogInstanceState;

		typedef struct ViewportState
		{
			MathUtility::RectI rect;
			bool isDirty;
		} ViewportState;

		typedef struct ScissorState
		{
			ScissorOperation operation;
			MathUtility::RectI rect;
			bool isDirty;
		} ScissorState;

		typedef struct DrawModeState
		{
			DrawModeOperation operation;
			bool isDirty;
		} DrawModeState;

		typedef struct RenderState
		{
			ShaderState shaderState;
			ModelMatrixState modelMatrixState;
			ViewMatrixState viewMatrixState;
			ProjectionMatrixState projectionMatrixState;
			AmbientLightState ambientLightState;
			TextureState textureState;
			TintState tintState;
			BlendState blendState;	
			BlendFactorsState blendFactorsState;	
			CullingState cullingState;
			DepthState depthState;
			LightsState lightsState;
			LightState lightState1;
			LightInstanceState lightInstanceState1;
			LightState lightState2;
			LightInstanceState lightInstanceState2;
			LightState lightState3;
			LightInstanceState lightInstanceState3;
			LightState lightState4;
			LightInstanceState lightInstanceState4;
			FogState fogState;
			FogInstanceState fogInstanceState;
			ViewportState viewportState;
			ScissorState scissorState;
			DrawModeState drawModeState;
		} RenderState;

		static void Init(void);
		static void Close(void);

		static RenderState* GetRenderState(void);
		static bool CanBatch(void);

		static void SetShader(const std::string& shader);
		static void SetModelMatrix(const MathUtility::Matrix4x4& matrix);
		static void SetViewMatrix(const MathUtility::Matrix4x4& matrix);
		static void SetProjectionMatrix(const MathUtility::Matrix4x4& matrix);
		static void SetAmbientLight(const MathUtility::Color3I& color);
		static void SetTexture(const uint32_t& textureID, const TextureFilter& filter);
		static void SetTint(const MathUtility::Color4I& color);
		static void SetBlend(const BlendOperation& operation);
		static void SetBlendFactors(const BlendFactor& srcFactor, const BlendFactor& dstFactor);
		static void SetCulling(const CullingOperation& operation);
		static void SetDepth(const DepthOperation& operation);
		static void SetLights(const LightsOperation& operation);
		static void SetLight1(const LightOperation& operation);
		static void SetLightInstance1(const MathUtility::Vec3F& position, const float& distance, const MathUtility::Color4I& diffuse);
		static void SetLight2(const LightOperation& operation);
		static void SetLightInstance2(const MathUtility::Vec3F& position, const float& distance, const MathUtility::Color4I& diffuse);
		static void SetLight3(const LightOperation& operation);
		static void SetLightInstance3(const MathUtility::Vec3F& position, const float& distance, const MathUtility::Color4I& diffuse);
		static void SetLight4(const LightOperation& operation);
		static void SetLightInstance4(const MathUtility::Vec3F& position, const float& distance, const MathUtility::Color4I& diffuse);
		static void SetFog(const FogOperation& operation);
		static void SetFogInstance(const MathUtility::Color3I& color, const float& start, const float& end, const float& density);
		static void SetViewport(const MathUtility::RectI rect);
		static void SetScissor(const ScissorOperation& operation, const MathUtility::RectI rect);
		static void SetDrawMode(const DrawModeOperation& operation);

		static void ApplyChanges(void);
	};
}
