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

        static void Close(void);
        static NodeSprite* CreateSprite();
        static bool AssignNode(Node* node, uint32_t parentNodeID);
        static bool AssignNodeAt(Node* node, uint32_t parentNodeID, uint32_t insertNodeID);
        static void DeleteNode(uint32_t nodeID);    

        static void PurgeNodes();
        static void CheckForOrphans();

        // bool SetNodePropertyAnchor(uint32_t nodeID, MathUtility::Vec3F anchor);
        // bool SetNodePropertyRotation(uint32_t nodeID, MathUtility::Vec3F rotation);
        // bool SetNodePropertySkew(uint32_t nodeID, MathUtility::Vec3F skew);
        // bool SetNodePropertyPosition(uint32_t nodeID, MathUtility::Vec3F position);

    private:

        friend class SceneManager;
        friend class Node;
        static Node* GetNode(uint32_t nodeID);
        static void Update(uint32_t nodeID, float dt);
        static void Render(uint32_t nodeID);

    private:

        static void MarkForDelete(uint32_t nodeID);

    };
}