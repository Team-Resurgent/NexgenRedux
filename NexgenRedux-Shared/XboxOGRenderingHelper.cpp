#if defined NEXGEN_OG

#include "XboxOGRenderingHelper.h"
#include "XboxOGWindowHelper.h"
#include "RenderStateManager.h"
#include "WindowManager.h"
#include "MeshUtility.h"
#include "ConfigLoader.h"

#include <Gensys/Int.h>
#include <Gensys/DebugUtility.h>
#include <Gensys/StringUtility.h>

#include <xtl.h>
#include <vector>
#include <map>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ASSERT(x)
#include <STB/stb_image.h>

using namespace Gensys;
using namespace NexgenRedux;

namespace 
{
	bool m_initialized = false;

	D3DVertexBuffer* m_dynamicBuffer = NULL;
	uint32_t m_dynamicBufferSize = 0;

	uint32_t m_maxTextureContainerID = 0;
	std::map<uint32_t, XboxOGRenderingHelper::TextureContainer> m_textureContainerMap;


}

void XboxOGRenderingHelper::Close()
{
	DeleteTextures();
	DeleteDynamicBuffer();
}

bool XboxOGRenderingHelper::Init()
{
	if (m_initialized == true)
    {
        return true;
    }
    m_initialized = true;

	CreateDynamicBuffer();

	return true;
}

void XboxOGRenderingHelper::SetShader(std::string shaderName)
{
	if (shaderName != "Default")
	{
		DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetShader: Shader '%s' not recognized.", shaderName.c_str());
		return;
	}

	IDirect3DDevice8* d3dDevice = (IDirect3DDevice8*)XboxOGWindowHelper::GetD3DDevice();

	if (FAILED(d3dDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE)))
	{
		DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetShader: Shader '%s' init failed.", shaderName.c_str());
		return;
	}

	if (FAILED(d3dDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE)))
	{
		DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetShader: Shader '%s' init failed.", shaderName.c_str());
		return;
	}

	if (FAILED(d3dDevice->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_CURRENT)))
	{
		DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetShader: Shader '%s' init failed.", shaderName.c_str());
		return;
	}

	if (FAILED(d3dDevice->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_MODULATE)))
	{
		DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetShader: Shader '%s' init failed.", shaderName.c_str());
		return;
	}

	if (FAILED(d3dDevice->SetTextureStageState(1, D3DTSS_COLORARG1, D3DTA_CURRENT)))
	{
		DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetShader: Shader '%s' init failed.", shaderName.c_str());
		return;
	}
	
	if (FAILED(d3dDevice->SetTextureStageState(1, D3DTSS_COLORARG2, D3DTA_TFACTOR)))
	{
		DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetShader: Shader '%s' init failed.", shaderName.c_str());
		return;
	}

	if (FAILED(d3dDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1)))
	{
		DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetShader: Shader '%s' init failed.", shaderName.c_str());
		return;
	}

	if (FAILED(d3dDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE)))
	{
		DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetShader: Shader '%s' init failed.", shaderName.c_str());
		return;
	}
	
	if (FAILED(d3dDevice->SetVertexShader(D3DFVF_XYZ | D3DFVF_NORMAL| D3DFVF_TEX1)))
	{
		DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetShader: Shader '%s' init failed.", shaderName.c_str());
		return;
	}
}

void XboxOGRenderingHelper::SetModelMatrix(const MathUtility::Matrix4x4& matrix) 
{
	IDirect3DDevice8* d3dDevice = (IDirect3DDevice8*)XboxOGWindowHelper::GetD3DDevice();

	D3DXMATRIX tempMatrix = D3DXMATRIX(matrix.values);
	if (FAILED(d3dDevice->SetTransform(D3DTS_WORLD, &tempMatrix)))
	{
		DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetModelMatrix: Failed.");
	}
}

void XboxOGRenderingHelper::SetViewMatrix(const MathUtility::Matrix4x4& matrix) 
{
	IDirect3DDevice8* d3dDevice = (IDirect3DDevice8*)XboxOGWindowHelper::GetD3DDevice();

	MathUtility::Vec3F scale = MathUtility::Vec3F(-1.0f, 1.0f, 1.0f);
	MathUtility::Matrix4x4 scaleMatrix = MathUtility::Matrix4x4::Scale(scale);
	MathUtility::Matrix4x4 scaledViewMatrix = scaleMatrix * matrix;
	D3DXMATRIX tempMatrix = D3DXMATRIX(scaledViewMatrix.values);
	if (FAILED(d3dDevice->SetTransform(D3DTS_VIEW, &tempMatrix)))
	{
		DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetViewMatrix: Failed.");
	}
}

