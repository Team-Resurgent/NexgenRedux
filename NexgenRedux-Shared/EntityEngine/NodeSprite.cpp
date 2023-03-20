#include "NodeSprite.h"

#include <Gensys/DebugUtility.h>

#include <string>

using namespace Gensys;
using namespace NexgenRedux;

NodeSprite::NodeSprite(uint32_t nodeID) : Node(nodeID)
{
    m_isDirty = true;
    m_texturePath = "";
    m_uv = MathUtility::RectF(0, 0, 1, 1);
}

NodeSprite::NodeSprite(Node* parentNode, uint32_t nodeID) : Node(parentNode, nodeID)
{
    m_isDirty = true;
    m_texturePath = "";
    m_uv = MathUtility::RectF(0, 0, 1, 1);
}

NodeSprite::~NodeSprite(void)
{
    DebugUtility::LogMessage(DebugUtility::LOGLEVEL_INFO, "Deleting node '%i'", m_nodeID);
}

void NodeSprite::Update(float dt)
{
    if (m_isDirty == true)
    {
        m_isDirty = false;
    }
    //MathUtility::Matrix4x4 matrix = GetTransform();
    //DebugUtility::LogMessage(DebugUtility::LOGLEVEL_INFO, "Updating, Calculated transfor for node '%i'", m_nodeID);
}

void NodeSprite::Render()
{
    DebugUtility::LogMessage(DebugUtility::LOGLEVEL_INFO, "Rendering node '%i'", m_nodeID);
}

const std::string NodeSprite::GetTexturePath()
{
    return m_texturePath;
}

void NodeSprite::SetTexturePath(std::string value)
{
    if (m_texturePath == value) 
    {
        return;
    }
    m_texturePath = value;
    m_isDirty = true;
}

const MathUtility::RectF NodeSprite::GetUV()
{
    return m_uv;
}

void NodeSprite::SetUV(MathUtility::RectF value)
{
    if (m_uv == value) 
    {
        return;
    }
    m_uv = value;
    m_isDirty = true;
}