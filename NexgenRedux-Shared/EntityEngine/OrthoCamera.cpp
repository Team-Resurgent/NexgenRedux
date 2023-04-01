#include "OrthoCamera.h"
#include "RenderStateManager.h"

#include <Gensys/DebugUtility.h>
#include <Gensys/StringUtility.h>
#include <stdexcept>
#include <string>

using namespace Gensys;
using namespace NexgenRedux;

OrthoCamera::OrthoCamera(uint32_t nodeID) : Node(nodeID)
{
    m_clearColor = MathUtility::Color4F(0, 0, 0, 1);

    m_viewIsDirty = true;
    m_eye = MathUtility::Vec3F(0, 0, 100);
    m_target = MathUtility::Vec3F(0, 0, 0);
    m_up = MathUtility::Vec3F(0, 1, 0);

    m_projectionIsDirty = true;
    m_left = 0;
    m_right = 1;
    m_bottom = 0;
    m_top = 1;
    m_zNear = 1;
    m_zFar = 100;
}

OrthoCamera::~OrthoCamera(void)
{
    DebugUtility::LogMessage(DebugUtility::LOGLEVEL_INFO, StringUtility::FormatString("Deleting node '%i'", GetID()));
}

void OrthoCamera::Update(float dt)
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
            m_projectionMatrix = MathUtility::Matrix4x4::OrthoOffCenter(m_left, m_right, m_bottom, m_top, m_zNear, m_zFar);
            m_projectionIsDirty = false;
        }

        m_inverseProjectionViewMatrix = MathUtility::Matrix4x4::Inverse(m_projectionMatrix * m_viewMatrix);
    }
}

void OrthoCamera::Render()
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

const MathUtility::Color4F OrthoCamera::GetClearColor()
{
    return m_clearColor;
}

void OrthoCamera::SetClearColor(const MathUtility::Color4F value)
{
    m_clearColor = value;
}

const MathUtility::Vec3F OrthoCamera::GetEye()
{
    return m_eye;
}

void OrthoCamera::SetEye(const MathUtility::Vec3F value)
{
    if (m_eye == value) 
    {
        return;
    }
    m_eye = value;
    m_viewIsDirty = true;
}

const MathUtility::Vec3F OrthoCamera::GetTarget()
{
    return m_target;
}

void OrthoCamera::SetTarget(const MathUtility::Vec3F value)
{
    if (m_target == value) 
    {
        return;
    }
    m_target = value;
    m_viewIsDirty = true;
}

const MathUtility::Vec3F OrthoCamera::GetUp()
{
    return m_up;
}

void OrthoCamera::SetUp(const MathUtility::Vec3F value)
{
    if (m_up == value) 
    {
        return;
    }
    m_up = value;
    m_viewIsDirty = true;
}

const float OrthoCamera::GetLeft()
{
    return m_left;
}

void OrthoCamera::SetLeft(const float value)
{
    if (abs(m_left - value) <= 1e-6f) 
    {
        return;
    }
    m_left = value;
    m_projectionIsDirty = true;
}

const float OrthoCamera::GetRight()
{
    return m_right;
}

void OrthoCamera::SetRight(const float value)
{
    if (abs(m_right - value) > 1e-6f) 
    {
        m_right = value;
        m_projectionIsDirty = true;
    }
}

const float OrthoCamera::GetBottom()
{
    return m_bottom;
}

void OrthoCamera::SetBottom(const float value)
{
    if (abs(m_bottom - value) <= 1e-6f) 
    {
        return;
    }
    m_bottom = value;
    m_projectionIsDirty = true;
}

const float OrthoCamera::GetTop()
{
    return m_top;
}

void OrthoCamera::SetTop(const float value)
{
    if (abs(m_top - value) <= 1e-6f) 
    {
        return;
    }
    m_top = value;
    m_projectionIsDirty = true;
}

const float OrthoCamera::GetZNear()
{
    return m_zNear;
}

void OrthoCamera::SetZNear(const float value)
{
    if (abs(m_zNear - value) <= 1e-6f) 
    {
        return;
    }
    m_zNear = value;
    m_projectionIsDirty = true;
}

const float OrthoCamera::GetZFar()
{
    return m_zFar;
}

void OrthoCamera::SetZFar(const float value)
{
    if (abs(m_zFar - value) <= 1e-6f) 
    {
        return;
    }
    m_zFar = value;
    m_projectionIsDirty = true;
}

// Private

MathUtility::Vec3F OrthoCamera::Unproject(const MathUtility::Vec3F screenCoord)
{
    MathUtility::Vec4F viewport = MathUtility::Vec4F(0, 0, m_right, m_top); 

    MathUtility::Vec4F temp = MathUtility::Vec4F();
    temp.x = (((screenCoord.x - viewport.values[0]) / viewport.values[2]) * 2) - 1;
    temp.y = (((screenCoord.y - viewport.values[1]) / viewport.values[3]) * 2) - 1;
    temp.z = screenCoord.z;
    temp.w = 1;

    MathUtility::Vec4F result = MathUtility::Matrix4x4::MultiplyMatrixByVec4F(m_inverseProjectionViewMatrix, temp);
    return MathUtility::Vec3F(result.x / result.w, result.y / result.w, result.z / result.w);
}

bool OrthoCamera::TestRayIntersectsObb(const MathUtility::Vec2F screenCoord, const MathUtility::Vec3F aabb_min, const MathUtility::Vec3F aabb_max, const MathUtility::Matrix4x4 modelMatrix)
{
    MathUtility::Vec3F rayOrigin = MathUtility::Vec3F(screenCoord.x, screenCoord.y, 0.0f);
    MathUtility::Vec3F rayDirection = Unproject(MathUtility::Vec3F(screenCoord.x, screenCoord.y, 1.0f) - rayOrigin).Normal();

    MathUtility::Vec3F obbPositionWorld = MathUtility::Matrix4x4::GetPosition(modelMatrix);
    MathUtility::Vec3F delta = obbPositionWorld - rayOrigin;
    
    float tMin = 0.0f;
    float tMax = 100000.0f;

    for (uint32_t i = 0; i <= 2; i++)
    {
        MathUtility::Vec3F axis = MathUtility::Vec3F(modelMatrix.values[(i * 4)], modelMatrix.values[(i * 4) + 1], modelMatrix.values[(i * 4) + 2]);
        
        float e = axis.Dot(delta);
        float f = rayDirection.Dot(axis);
        if (fabs(f) > 0.001f)
        {
            float t1 = (e + aabb_min.values[i]) / f;
            float t2 = (e + aabb_max.values[i]) / f;
            if (t1 > t2)
            {
                float temp = t1;
                t1 = t2;
                t2 = temp;
            }
            if (t2 < tMax)
            {
                tMax = t2;
            }
            if (t1 > tMin)
            {
                tMin = t1;
            }
            if (tMin > tMax)
            {
                return false;
            }
        } 
        else if (-e + aabb_min.values[i] > 0.0f || -e + aabb_max.values[i] < 0.0f) 
        {
            return false;
        }
    }
    return true;
}