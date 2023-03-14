#pragma once

#include "Scene.h"

#include <Gensys/Int.h>

class SceneManager 
{
public:
    SceneManager(void);
    ~SceneManager(void);

    uint32_t AddScene(Scene* scene);
    void SetCurrentScene(uint32_t sceneID);
    void RemoveScene(uint32_t sceneID);
    void Update(float dt);
    void Render();

    uint32_t AddNode(uint32_t sceneID, Node* node);
    void RemoveNode(uint32_t sceneID, uint32_t nodeID);
    
private:
    uint32_t m_currentSceneID;
    uint32_t m_maxSceneID;
    std::map<uint32_t, Scene*> m_sceneMap;
};