#include "PerspectiveCamera.h"
#include "RenderStateManager.h"

#include <Gensys/DebugUtility.h>
#include <Gensys/StringUtility.h>
#include <stdexcept>
#include <string>

using namespace Gensys;
using namespace NexgenRedux;

PerspectiveCamera::PerspectiveCamera(uint32_t nodeID) : Node(nodeID)
{
    m_clearColor = MathUtility::Color4F(0, 0, 0, 1);

    m_viewIsDirty = true;
    m_eye = MathUtility::Vec3F(0, 0, 100);
    m_target = MathUtility::Vec3F(0, 0, 0);
    m_up = MathUtility::Vec3F(0, 1, 0);

    m_projectionIsDirty = true;
    m_fov = 90;
    m_aspectRatio = 1;
    m_zNear = 1;
    m_zFar = 100;
}

PerspectiveCamera::~PerspectiveCamera(void)
{
}

void PerspectiveCamera::Update(float dt)
{
    if (m_viewIsDirty == true || m_projectionIsDirty == true)
    {
        if (m_viewIsDirty == true)
        {
            m_viewMatrix = MathUtility::Matrix4x4::LookAt(m_eye, m_target, m_up);
            m_viewIsDirty = false;
        }

        if (m_projectionIsDirty == true)
        {
            m_projectionMatrix = MathUtility::Matrix4x4::PerspectiveFov(m_fov, m_aspectRatio, m_zNear, m_zFar);
            m_projectionIsDirty = false;
        }

        m_inverseProjectionViewMatrix = MathUtility::Matrix4x4::Inverse(m_projectionMatrix * m_viewMatrix);
    }
}

void PerspectiveCamera::Render()
{
    RenderStateManager *renderStateManager = RenderStateManager::GetInstance();

    renderStateManager->Clear(m_clearColor);
    renderStateManager->SetModelMatrix(GetTransform());
    renderStateManager->SetViewMatrix(m_viewMatrix);
    renderStateManager->SetProjectionMatrix(m_projectionMatrix);

    renderStateManager->SetTint(MathUtility::Color4F(1, 1, 1, 1));
    renderStateManager->SetBlend(BlendOperationAdd);
    renderStateManager->SetBlendFactors(BlendFactorSrcAlpha, BlendFactorOneMinusSrcAlpha);
    renderStateManager->SetDrawMode(DrawModeOperationTriangles);
    renderStateManager->SetCulling(CullingOperationBack);
    renderStateManager->SetDepth(DepthOperationLess);
    renderStateManager->SetFog(FogOperationDisabled);
    renderStateManager->SetFogInstance(MathUtility::Color3F(0, 0, 0), 0, 0, 0);
    renderStateManager->SetLights(LightsOperationDisabled);
    renderStateManager->SetAmbientLight(MathUtility::Color3F(0.25f, 0.25f, 0.25f));
    renderStateManager->SetLight1(LightOperationDisabled);
    renderStateManager->SetLightInstance1(MathUtility::Vec3F(0, 0, 0), 0, MathUtility::Color4F(0, 0, 0, 0));
    renderStateManager->SetLight2(LightOperationDisabled);
    renderStateManager->SetLightInstance2(MathUtility::Vec3F(0, 0, 0), 0, MathUtility::Color4F(0, 0, 0, 0));
    renderStateManager->SetLight3(LightOperationDisabled);
    renderStateManager->SetLightInstance3(MathUtility::Vec3F(0, 0, 0), 0, MathUtility::Color4F(0, 0, 0, 0));
    renderStateManager->SetLight4(LightOperationDisabled);
    renderStateManager->SetLightInstance4(MathUtility::Vec3F(0, 0, 0), 0, MathUtility::Color4F(0, 0, 0, 0));

    uint32_t windowWidth;
    uint32_t windowHeight;
    if (WindowManager::GetInstance()->GetWindowSize(windowWidth, windowHeight) == true)
    {
        renderStateManager->SetViewport(MathUtility::RectI(0, 0, windowWidth, windowHeight));
    }

    renderStateManager->ApplyChanges();
}

const MathUtility::Color4F PerspectiveCamera::GetClearColor()
{
    return m_clearColor;
}

void PerspectiveCamera::SetClearColor(const MathUtility::Color4F value)
{
    m_clearColor = value;
}

const MathUtility::Vec3F PerspectiveCamera::GetEye()
{
    return m_eye;
}

void PerspectiveCamera::SetEye(const MathUtility::Vec3F value)
{
    if (m_eye == value) 
    {
        return;
    }
    m_eye = value;
    m_viewIsDirty = true;
}