void XboxOGRenderingHelper::SetProjectionMatrix(const MathUtility::Matrix4x4& matrix) 
{
	IDirect3DDevice8* d3dDevice = (IDirect3DDevice8*)XboxOGWindowHelper::GetD3DDevice();

	D3DXMATRIX tempMatrix = D3DXMATRIX(matrix.values);
	if (FAILED(d3dDevice->SetTransform(D3DTS_PROJECTION, &tempMatrix)))
	{
		DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetProjectionMatrix: Failed.");
	}
}

void XboxOGRenderingHelper::SetAmbientLight(const MathUtility::Color3I& color) 
{
	IDirect3DDevice8* d3dDevice = (IDirect3DDevice8*)XboxOGWindowHelper::GetD3DDevice();

	D3DCOLOR tempColor = D3DCOLOR_XRGB(color.values[0], color.values[1], color.values[2]);
	if (FAILED(d3dDevice->SetRenderState(D3DRS_AMBIENT, tempColor)))
	{
		DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetAmbientLight: Failed.");
	}
}

void XboxOGRenderingHelper::SetTexture(const uint32_t& textureID, const RenderStateManager::TextureFilter& filter) 
{
	TextureContainer* textureContainer = GetTextureContainer(textureID);
	if (textureContainer == NULL)
	{
		DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetTexture: Texture with ID '%s' not found.", textureID);
		return;
	}

	IDirect3DDevice8* d3dDevice = (IDirect3DDevice8*)XboxOGWindowHelper::GetD3DDevice();

	if (FAILED(d3dDevice->SetTexture(0, textureContainer->texture)))
	{
		DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetTexture: Failed to set texture.");
	}

	if (filter == RenderStateManager::TextureFilterLinear)
	{
		if (FAILED(d3dDevice->SetTextureStageState(0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR)))
		{
			DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetTexture: Failed to set mag filter.");
		}
		if (FAILED(d3dDevice->SetTextureStageState(0, D3DTSS_MINFILTER, D3DTEXF_LINEAR)))
		{
			DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetTexture: Failed to set min filter.");
		}
	}
	else if (filter == RenderStateManager::TextureFilterNearest)
	{
		if (FAILED(d3dDevice->SetTextureStageState(0, D3DTSS_MAGFILTER, D3DTEXF_POINT)))
		{
			DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetTexture: Failed to set mag filter.");
		}
		if (FAILED(d3dDevice->SetTextureStageState(0, D3DTSS_MINFILTER, D3DTEXF_POINT)))
		{
			DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetTexture: Failed to set min filter.");
		}
	}
}

void XboxOGRenderingHelper::SetTint(const MathUtility::Color4I& color) 
{
	IDirect3DDevice8* d3dDevice = (IDirect3DDevice8*)XboxOGWindowHelper::GetD3DDevice();

	D3DCOLOR tempColor = D3DCOLOR_RGBA(color.values[0], color.values[1], color.values[2], color.values[3]);
	if (FAILED(d3dDevice->SetRenderState(D3DRS_TEXTUREFACTOR, tempColor)))
	{
		DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetTint: Failed.");
	}
}

void XboxOGRenderingHelper::SetBlend(const RenderStateManager::BlendOperation& operation) 
{
	IDirect3DDevice8* d3dDevice = (IDirect3DDevice8*)XboxOGWindowHelper::GetD3DDevice();

	if (operation == RenderStateManager::BlendOperationDisabled)
	{
		if (FAILED(d3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE)))
		{
			DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetBlend: Failed to disable.");
		}
	}
	else if (operation == RenderStateManager::BlendOperationAdd)
	{
		if (FAILED(d3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE)))
		{
			DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetBlend: Failed to enable.");
		}
		if (FAILED(d3dDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD)))
		{
			DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetBlend: Failed to set operation add.");
		}
	}
	else if (operation == RenderStateManager::BlendOperationSubtract)
	{
		if (FAILED(d3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE)))
		{
			DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetBlend: Failed to enable.");
		}
		if (FAILED(d3dDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_SUBTRACT)))
		{
			DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetBlend: Failed to set operation subtract.");
		}
	}
	else if (operation == RenderStateManager::BlendOperationReverseSubtract)
	{
		if (FAILED(d3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE)))
		{
			DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetBlend: Failed to enable.");
		}
		if (FAILED(d3dDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_REVSUBTRACT)))
		{
			DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetBlend: Failed to set operation reverse subtract.");
		}
	}
}

