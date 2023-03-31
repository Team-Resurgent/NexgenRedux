#include "Sprite.h"
#include "RenderStateManager.h"

#include <Gensys/DebugUtility.h>
#include <Gensys/StringUtility.h>
#include <stdexcept>
#include <string>

using namespace Gensys;
using namespace NexgenRedux;

Sprite::Sprite(uint32_t nodeID) : Node(nodeID)
{
    m_textureIsDirty = true;
    m_texturePath = "";
    m_textureID = 0;

    m_meshIsDirty = true;
    m_uv = MathUtility::RectF(0, 0, 1, 1);
    m_position = MathUtility::Vec3F(0, 0, 0);
    m_size = MathUtility::SizeF(1, 1);

    m_tint = MathUtility::Color4F(1, 1, 1, 1);
    m_blendOperation = BlendOperationAdd;
    m_blendFactorSrc = BlendFactorSrcAlpha;
    m_blendFactorDst = BlendFactorOneMinusSrcAlpha;
    m_drawModeOperation = DrawModeOperationTriangles;
    m_cullingOperation = CullingOperationBack;
    m_depthOperation = DepthOperationLess;
}

Sprite::~Sprite(void)
{
    RenderStateManager* renderStateManager = RenderStateManager::GetInstance();
    if (m_textureID != 0)
    {
        renderStateManager->DeleteTexture(m_textureID);
    }
}

void Sprite::Update(float dt)
{
    RenderStateManager* renderStateManager = RenderStateManager::GetInstance();

    if (m_textureIsDirty == true)
    {
        if (m_textureID != 0)
        {
            renderStateManager->DeleteTexture(m_textureID);
        }
        renderStateManager->LoadTexture(StringUtility::ToWideString(m_texturePath), m_textureID);
        m_textureIsDirty = false;
    }

    if (m_meshIsDirty == true)
    {
        m_mesh = MeshUtility::CreateQuadXY(m_position, m_size, m_uv);
        m_meshIsDirty = false;
    }
}

void Sprite::Render()
{
    if (m_textureID == 0 || m_mesh.size() == 0) 
    {
        return;
    }

    RenderStateManager* renderStateManager = RenderStateManager::GetInstance();

    renderStateManager->SetModelMatrix(GetTransform());
	renderStateManager->SetTexture(m_textureID, TextureFilterLinear);

    renderStateManager->SetTint(m_tint);
    renderStateManager->SetBlend(m_blendOperation);
    renderStateManager->SetBlendFactors(m_blendFactorSrc, m_blendFactorDst);
    renderStateManager->SetDrawMode(m_drawModeOperation);
    renderStateManager->SetCulling(m_cullingOperation);
    renderStateManager->SetDepth(m_depthOperation);

    renderStateManager->ApplyChanges();
    renderStateManager->RenderMesh(m_mesh);
}

const std::string Sprite::GetTexturePath()
{
    return m_texturePath;
}

void Sprite::SetTexturePath(const std::string value)
{
    if (m_texturePath == value) 
    {
        return;
    }
    m_texturePath = value;
    m_textureIsDirty = true;
}

const MathUtility::RectF Sprite::GetUV()
{
    return m_uv;
}

void Sprite::SetUV(const MathUtility::RectF value)
{
    if (m_uv == value) 
    {
        return;
    }
    m_uv = value;
    m_meshIsDirty = true;
}

const MathUtility::SizeF Sprite::GetSize()
{
    return m_size;
}

void Sprite::SetSize(const MathUtility::SizeF value)
{
    if (m_size == value) 
    {
        return;
    }
    m_size = value;
    m_meshIsDirty = true;
}

const MathUtility::Color4F Sprite::GetTint()
{
    return m_tint;
}

void Sprite::SetTint(const MathUtility::Color4F value)
{
    m_tint = value;
}

const BlendOperation Sprite::GetBlend()
{
    return m_blendOperation;
}

void Sprite::SetBlend(const BlendOperation value)
{
    m_blendOperation = value;
}

const BlendFactor Sprite::GetBlendFactorSrc()
{
    return m_blendFactorSrc;
}

void Sprite::SetBlendFactorSrc(const BlendFactor value)
{
    m_blendFactorSrc = value;
}

const BlendFactor Sprite::GetBlendFactorDst()
{
    return m_blendFactorDst;
}

void Sprite::SetBlendFactorDst(const BlendFactor value)
{
    m_blendFactorDst = value;
}

const DrawModeOperation Sprite::GetDrawMode()
{
    return m_drawModeOperation;
}

void Sprite::SetDrawMode(const DrawModeOperation value)
{
    m_drawModeOperation = value;
}

const CullingOperation Sprite::GetCulling()
{
    return m_cullingOperation;
}

void Sprite::SetCulling(const CullingOperation value)
{
    m_cullingOperation = value;
}

const DepthOperation Sprite::GetDepth()
{
    return m_depthOperation;
}

void Sprite::SetDepth(const DepthOperation value)
{
    m_depthOperation = value;
}

bool Sprite::HitTest(const double screenPosX, const double screenPosY, OrthoCamera* camera)
{
    MathUtility::RectF rect = MathUtility::RectF(0, 0, m_size.width, m_size.height);
    return camera->IsScreenPointInRect((float)screenPosX, (float)screenPosY, GetInverseTransform(), rect);
}