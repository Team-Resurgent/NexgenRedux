#pragma once

#include "GlobalTypes.h"
#include "MathUtility.h"

#include <vector>
#include <map>
#include <string>

namespace NexgenRedux
{
    // struct NodeX
    // {
    // private:
    //     uint32_t m_nodeID;
    //     uint32_t m_parentNodeID;
    //     MathUtility::Vec3F m_anchor;
    //     MathUtility::Vec3F m_rotation;
    //     MathUtility::Vec3F m_skew;
    //     MathUtility::Vec3F m_scale;
    //     MathUtility::Vec3F m_position;
    //     MathUtility::Matrix4x4 m_matrix;
    // };

    class NodeManager;

    class Node 
    {
    public:
        Node(uint32_t nodeID);
        virtual ~Node(void) {};

        uint32_t GetID();
        uint32_t GetParentID();

        void AddChildNode(uint32_t childNodeID);
        void AddChildNodeAt(uint32_t childNodeID, uint32_t insertNodeID);
        const std::vector<uint32_t>& GetChildNodes();
        void EraseChild(uint32_t nodeID);

        virtual void Update(float dt) = 0;
        virtual void Render() = 0;

        // Properties
        const MathUtility::Vec3F GetAnchor();
        void SetAnchor(MathUtility::Vec3F value);
        const MathUtility::Vec3F GetRotation();
        void SetRotation(MathUtility::Vec3F value);
        const MathUtility::Vec3F GetSkew();
        void SetSkew(MathUtility::Vec3F value);
        const MathUtility::Vec3F GetScale();
        void SetScale(MathUtility::Vec3F value);
        const MathUtility::Vec3F GetPosition();
        void SetPosition(MathUtility::Vec3F value);
        const MathUtility::Matrix4x4 GetTransform();

    private:

        friend class NodeManager;
        friend class SceneManager;
        void MarkForDelete();
        bool MarkedForDelete();
        void SetParentID(uint32_t parentNodeID);
        bool GetAssigned();
        void SetAssigned();

    private:

        friend class  Sprite;

        uint32_t m_nodeID;
        uint32_t m_parentNodeID;
        bool m_assigned;
        std::vector<uint32_t> m_childNodes;
        bool m_deleteFlag;

        bool m_isDirty;
        MathUtility::Vec3F m_anchor;
        MathUtility::Vec3F m_rotation;
        MathUtility::Vec3F m_skew;
        MathUtility::Vec3F m_scale;
        MathUtility::Vec3F m_position;
        MathUtility::Matrix4x4 m_matrix;
    };
}