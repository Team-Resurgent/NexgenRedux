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
    m_meshID = 0;
}

Sprite::~Sprite(void)
{
    RenderStateManager* renderStateManager = RenderStateManager::GetInstance();

    if (m_textureID != 0)
    {
        renderStateManager->DeleteTexture(m_textureID);
    }

    if (m_meshID != 0)
    {
        MeshUtility::DeleteMesh(m_meshID);
    }

    DebugUtility::LogMessage(DebugUtility::LOGLEVEL_INFO, StringUtility::FormatString("Deleting node '%i'", m_nodeID));
}

void Sprite::Update(float dt)
{
    RenderStateManager* renderStateManager = RenderStateManager::GetInstance();

    //TODO: add concept of texture shared (needs ref count)

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
        if (m_meshID != 0)
        {
            MeshUtility::DeleteMesh(m_meshID);
        }
        m_meshID = MeshUtility::CreateQuadXY(MathUtility::Vec3F(0, 0, 0), MathUtility::SizeF(640, 480), MathUtility::RectF(0, 0, 1, 1));
        m_meshIsDirty = false;
    }

    MathUtility::Matrix4x4 matrix = GetTransform();
    DebugUtility::LogMessage(DebugUtility::LOGLEVEL_INFO, StringUtility::FormatString("Updating, Calculated transfor for node '%i'", m_nodeID));
}

void Sprite::Render()
{
    if (m_textureID == 0 || m_meshID == 0) 
    {
        return;
    }

// need to add delete mesh

    RenderStateManager* renderStateManager = RenderStateManager::GetInstance();


	// MathUtility::Matrix4x4 modelMatrix = MathUtility::Matrix4x4::Translate(MathUtility::Vec3F(-320, -240, 0));
    // modelMatrix *= MathUtility::Matrix4x4::RotateZ(10);
    // modelMatrix *= MathUtility::Matrix4x4::Translate(MathUtility::Vec3F(320, 240, 0));

    MathUtility::Matrix4x4 matrix = GetTransform();
    renderStateManager->SetModelMatrix(matrix);

	renderStateManager->SetTexture(m_textureID, TextureFilterLinear);
	renderStateManager->ApplyChanges();
    renderStateManager->RenderMesh(m_meshID);

    DebugUtility::LogMessage(DebugUtility::LOGLEVEL_INFO, StringUtility::FormatString("Rendering node '%i'", m_nodeID));
}

const std::string Sprite::GetTexturePath()
{
    return m_texturePath;
}

void Sprite::SetTexturePath(std::string value)
{
    if (m_texturePath == value) 
    {
        return;
    }
    m_texturePath = value;
    m_isDirty = true;
}

const MathUtility::RectF Sprite::GetUV()
{
    return m_uv;
}

void Sprite::SetUV(MathUtility::RectF value)
{
    if (m_uv == value) 
    {
        return;
    }
    m_uv = value;
    m_isDirty = true;
}