void XboxOGRenderingHelper::SetBlendFactors(const RenderStateManager::BlendFactor& srcFactor, const RenderStateManager::BlendFactor& dstFactor) 
{
	uint32_t srcFactorValue = RenderStateManager::BlendFactorZero;
	if (srcFactor == RenderStateManager::BlendFactorZero) 
	{
		srcFactorValue = D3DBLEND_ZERO;
	}
	else if (srcFactor == RenderStateManager::BlendFactorOne) 
	{
		srcFactorValue = D3DBLEND_ONE;
	}
	else if (srcFactor == RenderStateManager::BlendFactorSrcColor) 
	{
		srcFactorValue = D3DBLEND_SRCCOLOR;
	}
	else if (srcFactor == RenderStateManager::BlendFactorOneMinusSrcColor) 
	{
		srcFactorValue = D3DBLEND_INVSRCCOLOR;
	}
	else if (srcFactor == RenderStateManager::BlendFactorDstColor) 
	{
		srcFactorValue = D3DBLEND_DESTCOLOR;
	}
	else if (srcFactor == RenderStateManager::BlendFactorOneMinusDstColor) 
	{
		srcFactorValue = D3DBLEND_INVDESTCOLOR;
	}
	else if (srcFactor == RenderStateManager::BlendFactorSrcAlpha) 
	{
		srcFactorValue = D3DBLEND_SRCALPHA;
	}
	else if (srcFactor == RenderStateManager::BlendFactorOneMinusSrcAlpha) 
	{
		srcFactorValue = D3DBLEND_INVSRCALPHA;
	}
	else if (srcFactor == RenderStateManager::BlendFactorDstAlpha) 
	{
		srcFactorValue = D3DBLEND_DESTALPHA;
	}
	else if (srcFactor == RenderStateManager::BlendFactorOneMinusDstAlpha) 
	{
		srcFactorValue = D3DBLEND_INVDESTALPHA;
	}
	else if (srcFactor == RenderStateManager::BlendFactorConstantColor) 
	{
		srcFactorValue = D3DBLEND_CONSTANTCOLOR;
	}
	else if (srcFactor == RenderStateManager::BlendFactorOneMinusConstantColor) 
	{
		srcFactorValue = D3DBLEND_INVCONSTANTCOLOR;
	}
	else if (srcFactor == RenderStateManager::BlendFactorConstantAlpha) 
	{
		srcFactorValue = D3DBLEND_CONSTANTALPHA;
	}
	else if (srcFactor == RenderStateManager::BlendFactorOneMinusConstantAlpha) 
	{
		srcFactorValue = D3DBLEND_INVCONSTANTALPHA;
	}
	else if (srcFactor == RenderStateManager::BlendFactorSrcAlphaSaturate) 
	{
		srcFactorValue = D3DBLEND_SRCALPHASAT;
	}

	uint32_t dstFactorValue = RenderStateManager::BlendFactorZero;
	if (dstFactor == RenderStateManager::BlendFactorZero) 
	{
		dstFactorValue = D3DBLEND_ZERO;
	}
	else if (dstFactor == RenderStateManager::BlendFactorOne) 
	{
		dstFactorValue = D3DBLEND_ONE;
	}
	else if (dstFactor == RenderStateManager::BlendFactorSrcColor) 
	{
		dstFactorValue = D3DBLEND_SRCCOLOR;
	}
	else if (dstFactor == RenderStateManager::BlendFactorOneMinusSrcColor) 
	{
		dstFactorValue = D3DBLEND_INVSRCCOLOR;
	}
	else if (dstFactor == RenderStateManager::BlendFactorDstColor) 
	{
		dstFactorValue = D3DBLEND_DESTCOLOR;
	}
	else if (dstFactor == RenderStateManager::BlendFactorOneMinusDstColor) 
	{
		dstFactorValue = D3DBLEND_INVDESTCOLOR;
	}
	else if (dstFactor == RenderStateManager::BlendFactorSrcAlpha) 
	{
		dstFactorValue = D3DBLEND_SRCALPHA;
	}
	else if (dstFactor == RenderStateManager::BlendFactorOneMinusSrcAlpha) 
	{
		dstFactorValue = D3DBLEND_INVSRCALPHA;
	}
	else if (dstFactor == RenderStateManager::BlendFactorDstAlpha) 
	{
		dstFactorValue = D3DBLEND_DESTALPHA;
	}
	else if (dstFactor == RenderStateManager::BlendFactorOneMinusDstAlpha) 
	{
		dstFactorValue = D3DBLEND_INVDESTALPHA;
	}
	else if (dstFactor == RenderStateManager::BlendFactorConstantColor) 
	{
		dstFactorValue = D3DBLEND_CONSTANTCOLOR;
	}
	else if (dstFactor == RenderStateManager::BlendFactorOneMinusConstantColor) 
	{
		dstFactorValue = D3DBLEND_INVCONSTANTCOLOR;
	}
	else if (dstFactor == RenderStateManager::BlendFactorConstantAlpha) 
	{
		dstFactorValue = D3DBLEND_CONSTANTALPHA;
	}
	else if (dstFactor == RenderStateManager::BlendFactorOneMinusConstantAlpha) 
	{
		dstFactorValue = D3DBLEND_INVCONSTANTALPHA;
	}
	else if (dstFactor == RenderStateManager::BlendFactorSrcAlphaSaturate) 
	{
		dstFactorValue = D3DBLEND_SRCALPHASAT;
	}

	IDirect3DDevice8* d3dDevice = (IDirect3DDevice8*)XboxOGWindowHelper::GetD3DDevice();

	if (FAILED(d3dDevice->SetRenderState(D3DRS_SRCBLEND, srcFactorValue)))
	{
		DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetBlendFactors: Failed to set source blend.");
	}
	if (FAILED(d3dDevice->SetRenderState(D3DRS_DESTBLEND, dstFactorValue)))
	{
		DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetBlendFactors: Failed to set destination blend.");
	}
}

