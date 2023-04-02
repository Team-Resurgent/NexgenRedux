#pragma once

#include "GlobalTypes.h"
#include "MathUtility.h"

#include <vector>
#include <map>
#include <string>

namespace NexgenRedux
{
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
        void SetAnchor(const MathUtility::Vec3F value);
        const MathUtility::Vec3F GetRotation();
        void SetRotation(const MathUtility::Vec3F value);
        const MathUtility::Vec3F GetSkew();
        void SetSkew(const MathUtility::Vec3F value);
        const MathUtility::Vec3F GetScale();
        void SetScale(const MathUtility::Vec3F value);
        const MathUtility::Vec3F GetPosition();
        void SetPosition(const MathUtility::Vec3F value);
        const MathUtility::Matrix4x4 GetTransform();
        const MathUtility::Matrix4x4 GetInverseTransform();

    private:

        void CalculateTransforms();
        
    private:

        friend class NodeManager;
        friend class SceneManager;
        void MarkForDelete();
        bool MarkedForDelete();
        void SetParentID(uint32_t parentNodeID);
        bool GetAssigned();
        void SetAssigned();

    private:

        uint32_t m_nodeID;
        uint32_t m_parentNodeID;
        bool m_assigned;
        std::vector<uint32_t> m_childNodes;
        bool m_deleteFlag;

        bool m_transformIsDirty;
        MathUtility::Vec3F m_anchor;
        MathUtility::Vec3F m_rotation;
        MathUtility::Vec3F m_skew;
        MathUtility::Vec3F m_scale;
        MathUtility::Vec3F m_position;
        MathUtility::Matrix4x4 m_transform;
        MathUtility::Matrix4x4 m_inverseTransform;
    };
}