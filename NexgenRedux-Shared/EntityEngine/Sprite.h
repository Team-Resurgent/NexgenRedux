#pragma once

#include "Node.h"
#include "GlobalTypes.h"

#include <vector>
#include <map>
#include <string>

namespace NexgenRedux
{
    class Sprite : public Node
    {
    public:
        Sprite(uint32_t nodeID);
        ~Sprite(void);

        void Update(float dt);
        void Render();

        const std::string GetTexturePath();
        void SetTexturePath(std::string value);
        const MathUtility::RectF GetUV();
        void SetUV(MathUtility::RectF value);

    private:

        bool m_textureIsDirty;
        std::string m_texturePath;
        uint32_t m_textureID;

        bool m_meshIsDirty;
        MathUtility::RectF m_uv;
        uint32_t m_meshID;
    };
}