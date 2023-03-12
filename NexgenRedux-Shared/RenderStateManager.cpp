#include "RenderStateManager.h"

using namespace NexgenRedux;

namespace 
{
    RenderStateManager::RenderState m_renderState;
}

RenderStateManager::RenderState* RenderStateManager::GetRenderState()
{
    return &m_renderState;
}

void RenderStateManager::SetModelMatrix(const MathUtility::Matrix4x4& matrix) 
{ 
    bool isDirty = false;
    if (m_renderState.modelMatrixState.matrix != matrix)
    {
        m_renderState.modelMatrixState.matrix = matrix;
        isDirty = true;
    }
    m_renderState.modelMatrixState.isDirty = isDirty;
}

void RenderStateManager::SetViewMatrix(const MathUtility::Matrix4x4& matrix) 
{ 
    bool isDirty = false;
    if (m_renderState.viewMatrixState.matrix != matrix)
    {
        m_renderState.viewMatrixState.matrix = matrix;
        isDirty = true;
    }
    m_renderState.viewMatrixState.isDirty = isDirty;
}

void RenderStateManager::SetProjectionMatrix(const MathUtility::Matrix4x4& matrix)
{
    bool isDirty = false;
    if (m_renderState.projectionMatrixState.matrix != matrix)
    {
        m_renderState.projectionMatrixState.matrix = matrix;
        isDirty = true;
    }
    m_renderState.projectionMatrixState.isDirty = isDirty;
}

void RenderStateManager::SetAmbientLight(const MathUtility::Color3I& color) 
{ 
    bool isDirty = false;
    if (m_renderState.ambientLightState.color != color)
    {
        m_renderState.ambientLightState.color = color;
        isDirty = true;
    }
    m_renderState.ambientLightState.isDirty = isDirty;
}

void RenderStateManager::SetTexture(const uint32_t& textureID, const TextureFilter& filter)
{ 
    bool isDirty = false;
    if (m_renderState.textureState.textureId != textureID)
    {
        m_renderState.textureState.textureId = textureID;
        isDirty = true;
    }
    if (m_renderState.textureState.filter != filter)
    {
        m_renderState.textureState.filter = filter;
        isDirty = true;
    }
    m_renderState.textureState.isDirty = isDirty;
}

void RenderStateManager::SetTint(const MathUtility::Color4I& color)
{
    bool isDirty = false;
    if (m_renderState.tintState.color != color)
    {
        m_renderState.tintState.color = color;
        isDirty = true;
    }
    m_renderState.tintState.isDirty = isDirty;
}

void RenderStateManager::SetBlend(const BlendOperation& operation) 
{ 
    bool isDirty = false;
    if (m_renderState.blendState.operation != operation)
    {
        m_renderState.blendState.operation = operation;
        isDirty = true;
    }
    m_renderState.blendState.isDirty = isDirty;
}

void RenderStateManager::SetBlendFactors(const BlendFactor& srcFactor, const BlendFactor& dstFactor) 
{ 
    bool isDirty = false;
    if (m_renderState.blendFactorsState.srcFactor != srcFactor)
    {
        m_renderState.blendFactorsState.srcFactor = srcFactor;
        isDirty = true;
    }
    if (m_renderState.blendFactorsState.dstFactor != dstFactor)
    {
        m_renderState.blendFactorsState.dstFactor = dstFactor;
        isDirty = true;
    }
    m_renderState.blendFactorsState.isDirty = isDirty;
}

void RenderStateManager::SetCulling(const CullingOperation& operation) 
{ 
    bool isDirty = false;
    if (m_renderState.cullingState.operation != operation)
    {
        m_renderState.cullingState.operation = operation;
        isDirty = true;
    }
    m_renderState.cullingState.isDirty = isDirty;
}

void RenderStateManager::SetDepth(const DepthOperation& operation)
{ 
    bool isDirty = false;
    if (m_renderState.depthState.operation != operation)
    {
        m_renderState.depthState.operation = operation;
        isDirty = true;
    }
    m_renderState.depthState.isDirty = isDirty;
}

void RenderStateManager::SetLights(const LightsOperation& operation)
{
    bool isDirty = false;
    if (m_renderState.lightsState.operation != operation)
    {
        m_renderState.lightsState.operation = operation;
        isDirty = true;
    }
    m_renderState.lightsState.isDirty = isDirty;
}

void RenderStateManager::SetLight1(const LightOperation& operation) 
{ 
    bool isDirty = false;
    if (m_renderState.lightState1.operation != operation)
    {
        m_renderState.lightState1.operation = operation;
        isDirty = true;
    }
    m_renderState.lightState1.isDirty = isDirty;
}

void RenderStateManager::SetLightInstance1(const MathUtility::Vec3F& position, const float& distance, const MathUtility::Color4I& diffuse) 
{ 
    bool isDirty = false;
    if (m_renderState.lightInstanceState1.position != position)
    {
        m_renderState.lightInstanceState1.position = position;
        isDirty = true;
    }
    if (fabs(m_renderState.lightInstanceState1.distance - distance) > 1e-6f)
    {
        m_renderState.lightInstanceState1.distance = distance;
        isDirty = true;
    }
    if (m_renderState.lightInstanceState1.diffuse != diffuse)
    {
        m_renderState.lightInstanceState1.diffuse = diffuse;
        isDirty = true;
    }
    m_renderState.lightInstanceState1.isDirty = isDirty;
}

