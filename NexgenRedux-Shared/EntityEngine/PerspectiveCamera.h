#pragma once

#include "Node.h"
#include "GlobalTypes.h"

#include <vector>
#include <map>
#include <string>

namespace NexgenRedux
{
    class PerspectiveCamera : public Node
    {
    public:
        PerspectiveCamera(uint32_t nodeID);
        ~PerspectiveCamera(void);

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
        const float GetFOV();
        void SetFOV(const float value);
        const float GetAspectRatio();
        void SetAspectRatio(const float value);


        const float GetZNear();
        void SetZNear(const float value);
        const float GetZFar();
        void SetZFar(const float value);
        
        //bool TestRayIntersectsObb(const MathUtility::Vec2F screenCoord, const MathUtility::Vec3F aabb_min, const MathUtility::Vec3F aabb_max, const MathUtility::Matrix4x4 modelMatrix);
        
    private:

        //MathUtility::Vec3F Unproject(const MathUtility::Vec3F screenCoord);

    private:

        MathUtility::Color4F m_clearColor;

        MathUtility::Vec3F m_eye;
        MathUtility::Vec3F m_target;
        MathUtility::Vec3F m_up;
        bool m_viewIsDirty;

        float m_fov;
        float m_aspectRatio;
        float m_zNear;
        float m_zFar;
        bool m_projectionIsDirty;

        MathUtility::Matrix4x4 m_viewMatrix;
        MathUtility::Matrix4x4 m_projectionMatrix;
        MathUtility::Matrix4x4 m_inverseProjectionViewMatrix;
    };
}