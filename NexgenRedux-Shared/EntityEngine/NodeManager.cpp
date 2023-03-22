#include "NodeManager.h"
#include "NodeSprite.h"

#include <Gensys/DebugUtility.h>
#include <Gensys/Int.h>

#include <string>
#include <map>

using namespace Gensys;
using namespace NexgenRedux;

namespace 
{
    uint32_t m_maxNodeID = 0;
    std::map<uint32_t, Node*> m_nodeMap;
}

void NodeManager::Close(void)
{
    DebugUtility::LogMessage(DebugUtility::LOGLEVEL_INFO, "Deleting node manager");

	for (std::map<uint32_t, Node*>::iterator it = m_nodeMap.begin(); it != m_nodeMap.end(); ++it)
	{
        delete it->second;
	}
}

uint32_t NodeManager::AddSceneNode(Node* node, uint32_t sceneID) 
{
    // Check if node already added
    if (node->GetID() != 0)
    {
        return 0;
    }

    uint32_t nodeID = ++m_maxNodeID;
    node->SetID(nodeID);
    m_nodeMap.insert(std::pair<uint32_t, Node*>(nodeID, node));
    SceneManager::AddSceneNode(sceneID, nodeID);
    return nodeID;
}

uint32_t NodeManager::AddNode(Node* node, uint32_t parentNodeID)
{
    // Check if node already added
    if (node->GetID() != 0)
    {
        return 0;
    }

    // Check parent mpde exists
    Node* parentNode = GetNode(parentNodeID);
    if (parentNode == NULL)
    {
        return 0;
    }

    uint32_t newNodeID = ++m_maxNodeID;
    node->SetID(newNodeID);
    node->SetParent(parentNode);
    m_nodeMap.insert(std::pair<uint32_t, Node*>(newNodeID, node));
    parentNode->AddChildNode(newNodeID);
    return newNodeID;
}

uint32_t NodeManager::AddNodeAt(Node* node, uint32_t parentNodeID, uint32_t nodeID)
{
    // Check if node already added
    if (node->GetID() != 0)
    {
        return 0;
    }

    // Check parent mpde exists
    Node* parentNode = GetNode(parentNodeID);
    if (parentNode == NULL)
    {
        return 0;
    }

    uint32_t newNodeID = ++m_maxNodeID;
    node->SetID(newNodeID);
    node->SetParent(parentNode);
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
	for (std::map<uint32_t, Node*>::iterator it = m_nodeMap.begin(); it != m_nodeMap.end();)
    {
        // Delete nddes marked for delete
        if (it->second->MarkedForDelete() == true)
        { 
            std::map<uint32_t, Node*>::iterator itErase = it;
            ++it;

            // Delete this node from parent if exists
            Node* parentNode = itErase->second->GetParent(); 
            if (parentNode == NULL || parentNode->GetID() == 0)
            {
                SceneManager::DeleteSceneNode(itErase->first);
            }
            else
            {
                parentNode->EraseChild(itErase->first);
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
    for (std::map<uint32_t, Node*>::iterator it = m_nodeMap.begin(); it != m_nodeMap.end(); ++it)
    {
        Node* parentNode = it->second->GetParent();
        if (parentNode == NULL || parentNode->GetID() == 0)
        {
            continue;
        }
        DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "Orphan node detected");
    }
}

void NodeManager::Update(uint32_t nodeID, float dt)
{
    Node* node = GetNode(nodeID);
    if (node == NULL || node->MarkedForDelete())
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