#include "Sprite.h"
#include "RenderStateManager.h"
#include "TextureManager.h"

#include <Gensys/DebugUtility.h>
#include <Gensys/StringUtility.h>
#include <stdexcept>
#include <string>

using namespace Gensys;
using namespace NexgenRedux;

Sprite::Sprite(uint32_t nodeID) : Node(nodeID)
{
    m_visible = true;

    m_textureIsDirty = true;
    m_texturePath = "";
    m_textureID = 0;

    m_wantsTexture = false;


    m_meshIsDirty = true;
    m_uv = MathUtility::RectF(0, 0, 1, 1);
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
        renderStateManager->DeleteTextureReference(m_textureID);
    }
}

void Sprite::Update(float dt)
{
    RenderStateManager* renderStateManager = RenderStateManager::GetInstance();

    if (m_textureIsDirty == true)
    {
        // TODO share this code as will be used elsewhere
        if (m_textureID > 0)
        {
            renderStateManager->DeleteTextureReference(m_textureID);
        }
        bool textureExists = renderStateManager->TextureExists(StringUtility::ToWideString(m_texturePath), m_textureID);
        renderStateManager->CreateTextureReference(StringUtility::ToWideString(m_texturePath), m_textureID);
        if (textureExists == false)
        {
            TextureManager::Request(StringUtility::ToWideString(m_texturePath), m_textureID);
        }
        m_wantsTexture = true;
        m_textureIsDirty = false;
    }

    if (m_wantsTexture == true && renderStateManager->IsTextureLoaded(m_textureID) == true)
    {
        m_meshIsDirty = true;
        m_wantsTexture = false;   
    }

    if (m_meshIsDirty == true)
    {
		MathUtility::Vec2F maxUV;
		if (renderStateManager->GetTexureMaxUV(m_textureID, maxUV) == true)
		{
			MathUtility::RectF uvRect;
			uvRect.x = m_uv.x * maxUV.x;
			uvRect.y = m_uv.y * maxUV.y;
			uvRect.width = m_uv.width * maxUV.x;
			uvRect.height = m_uv.height * maxUV.y;
			m_mesh = MeshUtility::CreateQuadXY(MathUtility::Vec3F(0, 0, 0), m_size, uvRect);
		}
        m_meshIsDirty = false;
    }
}

void Sprite::Render()
{
	RenderStateManager* renderStateManager = RenderStateManager::GetInstance();

    if (m_visible == false || m_textureID == 0 || m_mesh.size() == 0 || renderStateManager->IsTextureLoaded(m_textureID) == false) 
    {
        return;
    }

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

const bool Sprite::GetVisible()
{
    return m_visible;
}

void Sprite::SetVisible(const bool value)
{
    m_visible = value;
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
	m_meshIsDirty = true;
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
    MathUtility::Vec2F screenCoord = MathUtility::Vec2F((float)screenPosX, (float)screenPosY);
    return camera->TestRayIntersectsObb(screenCoord, MathUtility::Vec3F(), MathUtility::Vec3F(m_size.width, m_size.height, 0.0f), GetTransform());
}