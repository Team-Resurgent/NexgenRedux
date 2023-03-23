#include "Sprite.h"

#include <Gensys/DebugUtility.h>
#include <Gensys/StringUtility.h>
#include <stdexcept>
#include <string>

using namespace Gensys;
using namespace NexgenRedux;

Sprite::Sprite(uint32_t nodeID) : Node(nodeID)
{
    m_isDirty = true;
    m_texturePath = "";
    m_uv = MathUtility::RectF(0, 0, 1, 1);
}

Sprite::~Sprite(void)
{
    DebugUtility::LogMessage(DebugUtility::LOGLEVEL_INFO, StringUtility::FormatString("Deleting node '%i'", m_nodeID));
}

void Sprite::Update(float dt)
{
    if (m_isDirty == true)
    {
        m_isDirty = false;
    }
    MathUtility::Matrix4x4 matrix = GetTransform();
    DebugUtility::LogMessage(DebugUtility::LOGLEVEL_INFO, StringUtility::FormatString("Updating, Calculated transfor for node '%i'", m_nodeID));
}

void Sprite::Render()
{
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