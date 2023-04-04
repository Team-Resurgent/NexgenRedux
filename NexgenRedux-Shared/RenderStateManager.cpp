#include "RenderStateManager.h"
#include "OpenGLRenderingHelper.h"
#include "XboxOGRenderingHelper.h"
#include "IWindowHelper.h"
#include "IRenderingHelper.h"

#include <string>

using namespace NexgenRedux;

namespace
{
    RenderStateManager* m_instance = NULL;
}

RenderStateManager::RenderStateManager()
{
    m_initialized = false;
 #if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 
	m_renderingHelper = new OpenGLRenderingHelper(this);
#else
	m_renderingHelper = new XboxOGRenderingHelper(this);
#endif
}

RenderStateManager::~RenderStateManager()
{
    delete m_renderingHelper;
}

RenderStateManager* RenderStateManager::GetInstance()
{
	if (m_instance == NULL) 
	{
    	m_instance = new RenderStateManager();
    }
    return m_instance;
}

void RenderStateManager::Close()
{
	delete m_instance;
	m_instance = NULL;
}

void RenderStateManager::Init(void)
{
    SetShader("Default");
    SetModelMatrix(MathUtility::Matrix4x4());
    SetViewMatrix(MathUtility::Matrix4x4());
    SetProjectionMatrix(MathUtility::Matrix4x4());
    SetAmbientLight(MathUtility::Color3F(25 / 255.0f, 25 / 255.0f, 2 / 255.0f));
    SetTexture(0, TextureFilterLinear);
    SetTint(MathUtility::Color4F(1, 1, 1, 1));
    SetBlend(BlendOperationAdd);
    SetBlendFactors(BlendFactorSrcAlpha, BlendFactorOneMinusSrcAlpha);
    SetCulling(CullingOperationBack);
    SetDepth(DepthOperationLess);
    SetLights(LightsOperationDisabled);
    SetLight1(LightOperationDisabled);
    SetLightInstance1(MathUtility::Vec3F(0, 0, 0), 0, MathUtility::Color4F(0, 0, 0, 0));
    SetLight2(LightOperationDisabled);
    SetLightInstance2(MathUtility::Vec3F(0, 0, 0), 0, MathUtility::Color4F(0, 0, 0, 0));
    SetLight3(LightOperationDisabled);
    SetLightInstance3(MathUtility::Vec3F(0, 0, 0), 0, MathUtility::Color4F(0, 0, 0, 0));
    SetLight4(LightOperationDisabled);
    SetLightInstance4(MathUtility::Vec3F(0, 0, 0), 0, MathUtility::Color4F(0, 0, 0, 0));
    SetFog(FogOperationDisabled);
    SetFogInstance(MathUtility::Color3F(0, 0, 0), 0, 0, 0);
    SetViewport(MathUtility::RectI(0, 0, 640, 480));
    SetScissor(ScissorOperationDisabled, MathUtility::RectI());
    SetDrawMode(DrawModeOperationTriangles);

    m_renderingHelper->Init();
}

RenderState* RenderStateManager::GetRenderState(void)
{
    return &m_renderState;
}

