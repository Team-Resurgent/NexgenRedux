#pragma once

#include "Node.h"
#include "GlobalTypes.h"
#include "MeshUtility.h"

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
        void SetTexturePath(const std::string value);
        const MathUtility::RectF GetUV();
        void SetUV(const MathUtility::RectF value);
        const MathUtility::Vec3F GetPosition();
        void SetPosition(const MathUtility::Vec3F value);
        const MathUtility::SizeF GetSize();
        void SetSize(const MathUtility::SizeF value);
        const MathUtility::Color4F GetTint();
        void SetTint(const MathUtility::Color4F value);
        const BlendOperation GetBlend();
        void SetBlend(const BlendOperation value);
        const BlendFactor GetBlendFactorSrc();
        void SetBlendFactorSrc(const BlendFactor value);
        const BlendFactor GetBlendFactorDst();
        void SetBlendFactorDst(const BlendFactor value);
        const DrawModeOperation GetDrawMode();
        void SetDrawMode(const DrawModeOperation value);

    private:

        bool m_textureIsDirty;
        std::string m_texturePath;
        uint32_t m_textureID;

        bool m_meshIsDirty;
        MathUtility::RectF m_uv;
        MathUtility::Vec3F m_position;
        MathUtility::SizeF m_size;
        std::vector<MeshUtility::Vertex> m_mesh;

        MathUtility::Color4F m_tint;
        BlendOperation m_blendOperation;
        BlendFactor m_blendFactorSrc;
        BlendFactor m_blendFactorDst;
        DrawModeOperation m_drawMode;
    };
}