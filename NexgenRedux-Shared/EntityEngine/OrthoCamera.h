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
        void SetClearColor(const MathUtility::Color4F value);

        const MathUtility::Vec3F GetEye();
        void SetEye(const MathUtility::Vec3F value);
        const MathUtility::Vec3F GetTarget();
        void SetTarget(const MathUtility::Vec3F value);
        const MathUtility::Vec3F GetUp();
        void SetUp(const MathUtility::Vec3F value);

        const float GetLeft();
        void SetLeft(const float value);
        const float GetRight();
        void SetRight(const float value);
        const float GetBottom();
        void SetBottom(const float value);
        const float GetTop();
        void SetTop(const float value);
        const float GetZNear();
        void SetZNear(const float value);
        const float GetZFar();
        void SetZFar(const float value);
        
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