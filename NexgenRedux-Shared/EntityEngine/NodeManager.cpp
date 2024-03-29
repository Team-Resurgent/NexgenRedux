#include "NodeManager.h"
#include "OrthoCamera.h"
#include "PerspectiveCamera.h"
#include "Fog.h"
#include "Lighting.h"
#include "Sprite.h"

#include <Gensys/DebugUtility.h>
#include <Gensys/StringUtility.h>
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

OrthoCamera* NodeManager::CreateOrthoCamera()
{
    uint32_t nodeID = ++m_maxNodeID;
    OrthoCamera* node = new OrthoCamera(nodeID);
    m_nodeMap.insert(std::pair<uint32_t, Node*>(nodeID, node));
    return node;
}

PerspectiveCamera* NodeManager::CreatePerspectiveCamera()
{
    uint32_t nodeID = ++m_maxNodeID;
    PerspectiveCamera* node = new PerspectiveCamera(nodeID);
    m_nodeMap.insert(std::pair<uint32_t, Node*>(nodeID, node));
    return node;
}

Fog* NodeManager::CreateFog()
{
    uint32_t nodeID = ++m_maxNodeID;
    Fog* node = new Fog(nodeID);
    m_nodeMap.insert(std::pair<uint32_t, Node*>(nodeID, node));
    return node;
}

Lighting* NodeManager::CreateLighting()
{
    uint32_t nodeID = ++m_maxNodeID;
    Lighting* node = new Lighting(nodeID);
    m_nodeMap.insert(std::pair<uint32_t, Node*>(nodeID, node));
    return node;
}

Sprite* NodeManager::CreateSprite()
{
    uint32_t nodeID = ++m_maxNodeID;
    Sprite* node = new Sprite(nodeID);
    m_nodeMap.insert(std::pair<uint32_t, Node*>(nodeID, node));
    return node;
}

Text* NodeManager::CreateText()
{
    uint32_t nodeID = ++m_maxNodeID;
    Text* node = new Text(nodeID);
    m_nodeMap.insert(std::pair<uint32_t, Node*>(nodeID, node));
    return node;
}

bool NodeManager::AssignNode(Node* node, uint32_t parentNodeID)
{
    // Check if node already added
    if (node->GetAssigned() == true)
    {
        DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, StringUtility::FormatString("AssignNode: Node '%i' already assigned.", node->GetID()));
        return false;
    }

    Node* parentNode = NodeManager::GetNode(parentNodeID);
    if (parentNode == NULL)
    {
        DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, StringUtility::FormatString("AssignNode: Parent node '%i' not found.", parentNodeID));
        return false;
    }

    node->SetParentID(parentNode->GetID());
    parentNode->AddChildNode(node->GetID());
    node->SetAssigned();
    return true;
}

bool NodeManager::AssignNodeAt(Node* node, uint32_t parentNodeID, uint32_t insertNodeID)
{
    // Check if node already added
    if (node->GetAssigned() == true)
    {
        DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, StringUtility::FormatString("AssignNodeAt: Node '%i' already assigned.", node->GetID()));
        return false;
    }

    Node* parentNode = NodeManager::GetNode(parentNodeID);
    if (parentNode == NULL)
    {
        DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, StringUtility::FormatString("AssignNodeAt: Parent node '%i' not found.", parentNodeID));
        return false;
    }

    node->SetParentID(parentNodeID);
    parentNode->AddChildNodeAt(node->GetID(), insertNodeID);
    node->SetAssigned();
    return true;
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

Node* NodeManager::GetNode(uint32_t nodeID)
{
    std::map<uint32_t, Node*>::iterator it = m_nodeMap.find(nodeID);
	if (it != m_nodeMap.end()) 
    {
        return it->second;
    }
    return NULL;
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
            uint32_t parentNodeID = itErase->second->GetParentID(); 
            if (parentNodeID == 0)
            {
                SceneManager::DeleteSceneNode(itErase->first);
            }
            else
            {
                Node* parentNode = GetNode(parentNodeID);
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
        uint32_t parentNodeID = it->second->GetParentID(); 
        if (parentNodeID == 0)
        {
            continue;
        }
        Node* parentNode = GetNode(parentNodeID);
        if (parentNode != NULL)
        {
            continue;
        }
        DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "Orphan node detected.");
    }
    DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "Orphan check completed.");
}

// Private Friends

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