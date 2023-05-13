#pragma once

#include "Node.h"
#include "GlobalTypes.h"
#include "MeshUtility.h"
#include "OrthoCamera.h"

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

        const bool GetVisible();
        void SetVisible(const bool value);

        const std::string GetTexturePath();
        void SetTexturePath(const std::string value);
        const MathUtility::RectF GetUV();
        void SetUV(const MathUtility::RectF value);
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
        const CullingOperation GetCulling();
        void SetCulling(const CullingOperation value);
        const DepthOperation GetDepth();
        void SetDepth(const DepthOperation value);

        bool HitTest(const double screenPosX, const double screenPosY, OrthoCamera* camera);

    private:

        bool m_visible;

        bool m_textureIsDirty;
        std::string m_texturePath;
        uint32_t m_textureID;

        bool m_wantsTexture;
        uint32_t m_width;
        uint32_t m_height;
        uint8_t* m_data;

        bool m_meshIsDirty;
        MathUtility::RectF m_uv;
        MathUtility::SizeF m_size;
        std::vector<MeshUtility::Vertex> m_mesh;

        MathUtility::Color4F m_tint;
        BlendOperation m_blendOperation;
        BlendFactor m_blendFactorSrc;
        BlendFactor m_blendFactorDst;
        DrawModeOperation m_drawModeOperation;
        CullingOperation m_cullingOperation;
        DepthOperation m_depthOperation;
    };
}