void XboxOGRenderingHelper::SetCulling(const RenderStateManager::CullingOperation& operation) 
{
	IDirect3DDevice8* d3dDevice = (IDirect3DDevice8*)XboxOGWindowHelper::GetD3DDevice();

	if (operation == RenderStateManager::CullingOperationDisabled) 
	{
		if (FAILED(d3dDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE)))
		{
			DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetCulling: Failed to disable.");
		}
	}
	else if (operation == RenderStateManager::CullingOperationFront) 
	{
		if (FAILED(d3dDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_CW)))
		{
			DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetCulling: Failed to set cull operation front.");
		}
	}
	else if (operation == RenderStateManager::CullingOperationBack) 
	{
		if (FAILED(d3dDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW)))
		{
			DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetCulling: Failed to set cull operation back.");
		}
	}
}

void XboxOGRenderingHelper::SetDepth(const RenderStateManager::DepthOperation& operation) 
{
	IDirect3DDevice8* d3dDevice = (IDirect3DDevice8*)XboxOGWindowHelper::GetD3DDevice();

	if (operation == RenderStateManager::DepthOperationDisabled) 
	{
		if (FAILED(d3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE)))
		{
			DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetDepth: Failed to disable.");
		}
	}
	else if (operation == RenderStateManager::DepthOperationNever) 
	{
		if (FAILED(d3dDevice->SetRenderState(D3DRS_ZENABLE, TRUE)))
		{
			DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetDepth: Failed to enable.");
		}
		if (FAILED(d3dDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_NEVER)))
		{
			DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetDepth: Failed to set operation never.");
		}
	}
	else if (operation == RenderStateManager::DepthOperationLess) 
	{
		if (FAILED(d3dDevice->SetRenderState(D3DRS_ZENABLE, TRUE)))
		{
			DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetDepth: Failed to enable.");
		}
		if (FAILED(d3dDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESS)))
		{
			DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetDepth: Failed to set operation less.");
		}
	}
	else if (operation == RenderStateManager::DepthOperationEqual) 
	{
		if (FAILED(d3dDevice->SetRenderState(D3DRS_ZENABLE, TRUE)))
		{
			DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetDepth: Failed to enable.");
		}
		if (FAILED(d3dDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_EQUAL)))
		{
			DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetDepth: Failed to set operation equal.");
		}
	}
	else if (operation == RenderStateManager::DepthOperationGreater) 
	{
		if (FAILED(d3dDevice->SetRenderState(D3DRS_ZENABLE, TRUE)))
		{
			DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetDepth: Failed to enable.");
		}
		if (FAILED(d3dDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_GREATER)))
		{
			DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetDepth: Failed to set operation greater.");
		}
	}
	else if (operation == RenderStateManager::DepthOperationLessOrEqual) 
	{
		if (FAILED(d3dDevice->SetRenderState(D3DRS_ZENABLE, TRUE)))
		{
			DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetDepth: Failed to enable.");
		}
		if (FAILED(d3dDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL)))
		{
			DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetDepth: Failed to set operation less or equal.");
		}
	}
	else if (operation == RenderStateManager::DepthOperationNotEqual) 
	{
		if (FAILED(d3dDevice->SetRenderState(D3DRS_ZENABLE, TRUE)))
		{
			DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetDepth: Failed to enable.");
		}
		if (FAILED(d3dDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_NOTEQUAL)))
		{
			DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetDepth: Failed to set operation not euqal.");
		}
	}
	else if (operation == RenderStateManager::DepthOperationGreaterOrEqual) 
	{
		if (FAILED(d3dDevice->SetRenderState(D3DRS_ZENABLE, TRUE)))
		{
			DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetDepth: Failed to enable.");
		}
		if (FAILED(d3dDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_GREATEREQUAL)))
		{
			DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetDepth: Failed to set operation greater or equal.");
		}
	}
	else if (operation == RenderStateManager::DepthOperationAlways) 
	{
		if (FAILED(d3dDevice->SetRenderState(D3DRS_ZENABLE, TRUE)))
		{
			DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetDepth: Failed to enable.");
		}
		if (FAILED(d3dDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_ALWAYS)))
		{
			DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetDepth: Failed to set operation always.");
		}
	}
}

