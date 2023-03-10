#pragma once

#include "MathUtility.h"

namespace NexgenRedux
{
	class RenderingManager
	{		
	public:

		typedef struct ModelMatrixState
		{
			MathUtility::Matrix4x4 matrix;
		} ModelMatrixState;

		typedef struct ViewMatrixState
		{
			MathUtility::Matrix4x4 matrix;
		} ViewMatrixState;

		typedef struct ProjectionMatrixState
		{
			MathUtility::Matrix4x4 matrix;
		} ProjectionMatrixState;

		typedef struct AmbientLightState
		{
			MathUtility::Color3 color;
		} AmbientLightState;

		typedef struct TextureState
		{
			int textureId;
			int filter;
		} TextureState;

		typedef struct TintState
		{
			MathUtility::Color4 color;
		} TintState;

		typedef struct MeshInstance
		{
			int meshCollectionId;
			int meshId;
		} MeshInstance;

		typedef struct BlendState
		{
			bool enabled;
			int operation;
			int sfactor;
			int dfactor;
		} BlendState;

		typedef struct CullingState
		{
			int operation;
		} CullingState;

		typedef struct DepthState
		{
			bool enabled;
			int operation;
		} DepthState;

		typedef struct LightsState
		{
			bool enabled;
		} LightsState;

		typedef struct LightState
		{
			bool enabled;
			MathUtility::Vec3F position;
			float distance;
			MathUtility::Color4 diffuseColor;
		} LightState;

		typedef struct FogState
		{
			bool enabled;
			int operation;
			MathUtility::Color3 color;
			float start;
			float end;
			float density;
		} FogState;

		typedef struct ViewportState
		{
			MathUtility::Rect rect;
		} ViewportState;

		typedef struct ScissorState
		{
			bool enabled;
			MathUtility::Rect rect;
		} ScissorState;

		typedef struct DrawModeState
		{
			int operation;
		} DrawModeState;

		typedef struct RenderState
		{
			ModelMatrixState modelMatrixState;
			ViewMatrixState viewMatrixState;
			ProjectionMatrixState projectionMatrixState;
			AmbientLightState ambientLightState;
			TextureState textureState;
			MeshInstance meshInstance;
			TintState tintState;
			BlendState blendState;	
			CullingState cullingState;
			DepthState depthState;
			LightsState lightsState;
			LightState lightState1;
			LightState lightState2;
			LightState lightState3;
			LightState lightState4;
			FogState fogState;
			ViewportState viewportState;
			ScissorState scissorState;
			DrawModeState drawModeState;
		} RenderState;

		static bool Init(uint32_t windowHandle);

	private:
		RenderState m_currentRenderState;
		RenderState m_previousRenderState;
	};
}