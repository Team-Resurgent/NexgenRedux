#pragma once

#include "NodeManager.h"
#include "Node.h"

#include <Gensys/Int.h>

#include <map>
#include <vector>

namespace NexgenRedux
{
    class NodeManager;

    class SceneManager 
    {
    public:

        static uint32_t CreateScene(bool setAsCurrent);
        static void SetCurrentScene(uint32_t sceneID);
        static bool AssignNode(Node* node, uint32_t sceneID);
        static void Update(float dt);
        static void Render();
        
    private:

        friend class NodeManager;
        static bool DeleteSceneNode(uint32_t sceneID, uint32_t nodeID);
        static bool DeleteSceneNode(uint32_t nodeID);

    };
}