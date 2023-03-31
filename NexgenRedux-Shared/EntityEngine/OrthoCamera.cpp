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

bool OrthoCamera::IsScreenPointInRect(const float& screenPosX, const float& screenPosY, const MathUtility::Matrix4x4 w2l, const MathUtility::RectF rect)
{
    if (rect.width <= 0 || rect.height <= 0)
    {
        return false;
    }
    
    // first, convert pt to near/far plane, get Pn and Pf
    MathUtility::Vec3F Pn = MathUtility::Vec3F(screenPosX, screenPosY, -1);
    MathUtility::Vec3F Pf = MathUtility::Vec3F(screenPosX, screenPosY, 1);
    Pn = Unproject(Pn);
    Pf = Unproject(Pf);
    
    //  then convert Pn and Pf to node space

    Pn = MathUtility::Matrix4x4::TransformVec3F(w2l, Pn);
    Pf = MathUtility::Matrix4x4::TransformVec3F(w2l, Pf);

    // Pn and Pf define a line Q(t) = D + t * E which D = Pn
    auto E = Pf - Pn;
    
    // second, get three points which define content plane
    //  these points define a plane P(u, w) = A + uB + wC
    MathUtility::Vec3F A = MathUtility::Vec3F(rect.x, rect.y, 0);
    MathUtility::Vec3F B = MathUtility::Vec3F(rect.x + rect.width, rect.y, 0);
    MathUtility::Vec3F C = MathUtility::Vec3F(rect.x, rect.y + rect.height, 0);
    B = B - A;
    C = C - A;
    
    //  the line Q(t) intercept with plane P(u, w)
    //  calculate the intercept point P = Q(t)
    //      (BxC).A - (BxC).D
    //  t = -----------------
    //          (BxC).E
    MathUtility::Vec3F BxC = B.Cross(C);
    auto BxCdotE = BxC.Dot(E);
    if (BxCdotE == 0) 
    {
        return false;
    }
    float t = (BxC.Dot(A) - BxC.Dot(Pn)) / BxCdotE;
    MathUtility::Vec3F P = Pn + MathUtility::Vec3F(E.x * t, E.y * t, E.z * t);
    return rect.ContainsPoint(P.x, P.y);
}

// Private

MathUtility::Vec3F OrthoCamera::Unproject(const MathUtility::Vec3F source)
{
    uint32_t windowWidth;
    uint32_t windowHeight;
    if (WindowManager::GetInstance()->GetWindowSize(windowWidth, windowHeight) == false)
    {
        DebugUtility::LogMessage(DebugUtility::LOGLEVEL_INFO, "Unproject: Unable to get window size.");
        return source;
    }

    MathUtility::Vec3F screen = MathUtility::Vec3F(source.x / windowWidth, source.y / windowHeight, source.z);
    screen.x = screen.x * 2.0f - 1.0f;
    screen.y = screen.y * 2.0f - 1.0f;
    screen.z = screen.z * 2.0f - 1.0f;

    return MathUtility::Matrix4x4::TransformVec3F(m_inverseProjectionViewMatrix, screen);
}