void XboxOGRenderingHelper::SetLights(const RenderStateManager::LightsOperation& operation) 
{
	IDirect3DDevice8* d3dDevice = (IDirect3DDevice8*)XboxOGWindowHelper::GetD3DDevice();

	if (operation == RenderStateManager::LightsOperationDisabled) 
	{
		if (FAILED(d3dDevice->SetRenderState(D3DRS_LIGHTING, FALSE)))
		{
			DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetLights: Failed to disable.");
		}
	}
	else if (operation == RenderStateManager::LightsOperationEnabled) 
	{
		if (FAILED(d3dDevice->SetRenderState(D3DRS_LIGHTING, TRUE)))
		{
			DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetLights: Failed to enable.");
		}
	}
}

void XboxOGRenderingHelper::SetLight1(const RenderStateManager::LightOperation& operation) 
{
	IDirect3DDevice8* d3dDevice = (IDirect3DDevice8*)XboxOGWindowHelper::GetD3DDevice();

	if (operation == RenderStateManager::LightOperationDisabled) 
	{
		if (FAILED(d3dDevice->LightEnable(0, FALSE)))
		{
			DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetLight1: Failed to disable.");
		}
	}
	else if (operation == RenderStateManager::LightOperationEnabled) 
	{
		if (FAILED(d3dDevice->LightEnable(0, TRUE)))
		{
			DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetLight1: Failed to enable.");
		}
	}
}

void XboxOGRenderingHelper::SetLightInstance1(const MathUtility::Vec3F& position, const float& distance, const MathUtility::Color4I& diffuse) 
{
	IDirect3DDevice8* d3dDevice = (IDirect3DDevice8*)XboxOGWindowHelper::GetD3DDevice();

	D3DLIGHT8 light;
	memset(&light, 0, sizeof(light));
	light.Type = D3DLIGHT_POINT;
	light.Diffuse.r = diffuse.r / 255.0f;
	light.Diffuse.g = diffuse.g / 255.0f;
	light.Diffuse.b = diffuse.b / 255.0f;
	light.Diffuse.a = 1.0;
	light.Range = distance;
	light.Position = D3DXVECTOR3(position.values[0], position.values[1], position.values[2]);
	light.Attenuation0 = 1.0f;
	if (FAILED(d3dDevice->SetLight(0, &light)))
	{
		DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetLightInstance1: Failed to set light.");
	}

	D3DMATERIAL8 material;
	memset(&material, 0, sizeof(material));
	material.Diffuse = D3DXCOLOR(1.0f, 1.0f, 1.0f, 1.0f);
	material.Ambient = D3DXCOLOR(1.0f, 1.0f, 1.0f, 1.0f);
	if (FAILED(d3dDevice->SetMaterial(&material)))
	{
		DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetLightInstance1: Failed to set material.");
	}
}

void XboxOGRenderingHelper::SetLight2(const RenderStateManager::LightOperation& operation) 
{
	IDirect3DDevice8* d3dDevice = (IDirect3DDevice8*)XboxOGWindowHelper::GetD3DDevice();

	if (operation == RenderStateManager::LightOperationDisabled) 
	{
		if (FAILED(d3dDevice->LightEnable(1, FALSE)))
		{
			DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetLight2: Failed to disable.");
		}
	}
	else if (operation == RenderStateManager::LightOperationEnabled) 
	{
		if (FAILED(d3dDevice->LightEnable(1, TRUE)))
		{
			DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetLight2: Failed to enable.");
		}
	}
}