bool RenderStateManager::CanBatch(void)
{
    if (m_renderState.shaderState.isDirty)
    {
        return false;
    }
    if (m_renderState.modelMatrixState.isDirty)
    {
        return false;
    }
    if (m_renderState.viewMatrixState.isDirty)
    {
        return false;
    }
    if (m_renderState.projectionMatrixState.isDirty)
    {
        return false;
    }
    if (m_renderState.ambientLightState.isDirty)
    {
        return false;
    }
    if (m_renderState.textureState.isDirty)
    {
        return false;
    }
    if (m_renderState.tintState.isDirty)
    {
        return false;
    }
    if (m_renderState.blendState.isDirty)
    {
        return false;
    }
    if (m_renderState.blendFactorsState.isDirty)
    {
        return false;
    }
    if (m_renderState.cullingState.isDirty)
    {
        return false;
    }
    if (m_renderState.depthState.isDirty)
    {
        return false;
    }
    if (m_renderState.lightsState.isDirty)
    {
        return false;
    }
    if (m_renderState.lightState1.isDirty)
    {
        return false;
    }
    if (m_renderState.lightInstanceState1.isDirty)
    {
        return false;
    }
    if (m_renderState.lightState2.isDirty)
    {
        return false;
    }
    if (m_renderState.lightInstanceState2.isDirty)
    {
        return false;
    }
    if (m_renderState.lightState3.isDirty)
    {
        return false;
    }
    if (m_renderState.lightInstanceState3.isDirty)
    {
        return false;
    }
    if (m_renderState.lightState4.isDirty)
    {
        return false;
    }
    if (m_renderState.lightInstanceState4.isDirty)
    {
        return false;
    }
    if (m_renderState.fogState.isDirty)
    {
        return false;
    }
    if (m_renderState.fogInstanceState.isDirty)
    {
        return false;
    }
    if (m_renderState.viewportState.isDirty)
    {
        return false;
    }
    if (m_renderState.scissorState.isDirty)
    {
        return false;
    }
    if (m_renderState.drawModeState.isDirty)
    {
        return false;
    }
    return true;
}

bool RenderStateManager::LoadTexture(const std::wstring& path, uint32_t& textureID)
{
    return m_renderingHelper->LoadTexture(path, textureID);
}

bool RenderStateManager::LoadOrReplaceTextureData(const uint8_t* data, const uint32_t& width, const uint32_t& height, uint32_t& textureID)
{
    return m_renderingHelper->LoadOrReplaceTextureData(data, width, height, textureID);
}

void RenderStateManager::DeleteTexture(const uint32_t& textureID)
{
    m_renderingHelper->DeleteTexture(textureID);
}

bool RenderStateManager::RenderMesh(const std::vector<MeshUtility::Vertex>& mesh)
{
	return m_renderingHelper->RenderMesh(mesh);
}

void RenderStateManager::Clear(const MathUtility::Color4F& color)
{
    m_renderingHelper->Clear(color);
}

void RenderStateManager::SetShader(const std::string& shader)
{
    bool isDirty = false;
    if (m_initialized == false || m_renderState.shaderState.shader != shader)
    {
        m_renderState.shaderState.shader = shader;
        isDirty = true;
    }
    m_renderState.shaderState.isDirty = isDirty;
}

void RenderStateManager::SetModelMatrix(const MathUtility::Matrix4x4& matrix) 
{ 
    bool isDirty = false;
    if (m_initialized == false || m_renderState.modelMatrixState.matrix != matrix)
    {
        m_renderState.modelMatrixState.matrix = matrix;
        isDirty = true;
    }
    m_renderState.modelMatrixState.isDirty = isDirty;
}

void RenderStateManager::SetViewMatrix(const MathUtility::Matrix4x4& matrix) 
{ 
    bool isDirty = false;
    if (m_initialized == false || m_renderState.viewMatrixState.matrix != matrix)
    {
        m_renderState.viewMatrixState.matrix = matrix;
        isDirty = true;
    }
    m_renderState.viewMatrixState.isDirty = isDirty;
}

void RenderStateManager::SetProjectionMatrix(const MathUtility::Matrix4x4& matrix)
{
    bool isDirty = false;
    if (m_initialized == false || m_renderState.projectionMatrixState.matrix != matrix)
    {
        m_renderState.projectionMatrixState.matrix = matrix;
        isDirty = true;
    }
    m_renderState.projectionMatrixState.isDirty = isDirty;
}

void RenderStateManager::SetAmbientLight(const MathUtility::Color3F& color) 
{ 
    bool isDirty = false;
    if (m_initialized == false || m_renderState.ambientLightState.color != color)
    {
        m_renderState.ambientLightState.color = color;
        isDirty = true;
    }
    m_renderState.ambientLightState.isDirty = isDirty;
}

