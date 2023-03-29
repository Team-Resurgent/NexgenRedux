#include "Fog.h"
#include "RenderStateManager.h"

#include <Gensys/DebugUtility.h>
#include <Gensys/StringUtility.h>
#include <stdexcept>
#include <string>

using namespace Gensys;
using namespace NexgenRedux;

Fog::Fog(uint32_t nodeID) : Node(nodeID)
{
    m_fogColor = MathUtility::Color3F(0, 0, 0);
    m_fogStart = 0;
    m_fogEnd = 0;
    m_fogDensity = 0;
}

Fog::~Fog(void)
{
}

void Fog::Update(float dt)
{
}

void Fog::Render()
{
    RenderStateManager* renderStateManager = RenderStateManager::GetInstance();

    renderStateManager->SetFog(m_fogOperation);
    renderStateManager->SetFogInstance(m_fogColor, m_fogStart, m_fogEnd, m_fogDensity);
    renderStateManager->ApplyChanges();
}

const FogOperation Fog::GetFog()
{
    return m_fogOperation;
}

void Fog::SetFog(const FogOperation value)
{
    m_fogOperation = value;
}

const MathUtility::Color3F Fog::GetFogColor()
{
    return m_fogColor;
}

void Fog::SetFogColor(const MathUtility::Color3F value)
{
    m_fogColor = value;
}

const float Fog::GetFogStart()
{
    return m_fogStart;
}

void Fog::SetFogStart(const float value)
{
    m_fogStart = value;
}

const float Fog::GetFogEnd()
{
    return m_fogEnd;
}

void Fog::SetFogEnd(const float value)
{
    m_fogEnd = value;
}

const float Fog::GetFogDensity()
{
    return m_fogDensity;
}

void Fog::SetFogDensity(const float value)
{
    m_fogDensity = value;
}