void XboxOGRenderingHelper::SetLightInstance2(const MathUtility::Vec3F& position, const float& distance, const MathUtility::Color4I& diffuse) 
{
	IDirect3DDevice8* d3dDevice = (IDirect3DDevice8*)XboxOGWindowHelper::GetD3DDevice();

	D3DLIGHT8 light;
	memset(&light, 0, sizeof(light));
	light.Type = D3DLIGHT_POINT;
	light.Diffuse.r = diffuse.r / 255.0f;
	light.Diffuse.g = diffuse.g / 255.0f;
	light.Diffuse.b = diffuse.b / 255.0f;
	light.Diffuse.a = 1.0;
	light.Range = distance;
	light.Position = D3DXVECTOR3(position.values[0], position.values[1], position.values[2]);
	light.Attenuation0 = 1.0f;
	if (FAILED(d3dDevice->SetLight(1, &light)))
	{
		DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetLightInstance2: Failed to set light.");
	}

	D3DMATERIAL8 material;
	memset(&material, 0, sizeof(material));
	material.Diffuse = D3DXCOLOR(1.0f, 1.0f, 1.0f, 1.0f);
	material.Ambient = D3DXCOLOR(1.0f, 1.0f, 1.0f, 1.0f);
	if (FAILED(d3dDevice->SetMaterial(&material)))
	{
		DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetLightInstance2: Failed to set material.");
	}
}

void XboxOGRenderingHelper::SetLight3(const RenderStateManager::LightOperation& operation) 
{
	IDirect3DDevice8* d3dDevice = (IDirect3DDevice8*)XboxOGWindowHelper::GetD3DDevice();

	if (operation == RenderStateManager::LightOperationDisabled) 
	{
		if (FAILED(d3dDevice->LightEnable(2, FALSE)))
		{
			DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetLight3: Failed to disable.");
		}
	}
	else if (operation == RenderStateManager::LightOperationEnabled) 
	{
		if (FAILED(d3dDevice->LightEnable(2, TRUE)))
		{
			DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetLight3: Failed to enable.");
		}
	}
}

void XboxOGRenderingHelper::SetLightInstance3(const MathUtility::Vec3F& position, const float& distance, const MathUtility::Color4I& diffuse) 
{
	IDirect3DDevice8* d3dDevice = (IDirect3DDevice8*)XboxOGWindowHelper::GetD3DDevice();

	D3DLIGHT8 light;
	memset(&light, 0, sizeof(light));
	light.Type = D3DLIGHT_POINT;
	light.Diffuse.r = diffuse.r / 255.0f;
	light.Diffuse.g = diffuse.g / 255.0f;
	light.Diffuse.b = diffuse.b / 255.0f;
	light.Diffuse.a = 1.0;
	light.Range = distance;
	light.Position = D3DXVECTOR3(position.values[0], position.values[1], position.values[2]);
	light.Attenuation0 = 1.0f;
	if (FAILED(d3dDevice->SetLight(2, &light)))
	{
		DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetLightInstance3: Failed to set light.");
	}

	D3DMATERIAL8 material;
	memset(&material, 0, sizeof(material));
	material.Diffuse = D3DXCOLOR(1.0f, 1.0f, 1.0f, 1.0f);
	material.Ambient = D3DXCOLOR(1.0f, 1.0f, 1.0f, 1.0f);
	if (FAILED(d3dDevice->SetMaterial(&material)))
	{
		DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetLightInstance3: Failed to set material.");
	}
}

void XboxOGRenderingHelper::SetLight4(const RenderStateManager::LightOperation& operation) 
{
	IDirect3DDevice8* d3dDevice = (IDirect3DDevice8*)XboxOGWindowHelper::GetD3DDevice();

	if (operation == RenderStateManager::LightOperationDisabled) 
	{
		if (FAILED(d3dDevice->LightEnable(3, FALSE)))
		{
			DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetLight4: Failed to disable.");
		}
	}
	else if (operation == RenderStateManager::LightOperationEnabled) 
	{
		if (FAILED(d3dDevice->LightEnable(3, TRUE)))
		{
			DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetLight4: Failed to enable.");
		}
	}
}

void XboxOGRenderingHelper::SetLightInstance4(const MathUtility::Vec3F& position, const float& distance, const MathUtility::Color4I& diffuse) 
{
	IDirect3DDevice8* d3dDevice = (IDirect3DDevice8*)XboxOGWindowHelper::GetD3DDevice();

	D3DLIGHT8 light;
	memset(&light, 0, sizeof(light));
	light.Type = D3DLIGHT_POINT;
	light.Diffuse.r = diffuse.r / 255.0f;
	light.Diffuse.g = diffuse.g / 255.0f;
	light.Diffuse.b = diffuse.b / 255.0f;
	light.Diffuse.a = 1.0;
	light.Range = distance;
	light.Position = D3DXVECTOR3(position.values[0], position.values[1], position.values[2]);
	light.Attenuation0 = 1.0f;
	if (FAILED(d3dDevice->SetLight(3, &light)))
	{
		DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetLightInstance4: Failed to set light.");
	}

	D3DMATERIAL8 material;
	memset(&material, 0, sizeof(material));
	material.Diffuse = D3DXCOLOR(1.0f, 1.0f, 1.0f, 1.0f);
	material.Ambient = D3DXCOLOR(1.0f, 1.0f, 1.0f, 1.0f);
	if (FAILED(d3dDevice->SetMaterial(&material)))
	{
		DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetLightInstance4: Failed to set material.");
	}
}

