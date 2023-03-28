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
}

void OrthoCamera::Render()
{
    RenderStateManager *renderStateManager = RenderStateManager::GetInstance();
    renderStateManager->Clear(m_clearColor);
    renderStateManager->SetModelMatrix(GetTransform());
    renderStateManager->SetViewMatrix(m_viewMatrix);
    renderStateManager->SetProjectionMatrix(m_projectionMatrix);
}

const MathUtility::Color4F OrthoCamera::GetClearColor()
{
    return m_clearColor;
}

void OrthoCamera::SetClearColor(MathUtility::Color4F value)
{
    m_clearColor = value;
}

const MathUtility::Vec3F OrthoCamera::GetEye()
{
    return m_eye;
}

void OrthoCamera::SetEye(MathUtility::Vec3F value)
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

void OrthoCamera::SetTarget(MathUtility::Vec3F value)
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

void OrthoCamera::SetUp(MathUtility::Vec3F value)
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

void OrthoCamera::SetLeft(float value)
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

void OrthoCamera::SetRight(float value)
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

void OrthoCamera::SetBottom(float value)
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

void OrthoCamera::SetTop(float value)
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

void OrthoCamera::SetZNear(float value)
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

void OrthoCamera::SetZFar(float value)
{
    if (abs(m_zFar - value) <= 1e-6f) 
    {
        return;
    }
    m_zFar = value;
    m_projectionIsDirty = true;
}