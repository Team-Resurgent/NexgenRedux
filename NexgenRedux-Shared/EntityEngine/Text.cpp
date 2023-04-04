#include "Text.h"
#include "FontManager.h"
#include "RenderStateManager.h"
#include "ConfigLoader.h"

#include <Gensys/FileSystem.h>
#include <Gensys/DebugUtility.h>
#include <Gensys/StringUtility.h>
#include <stdexcept>
#include <string>

using namespace Gensys;
using namespace NexgenRedux;

Text::Text(uint32_t nodeID) : Node(nodeID)
{
    m_textIsDirty = true;
    m_text = "";
    m_fontName = "";
    m_fontStyle = 0;
    m_fontSize = 10;
    m_textSize = MathUtility::SizeF();
    m_textureID = 0;
    m_tint = MathUtility::Color4F(1, 1, 1, 1);
    m_blendOperation = BlendOperationAdd;
    m_blendFactorSrc = BlendFactorSrcAlpha;
    m_blendFactorDst = BlendFactorOneMinusSrcAlpha;
    m_drawModeOperation = DrawModeOperationTriangles;
    m_cullingOperation = CullingOperationBack;
    m_depthOperation = DepthOperationLess;
}

Text::~Text(void)
{
    RenderStateManager* renderStateManager = RenderStateManager::GetInstance();
    if (m_textureID != 0)
    {
        renderStateManager->DeleteTexture(m_textureID);
    }
}

void Text::Update(float dt)
{
    RenderStateManager* renderStateManager = RenderStateManager::GetInstance();

    if (m_textIsDirty == true)
    {
        if (FontManager::SelectFont(m_fontName, m_fontStyle, m_fontSize) == true)
        {
            void* textHandle;
            if (FontManager::RenderText(m_text, &textHandle) == true)
            {
                uint8_t* pixelBuffer;
                uint32_t width;
                uint32_t height;
                FontManager::GetTextPixelBuffer(textHandle, &pixelBuffer, width, height);

                renderStateManager->LoadOrReplaceTextureData(pixelBuffer, width, height, m_textureID);

                m_textSize = MathUtility::SizeF((float)width, (float)height);
                m_mesh = MeshUtility::CreateQuadXY(MathUtility::Vec3F(0, 0, 0), m_textSize, MathUtility::RectF(0, 0, 1, 1));

                FontManager::DeleteRenderText(textHandle);
            }
        }
        m_textIsDirty = false;
    }
}

void Text::Render()
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

const std::string Text::GetText()
{
    return m_text;
}

void Text::SetText(const std::string value)
{
    if (m_text == value) 
    {
        return;
    }
    m_text = value;
    m_textIsDirty = true;
}

const std::string Text::GetFontName()
{
    return m_fontName;
}

void Text::SetFontName(const std::string value)
{
    if (m_fontName == value) 
    {
        return;
    }
    m_fontName = value;
    m_textIsDirty = true;
}

const uint32_t Text::GetFontStyle()
{
    return m_fontStyle;
}

void Text::SetFontStyle(const uint32_t value)
{
    if (m_fontStyle == value) 
    {
        return;
    }
    m_fontStyle = value;
    m_textIsDirty = true;
}

const uint32_t Text::GetFontSize()
{
    return m_fontSize;
}

void Text::SetFontSize(const uint32_t value)
{
    if (m_fontSize == value) 
    {
        return;
    }
    m_fontSize = value;
    m_textIsDirty = true;
}

const MathUtility::SizeF Text::GetTextSize()
{
    return m_textSize;
}

const MathUtility::Color4F Text::GetTint()
{
    return m_tint;
}

void Text::SetTint(const MathUtility::Color4F value)
{
    m_tint = value;
}

const BlendOperation Text::GetBlend()
{
    return m_blendOperation;
}

void Text::SetBlend(const BlendOperation value)
{
    m_blendOperation = value;
}

const BlendFactor Text::GetBlendFactorSrc()
{
    return m_blendFactorSrc;
}

void Text::SetBlendFactorSrc(const BlendFactor value)
{
    m_blendFactorSrc = value;
}

const BlendFactor Text::GetBlendFactorDst()
{
    return m_blendFactorDst;
}

void Text::SetBlendFactorDst(const BlendFactor value)
{
    m_blendFactorDst = value;
}

const DrawModeOperation Text::GetDrawMode()
{
    return m_drawModeOperation;
}

void Text::SetDrawMode(const DrawModeOperation value)
{
    m_drawModeOperation = value;
}

const CullingOperation Text::GetCulling()
{
    return m_cullingOperation;
}

void Text::SetCulling(const CullingOperation value)
{
    m_cullingOperation = value;
}

const DepthOperation Text::GetDepth()
{
    return m_depthOperation;
}

void Text::SetDepth(const DepthOperation value)
{
    m_depthOperation = value;
}

bool Text::HitTest(const double screenPosX, const double screenPosY, OrthoCamera* camera)
{
    MathUtility::Vec2F screenCoord = MathUtility::Vec2F((float)screenPosX, (float)screenPosY);
    return camera->TestRayIntersectsObb(screenCoord, MathUtility::Vec3F(), MathUtility::Vec3F(m_textSize.width, m_textSize.height, 0.0f), GetTransform());
}