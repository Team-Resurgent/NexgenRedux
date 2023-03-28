#pragma once

#include "Node.h"
#include "GlobalTypes.h"

#include <vector>
#include <map>
#include <string>

namespace NexgenRedux
{
    class OrthoCamera : public Node
    {
    public:
        OrthoCamera(uint32_t nodeID);
        ~OrthoCamera(void);

        void Update(float dt);
        void Render();

        const MathUtility::Color4F GetClearColor();
        void SetClearColor(MathUtility::Color4F value);

        const MathUtility::Vec3F GetEye();
        void SetEye(MathUtility::Vec3F value);
        const MathUtility::Vec3F GetTarget();
        void SetTarget(MathUtility::Vec3F value);
        const MathUtility::Vec3F GetUp();
        void SetUp(MathUtility::Vec3F value);

        const float GetLeft();
        void SetLeft(float value);
        const float GetRight();
        void SetRight(float value);
        const float GetBottom();
        void SetBottom(float value);
        const float GetTop();
        void SetTop(float value);
        const float GetZNear();
        void SetZNear(float value);
        const float GetZFar();
        void SetZFar(float value);
        
    private:

        MathUtility::Color4F m_clearColor;

        MathUtility::Vec3F m_eye;
        MathUtility::Vec3F m_target;
        MathUtility::Vec3F m_up;
        bool m_viewIsDirty;

        float m_left;
        float m_right;
        float m_bottom;
        float m_top;
        float m_zNear;
        float m_zFar;
        bool m_projectionIsDirty;

        MathUtility::Matrix4x4 m_viewMatrix;
        MathUtility::Matrix4x4 m_projectionMatrix;
    };
}