void RenderStateManager::SetTexture(const uint32_t& textureID, const TextureFilter& filter)
{ 
    bool isDirty = false;
    if (m_initialized == false || m_renderState.textureState.textureId != textureID)
    {
        m_renderState.textureState.textureId = textureID;
        isDirty = true;
    }
    if (m_initialized == false || m_renderState.textureState.filter != filter)
    {
        m_renderState.textureState.filter = filter;
        isDirty = true;
    }
    m_renderState.textureState.isDirty = isDirty;
}

void RenderStateManager::SetTint(const MathUtility::Color4F& color)
{
    bool isDirty = false;
    if (m_initialized == false || m_renderState.tintState.color != color)
    {
        m_renderState.tintState.color = color;
        isDirty = true;
    }
    m_renderState.tintState.isDirty = isDirty;
}

void RenderStateManager::SetBlend(const BlendOperation& operation) 
{ 
    bool isDirty = false;
    if (m_initialized == false || m_renderState.blendState.operation != operation)
    {
        m_renderState.blendState.operation = operation;
        isDirty = true;
    }
    m_renderState.blendState.isDirty = isDirty;
}

void RenderStateManager::SetBlendFactors(const BlendFactor& srcFactor, const BlendFactor& dstFactor) 
{ 
    bool isDirty = false;
    if (m_initialized == false || m_renderState.blendFactorsState.srcFactor != srcFactor)
    {
        m_renderState.blendFactorsState.srcFactor = srcFactor;
        isDirty = true;
    }
    if (m_initialized == false || m_renderState.blendFactorsState.dstFactor != dstFactor)
    {
        m_renderState.blendFactorsState.dstFactor = dstFactor;
        isDirty = true;
    }
    m_renderState.blendFactorsState.isDirty = isDirty;
}

void RenderStateManager::SetCulling(const CullingOperation& operation) 
{ 
    bool isDirty = false;
    if (m_initialized == false || m_renderState.cullingState.operation != operation)
    {
        m_renderState.cullingState.operation = operation;
        isDirty = true;
    }
    m_renderState.cullingState.isDirty = isDirty;
}

void RenderStateManager::SetDepth(const DepthOperation& operation)
{ 
    bool isDirty = false;
    if (m_initialized == false || m_renderState.depthState.operation != operation)
    {
        m_renderState.depthState.operation = operation;
        isDirty = true;
    }
    m_renderState.depthState.isDirty = isDirty;
}

void RenderStateManager::SetLights(const LightsOperation& operation)
{
    bool isDirty = false;
    if (m_initialized == false || m_renderState.lightsState.operation != operation)
    {
        m_renderState.lightsState.operation = operation;
        isDirty = true;
    }
    m_renderState.lightsState.isDirty = isDirty;
}

void RenderStateManager::SetLight1(const LightOperation& operation) 
{ 
    bool isDirty = false;
    if (m_initialized == false || m_renderState.lightState1.operation != operation)
    {
        m_renderState.lightState1.operation = operation;
        isDirty = true;
    }
    m_renderState.lightState1.isDirty = isDirty;
}

void RenderStateManager::SetLightInstance1(const MathUtility::Vec3F& position, const float& distance, const MathUtility::Color4F& diffuse) 
{ 
    bool isDirty = false;
    if (m_initialized == false || m_renderState.lightInstanceState1.position != position)
    {
        m_renderState.lightInstanceState1.position = position;
        isDirty = true;
    }
    if (m_initialized == false || fabs(m_renderState.lightInstanceState1.distance - distance) > 1e-6f)
    {
        m_renderState.lightInstanceState1.distance = distance;
        isDirty = true;
    }
    if (m_initialized == false || m_renderState.lightInstanceState1.diffuse != diffuse)
    {
        m_renderState.lightInstanceState1.diffuse = diffuse;
        isDirty = true;
    }
    m_renderState.lightInstanceState1.isDirty = isDirty;
}

