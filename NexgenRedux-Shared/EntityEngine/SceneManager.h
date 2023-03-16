#pragma once

#include "Scene.h"
#include "NodeManager.h"

#include <Gensys/Int.h>

#include <string>
#include <map>

class SceneManager 
{
public:
    SceneManager(NodeManager* nodeManager);
    ~SceneManager(void);

    uint32_t CreateScene(std::string sceneTag, bool setAsCurrent);
    void MarkSceneForDelete(uint32_t sceneID);
    void SetCurrentScene(uint32_t sceneID);

    void Update(float dt);
    void Render();

    uint32_t CreateSceneNode(uint32_t sceneID, std::string nodeTag);

    void CleanUp();
    
private:

    NodeManager* m_nodeManager;
    uint32_t m_currentSceneID;
    uint32_t m_maxSceneID;
    std::map<uint32_t, Scene*> m_sceneMap;
};