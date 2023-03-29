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

    MathUtility::Vec3F rotation = GetRotation();

    renderStateManager->SetModelMatrix(GetTransform());
	renderStateManager->SetTexture(m_textureID, TextureFilterLinear);
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

const MathUtility::Vec3F Sprite::GetPosition()
{
    return m_position;
}

void Sprite::SetPosition(const MathUtility::Vec3F value)
{
    if (m_position == value) 
    {
        return;
    }
    m_position = value;
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