#pragma once

#include "NodeManager.h"

#include <Gensys/Int.h>

#include <map>
#include <vector>

class NodeManager;

class SceneManager 
{
public:
    SceneManager(void);

    uint32_t CreateScene(bool setAsCurrent);
    void SetCurrentScene(uint32_t sceneID);
    void Update(NodeManager* nodeManager, float dt);
    void Render(NodeManager* nodeManager);
    
private:

    friend class NodeManager;
    bool AddSceneNode(uint32_t sceneID, uint32_t nodeID);
    bool DeleteSceneNode(uint32_t sceneID, uint32_t nodeID);
    bool DeleteSceneNode(uint32_t nodeID);

private:

    uint32_t m_currentSceneID;
    uint32_t m_maxSceneID;
    std::map<uint32_t, std::vector<uint32_t>> m_sceneMap;
};