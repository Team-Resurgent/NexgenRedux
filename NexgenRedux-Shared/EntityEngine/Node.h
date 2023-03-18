#pragma once

#include "PropertyTransform.h"
#include "GlobalTypes.h"

#include <vector>
#include <map>
#include <string>

namespace NexgenRedux
{
    class NodeManager;

    class Node 
    {
    public:
        Node(NodeType nodeType, uint32_t nodeID);
        Node(Node* parentNode, NodeType nodeType, uint32_t nodeID);
        ~Node(void);

        void MarkForDelete();
        bool MarkedForDelete();
        uint32_t GetID();
        NodeType GetType();
        uint32_t GetParentID();

        void AddChildNode(uint32_t childNodeID);
        void AddChildNodeAt(uint32_t nodeID, uint32_t childNodeID);
        const std::vector<uint32_t>& GetChildNodes();
        void EraseChild(uint32_t nodeID);

        bool HasProperty(PropertyType propertyType);

        void Update(float dt);
        void Render();

    private:

        friend class PropertyTransform;
        Node* GetParentNode();

    private:

        void AddProperty(PropertyType propertyType, Property* property);
        Property* GetProperty(PropertyType propertyType);
        void AddProperties();

    private:

        NodeType m_nodeType;
        Node* m_parentNode;
        uint32_t m_nodeID;
        std::vector<uint32_t> m_childNodes;
        std::map<PropertyType, Property*> m_propertyMap;
        bool m_deleteFlag;
    };
}