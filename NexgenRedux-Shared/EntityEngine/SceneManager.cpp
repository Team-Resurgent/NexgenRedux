#include "SceneManager.h"
#include "Scene.h"

#include <Gensys/Int.h>

#include <map>
#include <vector>

SceneManager::SceneManager(void) : m_currentSceneID(0), m_maxSceneID(0)
{
}

SceneManager::~SceneManager(void)
{
	for (std::map<uint32_t, Scene*>::iterator it = m_sceneMap.begin(); it != m_sceneMap.end(); ++it)
	{
        delete it->second;
	}
}

uint32_t SceneManager::AddScene(Scene* scene) 
{
    m_maxSceneID++;
    m_sceneMap.insert(std::pair<uint32_t, Scene*>(m_maxSceneID, scene));
    return m_maxSceneID;
}

void SceneManager::SetCurrentScene(uint32_t sceneID)
{
    m_currentSceneID = sceneID;
}

void SceneManager::RemoveScene(uint32_t sceneID) 
{
    std::map<uint32_t, Scene*>::iterator it = m_sceneMap.find(m_currentSceneID);
	if (it == m_sceneMap.end()) 
    {
        m_sceneMap.erase(it);
        delete it->second;
    }
}

void SceneManager::Update(float dt) 
{
    std::map<uint32_t, Scene*>::iterator it = m_sceneMap.find(m_currentSceneID);
	if (it == m_sceneMap.end()) 
	{
		it->second->Update(dt);
	}
}

void SceneManager::Render() 
{
    std::map<uint32_t, Scene*>::iterator it = m_sceneMap.find(m_currentSceneID);
	if (it == m_sceneMap.end()) 
    {
        it->second->Render();
    }
}

uint32_t SceneManager::AddNode(uint32_t sceneID, Node* node)
{
    std::map<uint32_t, Scene*>::iterator it = m_sceneMap.find(sceneID);
	if (it == m_sceneMap.end()) 
	{
        return it->second->AddNode(node);
    }
    return 0;
}

void SceneManager::RemoveNode(uint32_t sceneID, uint32_t nodeID)
{
    std::map<uint32_t, Scene*>::iterator it = m_sceneMap.find(sceneID);
	if (it == m_sceneMap.end()) 
	{
        it->second->RemoveNode(nodeID);
    }
}