const MathUtility::Vec3F PerspectiveCamera::GetTarget()
{
    return m_target;
}

void PerspectiveCamera::SetTarget(const MathUtility::Vec3F value)
{
    if (m_target == value) 
    {
        return;
    }
    m_target = value;
    m_viewIsDirty = true;
}

const MathUtility::Vec3F PerspectiveCamera::GetUp()
{
    return m_up;
}

void PerspectiveCamera::SetUp(const MathUtility::Vec3F value)
{
    if (m_up == value) 
    {
        return;
    }
    m_up = value;
    m_viewIsDirty = true;
}

const float PerspectiveCamera::GetFOV()
{
    return m_fov;
}

void PerspectiveCamera::SetFOV(const float value)
{
    if (abs(m_fov - value) <= 1e-6f) 
    {
        return;
    }
    m_fov = value;
    m_projectionIsDirty = true;
}

const float PerspectiveCamera::GetAspectRatio()
{
    return m_aspectRatio;
}

void PerspectiveCamera::SetAspectRatio(const float value)
{
    if (abs(m_aspectRatio - value) <= 1e-6f) 
    {
        return;
    }
    m_aspectRatio = value;
    m_projectionIsDirty = true;
}

const float PerspectiveCamera::GetZNear()
{
    return m_zNear;
}

void PerspectiveCamera::SetZNear(const float value)
{
    if (abs(m_zNear - value) <= 1e-6f) 
    {
        return;
    }
    m_zNear = value;
    m_projectionIsDirty = true;
}

const float PerspectiveCamera::GetZFar()
{
    return m_zFar;
}

void PerspectiveCamera::SetZFar(const float value)
{
    if (abs(m_zFar - value) <= 1e-6f) 
    {
        return;
    }
    m_zFar = value;
    m_projectionIsDirty = true;
}

// Private

// MathUtility::Vec3F PerspectiveCamera::Unproject(const MathUtility::Vec3F screenCoord)
// {
//     MathUtility::Vec4F viewport = MathUtility::Vec4F(0, 0, m_right, m_top); 

//     MathUtility::Vec4F temp = MathUtility::Vec4F();
//     temp.x = (((screenCoord.x - viewport.values[0]) / viewport.values[2]) * 2) - 1;
//     temp.y = (((screenCoord.y - viewport.values[1]) / viewport.values[3]) * 2) - 1;
//     temp.z = screenCoord.z;
//     temp.w = 1;

//     MathUtility::Vec4F result = MathUtility::Matrix4x4::MultiplyMatrixByVec4F(m_inverseProjectionViewMatrix, temp);
//     return MathUtility::Vec3F(result.x / result.w, result.y / result.w, result.z / result.w);
// }

// bool PerspectiveCamera::TestRayIntersectsObb(const MathUtility::Vec2F screenCoord, const MathUtility::Vec3F aabb_min, const MathUtility::Vec3F aabb_max, const MathUtility::Matrix4x4 modelMatrix)
// {
//     MathUtility::Vec3F rayOrigin = MathUtility::Vec3F(screenCoord.x, screenCoord.y, 0.0f);
//     MathUtility::Vec3F rayDirection = Unproject(MathUtility::Vec3F(screenCoord.x, screenCoord.y, 1.0f) - rayOrigin).Normal();

//     MathUtility::Vec3F obbPositionWorld = MathUtility::Matrix4x4::GetPosition(modelMatrix);
//     MathUtility::Vec3F delta = obbPositionWorld - rayOrigin;
    
//     float tMin = 0.0f;
//     float tMax = 100000.0f;

//     for (uint32_t i = 0; i <= 2; i++)
//     {
//         MathUtility::Vec3F axis = MathUtility::Vec3F(modelMatrix.values[(i * 4)], modelMatrix.values[(i * 4) + 1], modelMatrix.values[(i * 4) + 2]);
        
//         float e = axis.Dot(delta);
//         float f = rayDirection.Dot(axis);
//         if (fabs(f) > 0.001f)
//         {
//             float t1 = (e + aabb_min.values[i]) / f;
//             float t2 = (e + aabb_max.values[i]) / f;
//             if (t1 > t2)
//             {
//                 float temp = t1;
//                 t1 = t2;
//                 t2 = temp;
//             }
//             if (t2 < tMax)
//             {
//                 tMax = t2;
//             }
//             if (t1 > tMin)
//             {
//                 tMin = t1;
//             }
//             if (tMin > tMax)
//             {
//                 return false;
//             }
//         } 
//         else if (-e + aabb_min.values[i] > 0.0f || -e + aabb_max.values[i] < 0.0f) 
//         {
//             return false;
//         }
//     }
//     return true;
// }