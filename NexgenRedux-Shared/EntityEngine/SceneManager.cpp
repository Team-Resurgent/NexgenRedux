#include "SceneManager.h"
#include "Node.h"

#include <Gensys/DebugUtility.h>
#include <Gensys/StringUtility.h>
#include <Gensys/Int.h>

#include <algorithm>
#include <vector>
#include <map>

using namespace Gensys;
using namespace NexgenRedux;

namespace
{
    uint32_t m_currentSceneID = 0;
    uint32_t m_maxSceneID = 0;
    std::map<uint32_t, std::vector<uint32_t> > m_sceneMap;
}

void SceneManager::Update(float dt) 
{
    NodeManager::PurgeNodes();

    std::map<uint32_t, std::vector<uint32_t> >::iterator itScene = m_sceneMap.find(m_currentSceneID);
	if (itScene != m_sceneMap.end()) 
	{
        std::vector<uint32_t>* sceneNodeLookup = &itScene->second;
        for (uint32_t i = 0; i < sceneNodeLookup->size(); i++)
        {
            NodeManager::Update(sceneNodeLookup->at(i), dt);
        }
	}
}

void SceneManager::Render() 
{
    std::map<uint32_t, std::vector<uint32_t> >::iterator itScene = m_sceneMap.find(m_currentSceneID);
	if (itScene != m_sceneMap.end()) 
    {
        std::vector<uint32_t>* sceneNodeLookup = &itScene->second;
        for (uint32_t i = 0; i < sceneNodeLookup->size(); i++)
        {
            NodeManager::Render(sceneNodeLookup->at(i));
        }
    }
}

uint32_t SceneManager::CreateScene(bool setAsCurrent)
{
    uint32_t sceneID = ++m_maxSceneID;
    std::vector<uint32_t> childNodes;
    m_sceneMap.insert(std::pair<uint32_t, std::vector<uint32_t> >(sceneID, childNodes));
    if (setAsCurrent == true)
    {
        m_currentSceneID = sceneID;
    }
    return sceneID;
}

void SceneManager::SetCurrentScene(uint32_t sceneID)
{
    m_currentSceneID = sceneID;
}

bool SceneManager::AssignNode(Node* node, uint32_t sceneID)
{
    // Check if node already added
    if (node != NULL && node->GetAssigned() == true)
    {
        DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, StringUtility::FormatString("Node '%i' already assigned.", node->GetID()));
        return false;
    }

    // Check scene exists
    std::map<uint32_t, std::vector<uint32_t> >::iterator itScene = m_sceneMap.find(sceneID);
	if (itScene == m_sceneMap.end()) 
	{
        return false;
    }

    itScene->second.push_back(node->GetID());
    node->SetAssigned();
    return true;
}

// Privates

bool SceneManager::DeleteSceneNode(uint32_t sceneID, uint32_t nodeID)
{
    std::map<uint32_t, std::vector<uint32_t> >::iterator itScene = m_sceneMap.find(sceneID);
	if (itScene == m_sceneMap.end()) 
	{
        return false;
    }
    std::vector<uint32_t>* sceneNodeLookup = &itScene->second;
    std::vector<uint32_t>::iterator itSceneNode = std::find(sceneNodeLookup->begin(), sceneNodeLookup->end(), nodeID);
    if (itSceneNode == sceneNodeLookup->end())
    {
        return false;
    }
    sceneNodeLookup->erase(itSceneNode);
    return true;
}

bool SceneManager::DeleteSceneNode(uint32_t nodeID)
{
	for (std::map<uint32_t, std::vector<uint32_t> >::iterator it = m_sceneMap.begin(); it != m_sceneMap.end(); ++it)
    {
        if (DeleteSceneNode(it->first, nodeID) == true)
        {
            return true;
        }
    }
    return false;
}