void XboxOGRenderingHelper::SetFog(const RenderStateManager::FogOperation& operation) 
{
	IDirect3DDevice8* d3dDevice = (IDirect3DDevice8*)XboxOGWindowHelper::GetD3DDevice();

	if (operation == RenderStateManager::FogOperationDisabled) 
	{
		if (FAILED(d3dDevice->SetRenderState(D3DRS_FOGENABLE, FALSE)))
		{
			DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetFog: Failed to disable.");
		}
	}
	else if (operation == RenderStateManager::FogOperationExponent) 
	{
		if (FAILED(d3dDevice->SetRenderState(D3DRS_FOGENABLE, TRUE)))
		{
			DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetFog: Failed to enable.");
		}
		if (FAILED(d3dDevice->SetRenderState(D3DRS_FOGTABLEMODE, D3DFOG_EXP)))
		{
			DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetFog: Failed to set operation exponent.");
		}
	}
	else if (operation == RenderStateManager::FogOperationExponent2) 
	{
		if (FAILED(d3dDevice->SetRenderState(D3DRS_FOGENABLE, TRUE)))
		{
			DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetFog: Failed to enable.");
		}
		if (FAILED(d3dDevice->SetRenderState(D3DRS_FOGTABLEMODE, D3DFOG_EXP2)))
		{
			DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetFog: Failed to set operation exponent 2.");
		}
	}
	else if (operation == RenderStateManager::FogOperationLinear) 
	{
		if (FAILED(d3dDevice->SetRenderState(D3DRS_FOGENABLE, TRUE)))
		{
			DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetFog: Failed to enable.");
		}
		if (FAILED(d3dDevice->SetRenderState(D3DRS_FOGTABLEMODE, D3DFOG_LINEAR)))
		{
			DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetFog: Failed to set operation linear.");
		}
	}
}

void XboxOGRenderingHelper::SetFogInstance(const MathUtility::Color3I& color, const float& start, const float& end, const float& density) 
{
	IDirect3DDevice8* d3dDevice = (IDirect3DDevice8*)XboxOGWindowHelper::GetD3DDevice();

	D3DCOLOR tempColor = D3DCOLOR_RGBA(color.values[0], color.values[1], color.values[2], 255);
	if (FAILED(d3dDevice->SetRenderState(D3DRS_FOGCOLOR, tempColor)))
	{
		DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetFogInstance: Failed to set color.");
	}

	if (FAILED(d3dDevice->SetRenderState(D3DRS_FOGSTART, *(DWORD*)(&start))))
	{
		DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetFogInstance: Failed to set start.");
	}

	if (FAILED(d3dDevice->SetRenderState(D3DRS_FOGEND, *(DWORD*)(&end))))
	{
		DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetFogInstance: Failed to set end.");
	}

	if (FAILED(d3dDevice->SetRenderState(D3DRS_FOGDENSITY, *(DWORD*)(&density))))
	{
		DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetFogInstance: Failed to set density.");
	}
}

void XboxOGRenderingHelper::SetViewport(const MathUtility::RectI rect) 
{
	IDirect3DDevice8* d3dDevice = (IDirect3DDevice8*)XboxOGWindowHelper::GetD3DDevice();

	D3DVIEWPORT8 viewport;
	memset(&viewport, 0, sizeof(viewport));
	viewport.X = rect.x;
	viewport.Y = rect.y;
	viewport.Width = rect.width;
	viewport.Height = rect.height;
	viewport.MinZ = 0.0f;
	viewport.MaxZ = 1.0f;

	if (FAILED(d3dDevice->SetViewport(&viewport)))
	{
		DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetViewport: Failed.");
	}
}

void XboxOGRenderingHelper::SetScissor(const RenderStateManager::ScissorOperation& operation, const MathUtility::RectI& rect) 
{
	IDirect3DDevice8* d3dDevice = (IDirect3DDevice8*)XboxOGWindowHelper::GetD3DDevice();

	if (operation == RenderStateManager::ScissorOperationDisabled) 
	{
		if (FAILED(d3dDevice->SetScissors(0, FALSE, NULL)))
		{
			DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetScissor: Failed to disable.");
		}
	}
	else if (operation == RenderStateManager::ScissorOperationEnabled) 
	{
		D3DRECT scrissorRect;
		memset(&scrissorRect, 0, sizeof(scrissorRect));
		scrissorRect.x1 = rect.x;
		scrissorRect.x2 = rect.x + rect.width;
		scrissorRect.y1 = rect.y;
		scrissorRect.y2 = rect.y + rect.height;

		if (FAILED(d3dDevice->SetScissors(1, TRUE, &scrissorRect)))
		{
			DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "SetScissor: Failed.");
		}
	}
}

