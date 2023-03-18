#pragma once

#include "Property.h"
#include "Node.h"
#include "MathUtility.h"

namespace NexgenRedux
{
    class Node;
    
    class PropertyTransform : public Property
    {
    public:

        PropertyTransform(Node* node);

        const MathUtility::Vec3F GetPosition();
        void SetPosition(MathUtility::Vec3F value);

        const MathUtility::Vec3F GetAnchor();
        void SetAnchor(MathUtility::Vec3F value);

        const MathUtility::Vec3F GetRotation();
        void SetRotation(MathUtility::Vec3F value);

        const MathUtility::Vec3F GetSkew();
        void SetSkew(MathUtility::Vec3F value);

        const MathUtility::Matrix4x4 GetTransform();

    private:

        Node* m_node;

        MathUtility::Vec3F m_position;
        MathUtility::Vec3F m_anchor;
        MathUtility::Vec3F m_rotation;
        MathUtility::Vec3F m_skew;

        bool m_matrixDirty;
        MathUtility::Matrix4x4 m_matrix;

    };
}
