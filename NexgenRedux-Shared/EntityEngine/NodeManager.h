#pragma once

#include "SceneManager.h"
#include "Node.h"

#include <Gensys/Int.h>

#include <map>

namespace NexgenRedux
{
    class SceneManager;

    class NodeManager 
    {
    public:

        NodeManager(SceneManager* sceneManager);
        ~NodeManager(void);

        uint32_t CreateSceneNode(NodeType nodeType, uint32_t sceneID);
        uint32_t CreateNode(NodeType nodeType, uint32_t parentNodeID);
        uint32_t CreateNodeAt(NodeType nodeType, uint32_t parentNodeID, uint32_t nodeID);
        void DeleteNode(uint32_t nodeID);    

        void PurgeNodes();
        void CheckForOrphans();

        void Update(uint32_t nodeID, float dt);
        void Render(uint32_t nodeID);

    private:

        friend class SceneManager;
        Node* GetNode(uint32_t nodeID);

    private:

        void MarkForDelete(uint32_t nodeID);

    private:
        SceneManager* m_sceneManager;
        uint32_t m_maxNodeID;
        std::map<uint32_t, Node*> m_nodeMap;
    };
}