void RenderStateManager::SetLight2(const LightOperation& operation) 
{ 
    bool isDirty = false;
    if (m_renderState.lightState2.operation != operation)
    {
        m_renderState.lightState2.operation = operation;
        isDirty = true;
    }
    m_renderState.lightState2.isDirty = isDirty;
}

void RenderStateManager::SetLightInstance2(const MathUtility::Vec3F& position, const float& distance, const MathUtility::Color4I& diffuse) 
{ 
    bool isDirty = false;
    if (m_renderState.lightInstanceState2.position != position)
    {
        m_renderState.lightInstanceState2.position = position;
        isDirty = true;
    }
    if (fabs(m_renderState.lightInstanceState2.distance - distance) > 1e-6f)
    {
        m_renderState.lightInstanceState2.distance = distance;
        isDirty = true;
    }
    if (m_renderState.lightInstanceState2.diffuse != diffuse)
    {
        m_renderState.lightInstanceState2.diffuse = diffuse;
        isDirty = true;
    }
    m_renderState.lightInstanceState2.isDirty = isDirty;
}

void RenderStateManager::SetLight3(const LightOperation& operation) 
{ 
    bool isDirty = false;
    if (m_renderState.lightState3.operation != operation)
    {
        m_renderState.lightState3.operation = operation;
        isDirty = true;
    }
    m_renderState.lightState3.isDirty = isDirty;
}

void RenderStateManager::SetLightInstance3(const MathUtility::Vec3F& position, const float& distance, const MathUtility::Color4I& diffuse) 
{ 
    bool isDirty = false;
    if (m_renderState.lightInstanceState3.position != position)
    {
        m_renderState.lightInstanceState3.position = position;
        isDirty = true;
    }
    if (fabs(m_renderState.lightInstanceState3.distance - distance) > 1e-6f)
    {
        m_renderState.lightInstanceState3.distance = distance;
        isDirty = true;
    }
    if (m_renderState.lightInstanceState3.diffuse != diffuse)
    {
        m_renderState.lightInstanceState3.diffuse = diffuse;
        isDirty = true;
    }
    m_renderState.lightInstanceState3.isDirty = isDirty;
}

void RenderStateManager::SetLight4(const LightOperation& operation) 
{ 
    bool isDirty = false;
    if (m_renderState.lightState4.operation != operation)
    {
        m_renderState.lightState4.operation = operation;
        isDirty = true;
    }
    m_renderState.lightState4.isDirty = isDirty;
}

void RenderStateManager::SetLightInstance4(const MathUtility::Vec3F& position, const float& distance, const MathUtility::Color4I& diffuse) 
{ 
    bool isDirty = false;
    if (m_renderState.lightInstanceState4.position != position)
    {
        m_renderState.lightInstanceState4.position = position;
        isDirty = true;
    }
    if (fabs(m_renderState.lightInstanceState4.distance - distance) > 1e-6f)
    {
        m_renderState.lightInstanceState4.distance = distance;
        isDirty = true;
    }
    if (m_renderState.lightInstanceState4.diffuse != diffuse)
    {
        m_renderState.lightInstanceState4.diffuse = diffuse;
        isDirty = true;
    }
    m_renderState.lightInstanceState4.isDirty = isDirty;
}

void RenderStateManager::SetFog(const FogOperation& operation) 
{ 
    bool isDirty = false;
    if (m_renderState.fogState.operation != operation)
    {
        m_renderState.fogState.operation = operation;
        isDirty = true;
    }
    m_renderState.fogState.isDirty = isDirty;
}

void RenderStateManager::SetFogInstance(const MathUtility::Color3I& color, const float& start, const float& end, const float& density) 
{ 
    bool isDirty = false;
    if (m_renderState.fogInstanceState.color != color)
    {
        m_renderState.fogInstanceState.color = color;
        isDirty = true;
    }
    if (fabs(m_renderState.fogInstanceState.start - start) > 1e-6f)
    {
        m_renderState.fogInstanceState.start = start;
        isDirty = true;
    }
    if (fabs(m_renderState.fogInstanceState.end - end) > 1e-6f)
    {
        m_renderState.fogInstanceState.end = end;
        isDirty = true;
    }
    if (fabs(m_renderState.fogInstanceState.density - density) > 1e-6f)
    {
        m_renderState.fogInstanceState.density = density;
        isDirty = true;
    }
    m_renderState.fogInstanceState.isDirty = isDirty;
}

void RenderStateManager::SetViewport(const MathUtility::RectI rect)
{ 
    bool isDirty = false;
    if (m_renderState.viewportState.rect != rect)
    {
        m_renderState.viewportState.rect = rect;
        isDirty = true;
    }
    m_renderState.viewportState.isDirty = isDirty;
}

void RenderStateManager::SetScissor(const ScissorOperation& operation) 
{ 
    bool isDirty = false;
    if (m_renderState.scissorState.operation != operation)
    {
        m_renderState.scissorState.operation = operation;
        isDirty = true;
    }
    m_renderState.scissorState.isDirty = isDirty;
}

void RenderStateManager::SetScissorInstance(const MathUtility::RectI rect)
{
    bool isDirty = false;
    if (m_renderState.scissorInstanceState.rect != rect)
    {
        m_renderState.scissorInstanceState.rect = rect;
        isDirty = true;
    }
    m_renderState.scissorInstanceState.isDirty = isDirty;
}

void RenderStateManager::SetDrawMode(const DrawModeOperation& operation) 
{ 
    bool isDirty = false;
    if (m_renderState.drawModeState.operation != operation)
    {
        m_renderState.drawModeState.operation = operation;
        isDirty = true;
    }
    m_renderState.drawModeState.isDirty = isDirty;
}