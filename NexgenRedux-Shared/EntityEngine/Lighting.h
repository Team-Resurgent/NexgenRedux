#pragma once

#include "Node.h"
#include "GlobalTypes.h"
#include "MeshUtility.h"

#include <vector>
#include <map>
#include <string>

namespace NexgenRedux
{
    class Lighting : public Node
    {
    public:
        Lighting(uint32_t nodeID);
        ~Lighting(void);

        void Update(float dt);
        void Render();

        const LightsOperation GetLights();
        void SetLights(const LightsOperation value);
        const MathUtility::Color3F GetAmbientLight();
        void SetAmbientLight(const MathUtility::Color3F value);

        const LightOperation GetLight1();
        void SetLight1(const LightOperation value);
        const MathUtility::Vec3F GetLight1Position();
        void SetLight1Position(const MathUtility::Vec3F value);
        const float GetLight1Distance();
        void SetLight1Distance(const float value);
        const MathUtility::Color4F GetLight1Diffuse();
        void SetLight1Diffuse(const MathUtility::Color4F value);

        const LightOperation GetLight2();
        void SetLight2(const LightOperation value);
        const MathUtility::Vec3F GetLight2Position();
        void SetLight2Position(const MathUtility::Vec3F value);
        const float GetLight2Distance();
        void SetLight2Distance(const float value);
        const MathUtility::Color4F GetLight2Diffuse();
        void SetLight2Diffuse(const MathUtility::Color4F value);

        const LightOperation GetLight3();
        void SetLight3(const LightOperation value);
        const MathUtility::Vec3F GetLight3Position();
        void SetLight3Position(const MathUtility::Vec3F value);
        const float GetLight3Distance();
        void SetLight3Distance(const float value);
        const MathUtility::Color4F GetLight3Diffuse();
        void SetLight3Diffuse(const MathUtility::Color4F value);

        const LightOperation GetLight4();
        void SetLight4(const LightOperation value);
        const MathUtility::Vec3F GetLight4Position();
        void SetLight4Position(const MathUtility::Vec3F value);
        const float GetLight4Distance();
        void SetLight4Distance(const float value);
        const MathUtility::Color4F GetLight4Diffuse();
        void SetLight4Diffuse(const MathUtility::Color4F value);

    private:

        LightsOperation m_lightsOperation;
        MathUtility::Color3F m_ambientLight;

        LightOperation m_light1Operation;
        MathUtility::Vec3F m_light1Position;
        float m_light1Distance;
        MathUtility::Color4F m_light1Diffuse;

        LightOperation m_light2Operation;
        MathUtility::Vec3F m_light2Position;
        float m_light2Distance;
        MathUtility::Color4F m_light2Diffuse;

        LightOperation m_light3Operation;
        MathUtility::Vec3F m_light3Position;
        float m_light3Distance;
        MathUtility::Color4F m_light3Diffuse;

        LightOperation m_light4Operation;
        MathUtility::Vec3F m_light4Position;
        float m_light4Distance;
        MathUtility::Color4F m_light4Diffuse;
    };
}