void RenderStateManager::SetLight2(const LightOperation& operation) 
{ 
    bool isDirty = false;
    if (m_initialized == false || m_renderState.lightState2.operation != operation)
    {
        m_renderState.lightState2.operation = operation;
        isDirty = true;
    }
    m_renderState.lightState2.isDirty = isDirty;
}

void RenderStateManager::SetLightInstance2(const MathUtility::Vec3F& position, const float& distance, const MathUtility::Color4F& diffuse) 
{ 
    bool isDirty = false;
    if (m_initialized == false || m_renderState.lightInstanceState2.position != position)
    {
        m_renderState.lightInstanceState2.position = position;
        isDirty = true;
    }
    if (m_initialized == false || fabs(m_renderState.lightInstanceState2.distance - distance) > 1e-6f)
    {
        m_renderState.lightInstanceState2.distance = distance;
        isDirty = true;
    }
    if (m_initialized == false || m_renderState.lightInstanceState2.diffuse != diffuse)
    {
        m_renderState.lightInstanceState2.diffuse = diffuse;
        isDirty = true;
    }
    m_renderState.lightInstanceState2.isDirty = isDirty;
}

void RenderStateManager::SetLight3(const LightOperation& operation) 
{ 
    bool isDirty = false;
    if (m_initialized == false || m_renderState.lightState3.operation != operation)
    {
        m_renderState.lightState3.operation = operation;
        isDirty = true;
    }
    m_renderState.lightState3.isDirty = isDirty;
}

void RenderStateManager::SetLightInstance3(const MathUtility::Vec3F& position, const float& distance, const MathUtility::Color4F& diffuse) 
{ 
    bool isDirty = false;
    if (m_initialized == false || m_renderState.lightInstanceState3.position != position)
    {
        m_renderState.lightInstanceState3.position = position;
        isDirty = true;
    }
    if (m_initialized == false || fabs(m_renderState.lightInstanceState3.distance - distance) > 1e-6f)
    {
        m_renderState.lightInstanceState3.distance = distance;
        isDirty = true;
    }
    if (m_initialized == false || m_renderState.lightInstanceState3.diffuse != diffuse)
    {
        m_renderState.lightInstanceState3.diffuse = diffuse;
        isDirty = true;
    }
    m_renderState.lightInstanceState3.isDirty = isDirty;
}

void RenderStateManager::SetLight4(const LightOperation& operation) 
{ 
    bool isDirty = false;
    if (m_initialized == false || m_renderState.lightState4.operation != operation)
    {
        m_renderState.lightState4.operation = operation;
        isDirty = true;
    }
    m_renderState.lightState4.isDirty = isDirty;
}

void RenderStateManager::SetLightInstance4(const MathUtility::Vec3F& position, const float& distance, const MathUtility::Color4F& diffuse) 
{ 
    bool isDirty = false;
    if (m_initialized == false || m_renderState.lightInstanceState4.position != position)
    {
        m_renderState.lightInstanceState4.position = position;
        isDirty = true;
    }
    if (m_initialized == false || fabs(m_renderState.lightInstanceState4.distance - distance) > 1e-6f)
    {
        m_renderState.lightInstanceState4.distance = distance;
        isDirty = true;
    }
    if (m_initialized == false || m_renderState.lightInstanceState4.diffuse != diffuse)
    {
        m_renderState.lightInstanceState4.diffuse = diffuse;
        isDirty = true;
    }
    m_renderState.lightInstanceState4.isDirty = isDirty;
}

void RenderStateManager::SetFog(const FogOperation& operation) 
{ 
    bool isDirty = false;
    if (m_initialized == false || m_renderState.fogState.operation != operation)
    {
        m_renderState.fogState.operation = operation;
        isDirty = true;
    }
    m_renderState.fogState.isDirty = isDirty;
}

