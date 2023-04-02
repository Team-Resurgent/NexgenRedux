#include "Text.h"
#include "RenderStateManager.h"
#include "ConfigLoader.h"

#include <Gensys/FileSystem.h>
#include <Gensys/DebugUtility.h>
#include <Gensys/StringUtility.h>
#include <stdexcept>
#include <string>

#define SSFN_IMPLEMENTATION
// #include "stdlib.h"
#define SSFN_memcmp memcmp
#define SSFN_memset memset
#define SSFN_realloc realloc
#define SSFN_free free

#include <SSFN/ssfn.h>

using namespace Gensys;
using namespace NexgenRedux;

namespace 
{
    ssfn_t m_fontContext {0};
}

Text::Text(uint32_t nodeID) : Node(nodeID)
{
    m_fontIsDirty = true;
    m_fontPath = "";
    //memset(&m_fontContext, 0, sizeof(m_fontContext));
    m_fontLoaded = false;

    m_textIsDirty = true;
    m_fontSize = 10;



    //m_texturePath = "";
    m_textureID = 0;

    m_meshIsDirty = true;
    //m_uv = MathUtility::RectF(0, 0, 1, 1);
    //m_position = MathUtility::Vec3F(0, 0, 0);
    m_size = MathUtility::SizeF(1, 1);

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

    if (m_fontIsDirty == true)
    {
        std::wstring mappedPath;
        if (ConfigLoader::MapPath(StringUtility::ToWideString(m_fontPath), mappedPath) == 0)
        {
            DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, StringUtility::FormatString("Failed to map path '%s'.", m_fontPath.c_str()));
            return;
        }

        std::vector<uint8_t> buffer;
        if (FileSystem::FileReadAllAsData(mappedPath, &buffer) == false)
        {
            DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, StringUtility::FormatString("Failed to load font file '%s'.", m_fontPath.c_str()));
            return;
        }

        if (m_fontLoaded == true)
        {
            ssfn_free(&m_fontContext);
            m_fontLoaded = false;
        }

        memset(&m_fontContext, 0, sizeof(m_fontContext));
        int status = ssfn_load(&m_fontContext, buffer.data());
        if (status != 0)
        {
            DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "Failed to load font data.");
            return;
        }

        m_fontLoaded = true;
        m_fontIsDirty = false;
    }

    if (m_fontLoaded && m_textIsDirty == true)
    {
        // int status = ssfn_select(&m_fontContext, SSFN_FAMILY_ANY, NULL, SSFN_STYLE_REGULAR, m_fontSize);
        // if (status != 0)
        // {
        //     DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "Failed to select font.");
        //     return;
        // }

        // if (m_textureID != 0)
        // {
        //     renderStateManager->DeleteTexture(m_textureID);
        // }


       // renderStateManager->LoadTextureData()
    //     renderStateManager->LoadTexture(StringUtility::ToWideString(m_texturePath), m_textureID);
        m_textIsDirty = false;
    }

    // if (m_meshIsDirty == true)
    // {
    //     m_mesh = MeshUtility::CreateQuadXY(MathUtility::Vec3F(0, 0, 0), m_size, MathUtility::RectF(0, 0, 1, 1));
    //     m_meshIsDirty = false;
    // }
}

void Text::Render()
{
    if (m_textureID == 0 || m_mesh.size() == 0) 
    {
        return;
    }

    // RenderStateManager* renderStateManager = RenderStateManager::GetInstance();

    // renderStateManager->SetModelMatrix(GetTransform());
	// renderStateManager->SetTexture(m_textureID, TextureFilterLinear);

    // renderStateManager->SetTint(m_tint);
    // renderStateManager->SetBlend(m_blendOperation);
    // renderStateManager->SetBlendFactors(m_blendFactorSrc, m_blendFactorDst);
    // renderStateManager->SetDrawMode(m_drawModeOperation);
    // renderStateManager->SetCulling(m_cullingOperation);
    // renderStateManager->SetDepth(m_depthOperation);

    // renderStateManager->ApplyChanges();
    //renderStateManager->RenderMesh(m_mesh);
}

const std::string Text::GetFontPath()
{
    return m_fontPath;
}

void Text::SetFontPath(const std::string value)
{
    if (m_fontPath == value) 
    {
        return;
    }
    m_fontPath = value;
    m_fontIsDirty = true;
    m_textIsDirty = true;
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

// const MathUtility::SizeF Text::GetSize()
// {
//     return m_size;
// }

// void Text::SetSize(const MathUtility::SizeF value)
// {
//     if (m_size == value) 
//     {
//         return;
//     }
//     m_size = value;
//     m_meshIsDirty = true;
// }

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
    return camera->TestRayIntersectsObb(screenCoord, MathUtility::Vec3F(), MathUtility::Vec3F(m_size.width, m_size.height, 0.0f), GetTransform());
}