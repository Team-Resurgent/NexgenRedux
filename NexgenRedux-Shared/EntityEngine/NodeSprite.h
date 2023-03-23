#pragma once

#include "Node.h"
#include "GlobalTypes.h"

#include <vector>
#include <map>
#include <string>

namespace NexgenRedux
{
    class NodeSprite : public Node
    {
    public:
        NodeSprite(uint32_t nodeID);
        ~NodeSprite(void);

        void Update(float dt);
        void Render();

        const std::string GetTexturePath();
        void SetTexturePath(std::string value);
        const MathUtility::RectF GetUV();
        void SetUV(MathUtility::RectF value);

    private:

        bool m_isDirty;
        std::string m_texturePath;
        MathUtility::RectF m_uv;
    };
}