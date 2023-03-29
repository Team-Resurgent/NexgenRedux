#pragma once

#include "Node.h"
#include "GlobalTypes.h"
#include "MeshUtility.h"

#include <vector>
#include <map>
#include <string>

namespace NexgenRedux
{
    class Fog : public Node
    {
    public:
        Fog(uint32_t nodeID);
        ~Fog(void);

        void Update(float dt);
        void Render();

        const FogOperation GetFog();
        void SetFog(const FogOperation value);
        const MathUtility::Color3F GetFogColor();
        void SetFogColor(const MathUtility::Color3F value);
        const float GetFogStart();
        void SetFogStart(const float value);
        const float GetFogEnd();
        void SetFogEnd(const float value);
        const float GetFogDensity();
        void SetFogDensity(const float value);

    private:

        FogOperation m_fogOperation;
        MathUtility::Color3F m_fogColor;
        float m_fogStart;
        float m_fogEnd;
        float m_fogDensity;
    };
}