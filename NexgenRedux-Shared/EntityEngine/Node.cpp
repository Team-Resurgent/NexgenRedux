#include "Node.h"
#include "MathUtility.h"
#include "NodeManager.h"
#include "GlobalTypes.h"

#include <Gensys/Int.h>
#include <Gensys/DebugUtility.h>

#include <algorithm>
#include <string>

using namespace Gensys;
using namespace NexgenRedux;

Node::Node(uint32_t nodeID)
{
    m_nodeID = nodeID;
    m_parentNodeID = 0;
    m_assigned = false;
    m_deleteFlag = false;

    m_anchor = MathUtility::Vec3F(0, 0, 0);
    m_rotation = MathUtility::Vec3F(0, 0, 0);
    m_skew = MathUtility::Vec3F(0, 0, 0);
    m_scale = MathUtility::Vec3F(1, 1, 1);
    m_translation = MathUtility::Vec3F(0, 0, 0);
    m_isDirty = true;
}

uint32_t Node::GetID()
{
    return m_nodeID;
}

uint32_t Node::GetParentID()
{
    return m_parentNodeID;
}

void Node::AddChildNode(uint32_t childNodeID)
{
    m_childNodes.push_back(childNodeID);
}

void Node::AddChildNodeAt(uint32_t childNodeID, uint32_t insertNodeID)
{
    std::vector<uint32_t>::iterator it = std::find(m_childNodes.begin(), m_childNodes.end(), insertNodeID);
    if (it == m_childNodes.end())
    {
        return;
    }
    m_childNodes.insert(it, childNodeID);
}

const std::vector<uint32_t>& Node::GetChildNodes()
{
    return m_childNodes;
}

void Node::EraseChild(uint32_t nodeID)
{
    std::vector<uint32_t>::iterator it = std::find(m_childNodes.begin(), m_childNodes.end(), nodeID);
    if (it != m_childNodes.end())
    {
        m_childNodes.erase(it);
    }
}

const MathUtility::Vec3F Node::GetAnchor()
{
    return m_anchor;
}

void Node::SetAnchor(MathUtility::Vec3F value)
{
    if (m_anchor == value) 
    {
        return;
    }
    m_anchor = value;
    m_isDirty = true;
}

const MathUtility::Vec3F Node::GetRotation()
{
    return m_rotation;
}

void Node::SetRotation(MathUtility::Vec3F value)
{
    if (m_rotation == value) 
    {
        return;
    }
    m_rotation = value;
    m_isDirty = true;
}

const MathUtility::Vec3F Node::GetSkew()
{
    return m_skew;
}

void Node::SetSkew(MathUtility::Vec3F value)
{
    if (m_skew == value) 
    {
        return;
    }
    m_skew = value;
    m_isDirty = true;
}

const MathUtility::Vec3F Node::GetScale()
{
    return m_scale;
}

void Node::SetScale(MathUtility::Vec3F value)
{
    if (m_scale == value) 
    {
        return;
    }
    m_scale = value;
    m_isDirty = true;
}

const MathUtility::Vec3F Node::GetTranslation()
{
    return m_translation;
}

void Node::SetTranslation(MathUtility::Vec3F value)
{
    if (m_translation == value) 
    {
        return;
    }
    m_translation = value;
    m_isDirty = true;
}

const MathUtility::Matrix4x4 Node::GetTransform() 
{
    if (m_isDirty == false)
    {
        return m_matrix;
    }

    MathUtility::Matrix4x4 parentMatrix = MathUtility::Matrix4x4();
    if (m_parentNodeID != 0)
    {
        Node* parentNode = NodeManager::GetNode(m_parentNodeID);
        parentMatrix = parentNode->GetTransform();
    }
    
    MathUtility::Matrix4x4 inverseAnchorMatrix = MathUtility::Matrix4x4::Translate(MathUtility::Vec3F() - m_anchor);
    MathUtility::Matrix4x4 rotationMatrix = MathUtility::Matrix4x4::Rotation(m_rotation);
    MathUtility::Matrix4x4 scaleMatrix = MathUtility::Matrix4x4::Scale(m_scale);
    MathUtility::Matrix4x4 skewMatrix = MathUtility::Matrix4x4::Skew(m_skew);
    MathUtility::Matrix4x4 anchorPositionMatrix = MathUtility::Matrix4x4::Translate(m_anchor + m_translation);
    m_matrix = parentMatrix * inverseAnchorMatrix * rotationMatrix * scaleMatrix * skewMatrix * anchorPositionMatrix;
    m_isDirty = false;
    return m_matrix;
}

// Private Friends

void Node::MarkForDelete()
{
    m_deleteFlag = true;
}

bool Node::MarkedForDelete()
{
    return m_deleteFlag;
}

void Node::SetParentID(uint32_t parentNodeID)
{
    m_parentNodeID = parentNodeID;
}

bool Node::GetAssigned()
{
    return m_assigned;
}

void Node::SetAssigned()
{
    m_assigned = true;
}