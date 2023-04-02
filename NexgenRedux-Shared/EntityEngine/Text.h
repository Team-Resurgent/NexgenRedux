#pragma once

#include "Node.h"
#include "GlobalTypes.h"
#include "MeshUtility.h"
#include "OrthoCamera.h"

//#include <SSFN/ssfn.h>

#include <vector>
#include <map>
#include <string>

//https://github.com/Neptune650/scalable-font2/blob/6dbbd7be932a6be21b4e7439ed7c09032acddcb3/docs/API.md
//https://github.com/Team-Resurgent/XBlastOS/blob/5149a2d18032306d4f6e4b0b3a9c0948103f1699/drivers/video/BootVideoHelpers.c
//https://github.com/Team-Resurgent/XBlastOS/blob/5149a2d18032306d4f6e4b0b3a9c0948103f1699/drivers/video/BootVideoHelpers.c

namespace NexgenRedux
{
    class Text : public Node
    {
    public:
        Text(uint32_t nodeID);
        ~Text(void);

        void Update(float dt);
        void Render();

        const std::string GetFontPath();
        void SetFontPath(const std::string value);
        const std::string GetText();
        void SetText(std::string value);
        const uint32_t GetFontSize();
        void SetFontSize(const uint32_t value);

        //family, style, size, family name

        // const MathUtility::SizeF GetSize();
        // void SetSize(const MathUtility::SizeF value);

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

        bool m_fontIsDirty;
        std::string m_fontPath;
        //ssfn_t m_fontContext;
        bool m_fontLoaded;

        uint32_t m_textureID;

        bool m_textIsDirty;
        std::string m_text;
        uint32_t m_fontSize;

        bool m_meshIsDirty;
        MathUtility::SizeF m_size;
        MathUtility::SizeI m_pixelBufferSize;
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