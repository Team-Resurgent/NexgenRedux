#include "Node.h"

#include <Gensys/DebugUtility.h>

#include <string>

using namespace Gensys;
using namespace NexgenRedux;

Node::Node(NodeType nodeType, uint32_t nodeID)
{
    m_nodeType = nodeType;
    m_parentNodeID = 0;
    m_nodeID = nodeID;
    m_deleteFlag = false;
}

Node::Node(NodeType nodeType, uint32_t parentNodeID, uint32_t nodeID)
{
    m_nodeType = nodeType;
    m_parentNodeID = parentNodeID;
    m_nodeID = nodeID;
    m_deleteFlag = false;
}

Node::~Node(void)
{
    DebugUtility::LogMessage(DebugUtility::LOGLEVEL_INFO, "Deleting node '%i'", m_nodeID);
}

void Node::MarkForDelete()
{
    m_deleteFlag = true;
}

bool Node::MarkedForDelete()
{
    return m_deleteFlag;
}

uint32_t Node::GetID()
{
    return m_nodeID;
}

NodeType Node::GetType()
{
    return m_nodeType;
}

uint32_t Node::GetParent()
{
    return m_parentNodeID;
}

void Node::AddChildNode(uint32_t childNodeID)
{
    m_childNodes.push_back(childNodeID);
}

void Node::AddChildNodeAt(uint32_t nodeID, uint32_t childNodeID)
{
    std::vector<uint32_t>::iterator it = std::find(m_childNodes.begin(), m_childNodes.end(), nodeID);
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