void RenderStateManager::SetFogInstance(const MathUtility::Color3F& color, const float& start, const float& end, const float& density) 
{ 
    bool isDirty = false;
    if (m_initialized == false || m_renderState.fogInstanceState.color != color)
    {
        m_renderState.fogInstanceState.color = color;
        isDirty = true;
    }
    if (m_initialized == false || fabs(m_renderState.fogInstanceState.start - start) > 1e-6f)
    {
        m_renderState.fogInstanceState.start = start;
        isDirty = true;
    }
    if (m_initialized == false || fabs(m_renderState.fogInstanceState.end - end) > 1e-6f)
    {
        m_renderState.fogInstanceState.end = end;
        isDirty = true;
    }
    if (m_initialized == false || fabs(m_renderState.fogInstanceState.density - density) > 1e-6f)
    {
        m_renderState.fogInstanceState.density = density;
        isDirty = true;
    }
    m_renderState.fogInstanceState.isDirty = isDirty;
}

void RenderStateManager::SetViewport(const MathUtility::RectI rect)
{ 
    bool isDirty = false;
    if (m_initialized == false || m_renderState.viewportState.rect != rect)
    {
        m_renderState.viewportState.rect = rect;
        isDirty = true;
    }
    m_renderState.viewportState.isDirty = isDirty;
}

void RenderStateManager::SetScissor(const ScissorOperation& operation, const MathUtility::RectI rect) 
{ 
    bool isDirty = false;
    if (m_initialized == false || m_renderState.scissorState.operation != operation)
    {
        m_renderState.scissorState.operation = operation;
        isDirty = true;
    }
	if (m_initialized == false || m_renderState.scissorState.rect != rect)
    {
        m_renderState.scissorState.rect = rect;
        isDirty = true;
    }
    m_renderState.scissorState.isDirty = isDirty;
}

void RenderStateManager::SetDrawMode(const DrawModeOperation& operation) 
{ 
    bool isDirty = false;
    if (m_initialized == false || m_renderState.drawModeState.operation != operation)
    {
        m_renderState.drawModeState.operation = operation;
        isDirty = true;
    }
    m_renderState.drawModeState.isDirty = isDirty;
}

