#include "NodeManager.h"
#include "Node.h"

#include <Gensys/DebugUtility.h>
#include <Gensys/Int.h>

#include <string>
#include <map>

using namespace Gensys;
using namespace NexgenRedux;

NodeManager::NodeManager(SceneManager* sceneManager) : m_sceneManager(sceneManager), m_maxNodeID(0)
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

uint32_t NodeManager::CreateSceneNode(NodeType nodeType, uint32_t sceneID) 
{
    uint32_t nodeID = ++m_maxNodeID;
    Node* node = new Node(nodeType, nodeID);
    m_nodeMap.insert(std::pair<uint32_t, Node*>(nodeID, node));
    m_sceneManager->AddSceneNode(sceneID, nodeID);
    return nodeID;
}

uint32_t NodeManager::CreateNode(NodeType nodeType, uint32_t parentNodeID)
{
    Node* parentNode = GetNode(parentNodeID);
    if (parentNode == NULL)
    {
        return 0;
    }

    uint32_t newNodeID = ++m_maxNodeID;
    Node* node = new Node(parentNode, nodeType, newNodeID);
    m_nodeMap.insert(std::pair<uint32_t, Node*>(newNodeID, node));
    parentNode->AddChildNode(newNodeID);
    return newNodeID;
}

uint32_t NodeManager::CreateNodeAt(NodeType nodeType, uint32_t parentNodeID, uint32_t nodeID)
{
    Node* parentNode = GetNode(parentNodeID);
    if (parentNode == NULL)
    {
        return 0;
    }

    uint32_t newNodeID = ++m_maxNodeID;
    Node* node = new Node(parentNode, nodeType, newNodeID);
    m_nodeMap.insert(std::pair<uint32_t, Node*>(newNodeID, node));
    parentNode->AddChildNodeAt(nodeID, newNodeID);
    return newNodeID;
}

void NodeManager::DeleteNode(uint32_t nodeID)
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

void NodeManager::PurgeNodes() 
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
            if (parentNodeID == 0)
            {
                m_sceneManager->DeleteSceneNode(itErase->first);
            }
            else
            {
                Node* parentNode = GetNode(parentNodeID);
                if (parentNode != NULL)
                {
                    parentNode->EraseChild(itErase->first);
                }
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

void NodeManager::Update(uint32_t nodeID, float dt)
{
    Node* node = GetNode(nodeID);
    if (node->MarkedForDelete())
    {
        return;
    }

    node->Update(dt);

    const std::vector<uint32_t>* childNodes = &node->GetChildNodes();
    for (uint32_t i = 0; i < childNodes->size(); i++)
    {
        Update(childNodes->at(i), dt);
    }
}

void NodeManager::Render(uint32_t nodeID)
{
    Node* node = GetNode(nodeID);
    if (node->MarkedForDelete())
    {
        return;
    }

    node->Render();

    const std::vector<uint32_t>* childNodes = &node->GetChildNodes();
    for (uint32_t i = 0; i < childNodes->size(); i++)
    {
        Render(childNodes->at(i));
    }
}

// Private Friends

Node* NodeManager::GetNode(uint32_t nodeID)
{
    std::map<uint32_t, Node*>::iterator it = m_nodeMap.find(nodeID);
	if (it != m_nodeMap.end()) 
    {
        return it->second;
    }
    return NULL;
} 

// Privates

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