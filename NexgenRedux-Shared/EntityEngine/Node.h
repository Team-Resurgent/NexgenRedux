#pragma once

#include "NodeManager.h"
#include "PropertyTransform.h"

#include <vector>
#include <map>
#include <string>

namespace NexgenRedux
{
    class NodeManager;

    class Node 
    {
    public:
        Node(NodeManager* nodeManager, NodeType nodeType, uint32_t nodeID);
        Node(NodeManager* nodeManager, NodeType nodeType, uint32_t parentNodeID, uint32_t nodeID);
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

        bool HasProperty(PropertyType propertyType);
        Property* GetProperty(PropertyType propertyType);

    private:

        friend class PropertyTransform;
        Node* GetParentNode();

    private:

        void AddProperty(PropertyType propertyType, Property* property);
        void AddProperties();

    private:

        friend class Scene;
        NodeManager* m_nodeManager;
        NodeType m_nodeType;
        uint32_t m_parentNodeID;
        uint32_t m_nodeID;
        std::vector<uint32_t> m_childNodes;
        std::map<PropertyType, Property*> m_propertyMap;
        bool m_deleteFlag;
    };
}