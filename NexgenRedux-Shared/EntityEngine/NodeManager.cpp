#include "NodeManager.h"
#include "Node.h"

#include <Gensys/DebugUtility.h>
#include <Gensys/Int.h>

#include <string>
#include <map>

using namespace Gensys;

NodeManager::NodeManager(void) : m_maxNodeID(0)
{
    
}

NodeManager::~NodeManager(void)
{
    DebugUtility::LogMessage(DebugUtility::LOGLEVEL_INFO, "Deleting node manager");

	for (std::map<uint32_t, Node*>::iterator it = m_nodeMap.begin(); it != m_nodeMap.end(); ++it)
	{
        delete it->second;
	}
}

const std::string* NodeManager::GetNodeTag(uint32_t nodeID)
{
    std::map<uint32_t, Node*>::iterator it = m_nodeMap.find(nodeID);
	if (it != m_nodeMap.end()) 
    {
        return &it->second->GetTag();
    }
    return NULL;
}

void NodeManager::MarkForDelete(uint32_t nodeID) 
{
    Node* node = GetNode(nodeID);
	if (node != NULL) 
    {
        const std::vector<uint32_t> childNodes = node->GetChildNodes();
        for (uint32_t i = 0; i < childNodes.size(); ++i)
        {
            MarkForDelete(childNodes.at(i));
        }
        node->MarkForDelete();
    }
}

uint32_t NodeManager::CreateNode(std::string nodeTag) 
{
    uint32_t nodeID = ++m_maxNodeID;
    Node* node = new Node(nodeTag);
    m_nodeMap.insert(std::pair<uint32_t, Node*>(nodeID, node));
    return nodeID;
}

uint32_t NodeManager::CreateChildNode(uint32_t parentNodeID, std::string nodeTag)
{
    Node* parentNode = GetNode(parentNodeID);
    if (parentNode == NULL)
    {
        return 0;
    }

    uint32_t nodeID = ++m_maxNodeID;
    Node* node = new Node(parentNodeID, nodeTag);
    m_nodeMap.insert(std::pair<uint32_t, Node*>(nodeID, node));
    parentNode->AddChildNode(nodeID);
    return nodeID;
}

// Privates

Node* NodeManager::GetNode(uint32_t nodeID)
{
    std::map<uint32_t, Node*>::iterator it = m_nodeMap.find(nodeID);
	if (it != m_nodeMap.end()) 
    {
        return it->second;
    }
    return NULL;
} 

void NodeManager::CleanNodes() 
{
    for (auto it = m_nodeMap.begin(); it != m_nodeMap.end();)
    {
        // Delete nddes marked for delete
        if (it->second->MarkedForDelete() == true)
        { 
            std::map<uint32_t, Node*>::iterator itErase = it;
            ++it;

            // Delete this node from parent if exists
            uint32_t parentNodeID = itErase->second->GetParentID();
            Node* parentNode = GetNode(parentNodeID);
            if (parentNode != NULL)
            {
                parentNode->DeleteChild(itErase->first);
            }

            // Close and delete from node map
            delete itErase->second;
            m_nodeMap.erase(itErase);
            continue;
        }
        ++it;
    }
}

void NodeManager::CheckForOrphans()
{
    for (auto it = m_nodeMap.begin(); it != m_nodeMap.end(); ++it)
    {
        uint32_t parentID = it->second->GetParentID();
        if (parentID == 0)
        {
            continue;
        }

        Node* parentNode = GetNode(parentID);
        if (parentNode == NULL)
        {
            DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "Orphan node detected");
        }
    }
}