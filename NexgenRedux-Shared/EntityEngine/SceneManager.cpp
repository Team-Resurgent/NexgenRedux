#include "SceneManager.h"
#include "Scene.h"
#include "NodeManager.h"

#include <Gensys/Int.h>

#include <string>
#include <map>

SceneManager::SceneManager(NodeManager* nodeManager) : m_currentSceneID(0), m_maxSceneID(0)
{
    m_nodeManager = nodeManager;
}

SceneManager::~SceneManager(void)
{
	for (std::map<uint32_t, Scene*>::iterator it = m_sceneMap.begin(); it != m_sceneMap.end(); ++it)
	{
        delete it->second;
	}
}

uint32_t SceneManager::CreateScene(std::string sceneTag, bool setAsCurrent) 
{
    uint32_t sceneID = ++m_maxSceneID;
    m_sceneMap.insert(std::pair<uint32_t, Scene*>(sceneID, new Scene(sceneTag)));
    if (setAsCurrent == true)
    {
        SetCurrentScene(sceneID);
    }
    return sceneID;
}

void SceneManager::MarkSceneForDelete(uint32_t sceneID) 
{
    std::map<uint32_t, Scene*>::iterator it = m_sceneMap.find(m_currentSceneID);
	if (it != m_sceneMap.end()) 
    {
        it->second->MarkForDelete();
    }
}

void SceneManager::SetCurrentScene(uint32_t sceneID)
{
    m_currentSceneID = sceneID;
}


void SceneManager::Update(float dt) 
{
    std::map<uint32_t, Scene*>::iterator it = m_sceneMap.find(m_currentSceneID);
	if (it != m_sceneMap.end()) 
	{
		it->second->Update(m_nodeManager, dt);
	}
}

void SceneManager::Render() 
{
    std::map<uint32_t, Scene*>::iterator it = m_sceneMap.find(m_currentSceneID);
	if (it != m_sceneMap.end()) 
    {
        it->second->Render(m_nodeManager);
    }
}

uint32_t SceneManager::CreateSceneNode(uint32_t sceneID, std::string nodeTag)
{
    std::map<uint32_t, Scene*>::iterator it = m_sceneMap.find(sceneID);
	if (it != m_sceneMap.end()) 
	{
        uint32_t nodeID = m_nodeManager->CreateNode(nodeTag);
        it->second->AddNode(nodeID);
        return nodeID;
    }
    return 0;
}

void SceneManager::CleanUp()
{
    // Cleanup scenes
    for (auto it = m_sceneMap.begin(); it != m_sceneMap.end(); )
    {
        // Delete scenes marked for delete
        if (it->second->MarkedForDelete() == true)
        { 
            std::map<uint32_t, Scene*>::iterator itErase = it;
            ++it;
            delete itErase->second;
            m_sceneMap.erase(itErase);
            continue;
        }

        // Delete from scene nodes marked for deletion
        const std::vector<uint32_t> nodeIDs = it->second->GetNodes();
        for (uint32_t i = 0; i < nodeIDs.size(); i++)
        {
            uint32_t nodeID = nodeIDs.at(i);
            Node* node = m_nodeManager->GetNode(nodeID);
            if (node != NULL && node->MarkedForDelete()) 
            {
                it->second->DeleteNode(nodeID);
            }
        }

        ++it;
    }

    // Cleanup any nodes marked for delete
    m_nodeManager->CleanNodes();
}