bool XboxOGRenderingHelper::LoadTexture(std::wstring path, uint32_t& textureID)
{	
	std::wstring mappedPath;
	if (ConfigLoader::MapPath(path, mappedPath) == 0)
	{
		return false;
	}

	IDirect3DDevice8* d3dDevice = (IDirect3DDevice8*)XboxOGWindowHelper::GetD3DDevice();

	IDirect3DTexture8 *texture;
	if (FAILED(D3DXCreateTextureFromFileA(d3dDevice, StringUtility::ToString(mappedPath).c_str(), &texture)))
	{
		return false;
	}

	D3DSURFACE_DESC surfaceDesc;
	texture->GetLevelDesc(0, &surfaceDesc);

	uint32_t textureContainerID = ++m_maxTextureContainerID;
	TextureContainer textureContainer;
	textureContainer.texture = texture;
	textureContainer.width = surfaceDesc.Width;
	textureContainer.height = surfaceDesc.Height;
	m_textureContainerMap.insert(std::pair<int, TextureContainer>(textureContainerID, textureContainer));
	textureID = textureContainerID;
	return true;
}

bool XboxOGRenderingHelper::RenderMesh(uint32_t meshID)
{
	MeshUtility::Mesh* mesh = MeshUtility::GetMesh(meshID);
	if (mesh == NULL)
	{
		return false;
	}

	RenderStateManager::SetTexture(mesh->textureID, RenderStateManager::TextureFilterLinear);
	RenderStateManager::ApplyChanges();

	uint32_t requestedSize =  mesh->vertices.size() * sizeof(MeshUtility::Vertex);
	ResizeDynamicBufferIfNeeded(requestedSize);

	IDirect3DDevice8* d3dDevice = (IDirect3DDevice8*)XboxOGWindowHelper::GetD3DDevice();

	uint32_t vertexCount = mesh->vertices.size();
	if (RenderStateManager::GetRenderState()->drawModeState.operation == RenderStateManager::DrawModeTriangles) 
	{
		d3dDevice->DrawPrimitiveUP(D3DPT_TRIANGLELIST, vertexCount / 3, &mesh->vertices[0], sizeof(MeshUtility::Vertex));
	}
	else if (RenderStateManager::GetRenderState()->drawModeState.operation == RenderStateManager::DrawModeLines) 
	{
		d3dDevice->DrawPrimitiveUP(D3DPT_LINELIST, vertexCount / 3, &mesh->vertices[0], sizeof(MeshUtility::Vertex));
	}
	return true;
}

// Privates 

void XboxOGRenderingHelper::ResizeDynamicBufferIfNeeded(uint32_t requestedSize)
{
	if (requestedSize <= m_dynamicBufferSize)
	{
		return;
	}

	IDirect3DDevice8* d3dDevice = (IDirect3DDevice8*)XboxOGWindowHelper::GetD3DDevice();

	if (m_dynamicBuffer != NULL)
	{
		m_dynamicBuffer->Release();
	}

	m_dynamicBufferSize = requestedSize;
	if (FAILED(d3dDevice->CreateVertexBuffer(requestedSize, D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT, &m_dynamicBuffer)))
	{
		return;
	}
}

void XboxOGRenderingHelper::CreateDynamicBuffer()
{
	ResizeDynamicBufferIfNeeded(65535);
}

void XboxOGRenderingHelper::DeleteDynamicBuffer()
{
	if (m_dynamicBuffer == NULL)
	{
		return;
	}
	m_dynamicBuffer->Release();
	m_dynamicBuffer = NULL;
}

XboxOGRenderingHelper::TextureContainer* XboxOGRenderingHelper::GetTextureContainer(uint32_t textureID)
{
    std::map<uint32_t, TextureContainer>::iterator it = m_textureContainerMap.find(textureID);
	if (it == m_textureContainerMap.end()) 
	{
		return NULL;
	}
	return (TextureContainer*)&it->second;
}

void XboxOGRenderingHelper::DeleteTextures()
{
	for (std::map<uint32_t, TextureContainer>::iterator it = m_textureContainerMap.begin(); it != m_textureContainerMap.end(); ++it)
	{
		IDirect3DTexture8* texture = it->second.texture;
		texture->Release();
	}
}

#endif