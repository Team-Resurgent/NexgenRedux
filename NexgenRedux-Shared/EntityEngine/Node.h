#pragma once

#include <vector>
#include <string>

namespace NexgenRedux
{
	typedef enum NodeType {
		NodeTypeCamera,
		NodeTypeSprite
	} NodeType;  

    class Node 
    {
    public:
        Node(NodeType nodeType, uint32_t nodeID);
        Node(NodeType nodeType, uint32_t parentNodeID, uint32_t nodeID);
        ~Node(void);

        void MarkForDelete();
        bool MarkedForDelete();
        uint32_t GetID();
        NodeType GetType();
        uint32_t GetParent();

        void AddChildNode(uint32_t childNodeID);
        void AddChildNodeAt(uint32_t nodeID, uint32_t childNodeID);
        const std::vector<uint32_t>& GetChildNodes();
        void EraseChild(uint32_t nodeID);

    private:

        friend class Scene;
        NodeType m_nodeType;
        uint32_t m_parentNodeID;
        uint32_t m_nodeID;
        std::vector<uint32_t> m_childNodes;
        bool m_deleteFlag;
    };
}