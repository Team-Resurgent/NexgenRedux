#pragma once

#include "MathUtility.h"

#include <Gensys/Int.h>
#include <string>

namespace NexgenRedux
{
    typedef struct MonitorVideoMode
	{
		uint32_t monitorIndex;
		uint32_t width;
		uint32_t height; 
		uint32_t redBits;
		uint32_t greenBits;
		uint32_t blueBits;
		uint32_t refreshRate;
	} MonitorVideoMode;

    typedef enum JoystickButtonState {
		JoystickButtonStateNone,
		JoystickButtonStatePressed
	} JoystickButtonState;

	typedef struct JoystickButtonStates
	{
		JoystickButtonState buttonA;
		JoystickButtonState buttonB;
		JoystickButtonState buttonX;
		JoystickButtonState buttonY; 
		JoystickButtonState buttonLeftBumperOrWhite;
		JoystickButtonState buttonRightBumperOrBlack;
		JoystickButtonState buttonBack;
		JoystickButtonState buttonStart;
		JoystickButtonState buttonGuide;
		JoystickButtonState buttonLeftThumb;
		JoystickButtonState buttonRightThumb;
		JoystickButtonState buttonDpadUp;
		JoystickButtonState buttonDpadRight;
		JoystickButtonState buttonDpadDown;
		JoystickButtonState buttonDpadLeft;
	} JoystickButtonStates;

    typedef enum KeyButtonState {
		KeyButtonStateNone,
		KeyButtonStatePressed
	} KeyButtonState;

    typedef enum MousekButtonState {
		MouseButtonStateNone,
		MouseButtonStatePressed
	} MouseButtonState;

	typedef struct JoystickAxisStates
	{
		float axisLeftX;
		float axisLeftY;
		float axisRightX;
		float axisRightY; 
		float axisLeftTrigger;
		float axisRightTrigger;
	} JoystickAxisStates;

    typedef enum DrawModeOperation {
		DrawModeOperationTriangles,
		DrawModeOperationLines
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
		MathUtility::Color3F color;
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
		MathUtility::Color4F color;
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
		MathUtility::Color4F diffuse;
		bool isDirty;
	} LightInstanceState;

	typedef struct FogState
	{
		FogOperation operation;
		bool isDirty;
	} FogState;

	typedef struct FogInstanceState
	{
		MathUtility::Color3F color;
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

	typedef enum CursorMode {
		CursorModeNormal,
		CursorModeHidden,
		CursorModeDisabled,
		CursorModeCaptured
	} CursorMode;  

	typedef enum FontStyle {
		FontStyleRegular = 0,
		FontStyleBold = 1,
		FontStyleItalic = 2,
		FontStyleUserDefined1 = 4,
		FontStyleUserDefined2 = 8,
		FontStyleUnderline = 16,
		FontStyleStrikeThrough = 32,
	} FontStyle;  
}
