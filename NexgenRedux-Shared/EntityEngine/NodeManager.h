#pragma once

#include "SceneManager.h"
#include "GlobalTypes.h"
#include "Node.h"

#include <Gensys/Int.h>

#include <map>

namespace NexgenRedux
{
    class SceneManager;
    class Node;

    class NodeManager 
    {
    public:

        NodeManager(SceneManager* sceneManager);
        ~NodeManager(void);

        uint32_t AddSceneNode(Node* node, uint32_t sceneID);
        uint32_t AddNode(Node* node, uint32_t parentNodeID);
        uint32_t AddNodeAt(Node* node, uint32_t parentNodeID, uint32_t nodeID);
        void DeleteNode(uint32_t nodeID);    

        void PurgeNodes();
        void CheckForOrphans();

        // bool SetNodePropertyAnchor(uint32_t nodeID, MathUtility::Vec3F anchor);
        // bool SetNodePropertyRotation(uint32_t nodeID, MathUtility::Vec3F rotation);
        // bool SetNodePropertySkew(uint32_t nodeID, MathUtility::Vec3F skew);
        // bool SetNodePropertyPosition(uint32_t nodeID, MathUtility::Vec3F position);

        void Update(uint32_t nodeID, float dt);
        void Render(uint32_t nodeID);

    private:

        friend class SceneManager;
        friend class Node;
        Node* GetNode(uint32_t nodeID);

    private:

        void MarkForDelete(uint32_t nodeID);

    private:
        SceneManager* m_sceneManager;
        uint32_t m_maxNodeID;
        std::map<uint32_t, Node*> m_nodeMap;
    };
}