#include "Node.h"
#include "PropertyTransform.h"

#include <Gensys/DebugUtility.h>

#include <string>

using namespace Gensys;
using namespace NexgenRedux;

Node::Node(NodeManager* nodeManager, NodeType nodeType, uint32_t nodeID)
{
    m_nodeManager = nodeManager;
    m_nodeType = nodeType;
    m_parentNodeID = 0;
    m_nodeID = nodeID;
    m_deleteFlag = false;

    AddProperties();
}

Node::Node(NodeManager* nodeManager, NodeType nodeType, uint32_t parentNodeID, uint32_t nodeID)
{
    m_nodeManager = nodeManager;
    m_nodeType = nodeType;
    m_parentNodeID = parentNodeID;
    m_nodeID = nodeID;
    m_deleteFlag = false;

    AddProperties();
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

bool Node::HasProperty(PropertyType propertyType)
{
    std::map<PropertyType, Property*>::iterator it = m_propertyMap.find(propertyType);
	return it != m_propertyMap.end();
}

Property* Node::GetProperty(PropertyType propertyType)
{
    std::map<PropertyType, Property*>::iterator it = m_propertyMap.find(propertyType);
	if (it == m_propertyMap.end()) 
    {
        return NULL;
    }
    return it->second;
}

// Private Friends

Node* Node::GetParentNode()
{
    return m_nodeManager->GetNode(m_parentNodeID);
}

// Privates

void Node::AddProperty(PropertyType propertyType, Property* property)
{
    m_propertyMap.insert(std::pair<PropertyType, Property*>(propertyType, property));
}

void Node::AddProperties()
{
    AddProperty(PropertyTypeTransform, new PropertyTransform(this));
}