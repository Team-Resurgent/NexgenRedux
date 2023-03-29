#include "Lighting.h"
#include "RenderStateManager.h"

#include <Gensys/DebugUtility.h>
#include <Gensys/StringUtility.h>
#include <stdexcept>
#include <string>

using namespace Gensys;
using namespace NexgenRedux;

Lighting::Lighting(uint32_t nodeID) : Node(nodeID)
{
    m_lightsOperation = LightsOperationDisabled;
    m_ambientLight = MathUtility::Color3F(0.25f, 0.25f, 0.25f);

    m_light1Operation = LightOperationDisabled;
    m_light1Position = MathUtility::Vec3F(0, 0, 0);
    m_light1Distance = 0;
    m_light1Diffuse = MathUtility::Color4F(0, 0, 0, 0);
    m_light2Operation = LightOperationDisabled;
    m_light2Position = MathUtility::Vec3F(0, 0, 0);
    m_light2Distance = 0;
    m_light2Diffuse = MathUtility::Color4F(0, 0, 0, 0);
    m_light3Operation = LightOperationDisabled;
    m_light3Position = MathUtility::Vec3F(0, 0, 0);
    m_light3Distance = 0;
    m_light3Diffuse = MathUtility::Color4F(0, 0, 0, 0);
    m_light4Operation = LightOperationDisabled;
    m_light4Position = MathUtility::Vec3F(0, 0, 0);
    m_light4Distance = 0;
    m_light4Diffuse = MathUtility::Color4F(0, 0, 0, 0);
}

Lighting::~Lighting(void)
{
}

void Lighting::Update(float dt)
{
}

void Lighting::Render()
{
    RenderStateManager* renderStateManager = RenderStateManager::GetInstance();

    renderStateManager->SetLights(m_lightsOperation);
    renderStateManager->SetAmbientLight(m_ambientLight);
    renderStateManager->SetLight1(m_light1Operation);
    renderStateManager->SetLightInstance1(m_light1Position, m_light1Distance, m_light1Diffuse);
    renderStateManager->SetLight2(m_light2Operation);
    renderStateManager->SetLightInstance2(m_light2Position, m_light2Distance, m_light2Diffuse);
    renderStateManager->SetLight3(m_light3Operation);
    renderStateManager->SetLightInstance3(m_light3Position, m_light3Distance, m_light3Diffuse);
    renderStateManager->SetLight4(m_light4Operation);
    renderStateManager->SetLightInstance4(m_light4Position, m_light4Distance, m_light4Diffuse);
    renderStateManager->ApplyChanges();
}

const LightsOperation Lighting::GetLights()
{
    return m_lightsOperation;
}

void Lighting::SetLights(const LightsOperation value)
{
    m_lightsOperation = value;
}

const MathUtility::Color3F Lighting::GetAmbientLight()
{
    return m_ambientLight;
}

void Lighting::SetAmbientLight(const MathUtility::Color3F value)
{
    m_ambientLight = value;
}

const LightOperation Lighting::GetLight1()
{
    return m_light1Operation;
}

void Lighting::SetLight1(const LightOperation value)
{
    m_light1Operation = value;
}

const MathUtility::Vec3F Lighting::GetLight1Position()
{
    return m_light1Position;
}

void Lighting::SetLight1Position(const MathUtility::Vec3F value)
{
    m_light1Position = value;
}

const float Lighting::GetLight1Distance()
{
    return m_light1Distance;
}

void Lighting::SetLight1Distance(const float value)
{
    m_light1Distance = value;
}

const MathUtility::Color4F Lighting::GetLight1Diffuse()
{
    return m_light1Diffuse;
}

void Lighting::SetLight1Diffuse(const MathUtility::Color4F value)
{
    m_light1Diffuse = value;
}

const LightOperation Lighting::GetLight2()
{
    return m_light2Operation;
}

void Lighting::SetLight2(const LightOperation value)
{
    m_light2Operation = value;
}

const MathUtility::Vec3F Lighting::GetLight2Position()
{
    return m_light2Position;
}

void Lighting::SetLight2Position(const MathUtility::Vec3F value)
{
    m_light2Position = value;
}

const float Lighting::GetLight2Distance()
{
    return m_light2Distance;
}

void Lighting::SetLight2Distance(const float value)
{
    m_light2Distance = value;
}

const MathUtility::Color4F Lighting::GetLight2Diffuse()
{
    return m_light2Diffuse;
}

void Lighting::SetLight2Diffuse(const MathUtility::Color4F value)
{
    m_light2Diffuse = value;
}

const LightOperation Lighting::GetLight3()
{
    return m_light3Operation;
}

void Lighting::SetLight3(const LightOperation value)
{
    m_light3Operation = value;
}

const MathUtility::Vec3F Lighting::GetLight3Position()
{
    return m_light3Position;
}

void Lighting::SetLight3Position(const MathUtility::Vec3F value)
{
    m_light3Position = value;
}

const float Lighting::GetLight3Distance()
{
    return m_light3Distance;
}

void Lighting::SetLight3Distance(const float value)
{
    m_light3Distance = value;
}

const MathUtility::Color4F Lighting::GetLight3Diffuse()
{
    return m_light3Diffuse;
}

void Lighting::SetLight3Diffuse(const MathUtility::Color4F value)
{
    m_light3Diffuse = value;
}

const LightOperation Lighting::GetLight4()
{
    return m_light4Operation;
}

void Lighting::SetLight4(const LightOperation value)
{
    m_light4Operation = value;
}

const MathUtility::Vec3F Lighting::GetLight4Position()
{
    return m_light4Position;
}

void Lighting::SetLight4Position(const MathUtility::Vec3F value)
{
    m_light4Position = value;
}

const float Lighting::GetLight4Distance()
{
    return m_light4Distance;
}

void Lighting::SetLight4Distance(const float value)
{
    m_light4Distance = value;
}

const MathUtility::Color4F Lighting::GetLight4Diffuse()
{
    return m_light4Diffuse;
}

void Lighting::SetLight4Diffuse(const MathUtility::Color4F value)
{
    m_light4Diffuse = value;
}