void RenderStateManager::ApplyChanges(void)
{
    if (m_renderState.shaderState.isDirty)
    {
        m_renderingHelper->SetShader(m_renderState.shaderState.shader);
        m_renderState.shaderState.isDirty = false;
    }
    if (m_renderState.modelMatrixState.isDirty)
    {
	    m_renderingHelper->SetModelMatrix(m_renderState.modelMatrixState.matrix);
        m_renderState.modelMatrixState.isDirty = false;
    }
    if (m_renderState.viewMatrixState.isDirty)
    {
	    m_renderingHelper->SetViewMatrix(m_renderState.viewMatrixState.matrix);
        m_renderState.viewMatrixState.isDirty = false;
    }
    if (m_renderState.projectionMatrixState.isDirty)
    {
        m_renderingHelper->SetProjectionMatrix(m_renderState.projectionMatrixState.matrix);
        m_renderState.projectionMatrixState.isDirty = false;
    }
    if (m_renderState.ambientLightState.isDirty)
    {
	    m_renderingHelper->SetAmbientLight(m_renderState.ambientLightState.color);
        m_renderState.ambientLightState.isDirty = false;
    }
    if (m_renderState.textureState.isDirty)
    {
	    m_renderingHelper->SetTexture(m_renderState.textureState.textureId, m_renderState.textureState.filter);
        m_renderState.textureState.isDirty = false;
    }
    if (m_renderState.tintState.isDirty)
    {
	    m_renderingHelper->SetTint(m_renderState.tintState.color);
        m_renderState.tintState.isDirty = false;
    }
    if (m_renderState.blendState.isDirty)
    {
	    m_renderingHelper->SetBlend(m_renderState.blendState.operation);
        m_renderState.blendState.isDirty = false;
    }
    if (m_renderState.blendFactorsState.isDirty)
    {
	    m_renderingHelper->SetBlendFactors(m_renderState.blendFactorsState.srcFactor, m_renderState.blendFactorsState.dstFactor);
        m_renderState.blendFactorsState.isDirty = false;
    }
    if (m_renderState.cullingState.isDirty)
    {
	    m_renderingHelper->SetCulling(m_renderState.cullingState.operation);
        m_renderState.cullingState.isDirty = false;
    }
    if (m_renderState.depthState.isDirty)
    {
	    m_renderingHelper->SetDepth(m_renderState.depthState.operation);
        m_renderState.depthState.isDirty = false;
    }
    if (m_renderState.lightsState.isDirty)
    {
	    m_renderingHelper->SetLights(m_renderState.lightsState.operation);
        m_renderState.lightsState.isDirty = false;
    }
    if (m_renderState.lightState1.isDirty)
    {
        m_renderingHelper->SetLight1(m_renderState.lightState1.operation);
        m_renderState.lightState1.isDirty = false;
    }
    if (m_renderState.lightInstanceState1.isDirty)
    {
	    m_renderingHelper->SetLightInstance1(m_renderState.lightInstanceState1.position, m_renderState.lightInstanceState1.distance, m_renderState.lightInstanceState1.diffuse);
        m_renderState.lightInstanceState1.isDirty = false;
    }
    if (m_renderState.lightState2.isDirty)
    {
        m_renderingHelper->SetLight2(m_renderState.lightState2.operation);
        m_renderState.lightState2.isDirty = false;
    }
    if (m_renderState.lightInstanceState2.isDirty)
    {
        m_renderingHelper->SetLightInstance2(m_renderState.lightInstanceState2.position, m_renderState.lightInstanceState2.distance, m_renderState.lightInstanceState2.diffuse);
        m_renderState.lightInstanceState2.isDirty = false;
    }
    if (m_renderState.lightState3.isDirty)
    {
	    m_renderingHelper->SetLight3(m_renderState.lightState3.operation);
        m_renderState.lightState3.isDirty = false;
    }
    if (m_renderState.lightInstanceState3.isDirty)
    {
	    m_renderingHelper->SetLightInstance3(m_renderState.lightInstanceState3.position, m_renderState.lightInstanceState3.distance, m_renderState.lightInstanceState3.diffuse);
        m_renderState.lightInstanceState3.isDirty = false;
    }
    if (m_renderState.lightState4.isDirty)
    {
	    m_renderingHelper->SetLight4(m_renderState.lightState4.operation);
        m_renderState.lightState4.isDirty = false;
    }
    if (m_renderState.lightInstanceState4.isDirty)
    {
	    m_renderingHelper->SetLightInstance4(m_renderState.lightInstanceState4.position, m_renderState.lightInstanceState4.distance, m_renderState.lightInstanceState4.diffuse);
        m_renderState.lightInstanceState4.isDirty = false;
    }
    if (m_renderState.fogState.isDirty)
    {
	    m_renderingHelper->SetFog(m_renderState.fogState.operation);
        m_renderState.fogState.isDirty = false;
    }
    if (m_renderState.fogInstanceState.isDirty)
    {
	    m_renderingHelper->SetFogInstance(m_renderState.fogInstanceState.color, m_renderState.fogInstanceState.start, m_renderState.fogInstanceState.end, m_renderState.fogInstanceState.density);
        m_renderState.fogInstanceState.isDirty = false;
    }
    if (m_renderState.viewportState.isDirty)
    {
	    m_renderingHelper->SetViewport(m_renderState.viewportState.rect);
        m_renderState.viewportState.isDirty = false;
    }
    if (m_renderState.scissorState.isDirty)
    {
	    m_renderingHelper->SetScissor(m_renderState.scissorState.operation, m_renderState.scissorState.rect);
        m_renderState.scissorState.isDirty = false;
    }
    if (m_renderState.drawModeState.isDirty)
    {
        m_renderState.drawModeState.isDirty = false;
    }

    m_initialized = true;
}
