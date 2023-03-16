#include "Node.h"

#include <Gensys/DebugUtility.h>

#include <string>

using namespace Gensys;

Node::Node(std::string nodeTag)
{
    m_parentID = 0;
    m_nodeTag = nodeTag;
    m_deleteFlag = false;
}

Node::Node(uint32_t parentID, std::string nodeTag)
{
    m_parentID = parentID;
    m_nodeTag = nodeTag;
    m_deleteFlag = false;
}

Node::~Node(void)
{
    DebugUtility::LogMessage(DebugUtility::LOGLEVEL_INFO, "Deleting node '%s'", m_nodeTag.c_str());
}

const std::string& Node::GetTag()
{
    return m_nodeTag;
}

void Node::MarkForDelete()
{
    m_deleteFlag = true;
}

bool Node::MarkedForDelete()
{
    return m_deleteFlag;
}

uint32_t Node::GetParentID()
{
    return m_parentID;
}

void Node::AddChildNode(uint32_t nodeID)
{
    m_childNodes.push_back(nodeID);
}

const std::vector<uint32_t>& Node::GetChildNodes()
{
    return m_childNodes;
}

void Node::DeleteChild(uint32_t nodeID)
{
    std::vector<uint32_t>::iterator it = std::find(m_childNodes.begin(), m_childNodes.end(), nodeID);
    if (it != m_childNodes.end())
    {
        m_childNodes.erase(it);
    }
}

// Node::Node(int id) {
//     m_id = id;
//     m_parentId = -1;
//     m_positionX = 0;
//     m_positionY = 0;
//     m_rotation = 0;
//     m_scaleX = 1;
//     m_scaleY = 1;
// }

// void Node::Update(float dt) {}

// void Node::Draw() {}

// void Node::AddChild(int childId) 
// {
//     m_childrenIds.push_back(childId);
// }

// void Node::RemoveChild(int childId) 
// {
//     auto it = std::find(m_childrenIds.begin(), m_childrenIds.end(), childId);
//     if (it != m_childrenIds.end()) {
//         m_childrenIds.erase(it);
//     }
// }

// void Node::SetPosition(float x, float y) 
// {
//     m_positionX = x;
//     m_positionY = y;
// }

// void Node::SetRotation(float rotation) 
// {
//     m_rotation = rotation;
// }

// void Node::SetScale(float scaleX, float scaleY) 
// {
//     m_scaleX = scaleX;
//     m_scaleY = scaleY;
// }

// float Node::GetPositionX() const 
// {
//     return m_positionX;
// }

// float Node::GetPositionY() const 
// {
//     return m_positionY;
// }

// float Node::GetRotation() const 
// {
//     return m_rotation;
// }

// float Node::GetScaleX() const 
// {
//     return m_scaleX;
// }

// float Node::GetScaleY() const 
// {
//     return m_scaleY;
// }

// int Node::GetID() const 
// {
//     return m_id;
// }

// int Node::GetParentID() const 
// {
//     return m_parentId;
// }