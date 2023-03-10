#if defined NEXGEN_OG

#include "XboxOGRenderingHelper.h"
#include "XboxOGWindowHelper.h"
#include "WindowManager.h"
#include "ConfigLoader.h"

#include <Gensys/Int.h>
#include <Gensys/StringUtility.h>

#include <xtl.h>
#include <map>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ASSERT(x)
#include <STB/stb_image.h>

using namespace Gensys;
using namespace NexgenRedux;

namespace 
{
	typedef struct TextureContainer
	{
		IDirect3DTexture8 *texture;
		uint32_t width;
		uint32_t height;
	} TextureContainer;

	std::map<uint32_t, TextureContainer> m_textureContainerMap;
}

void XboxOGRenderingHelper::Close()
{
}

bool XboxOGRenderingHelper::Init()
{
	return true;
}

bool XboxOGRenderingHelper::SetShader(std::string shaderName)
{
	if (shaderName != "Default")
	{
		return false;
	}

	IDirect3DDevice8* d3dDevice = (IDirect3DDevice8*)XboxOGWindowHelper::GetD3DDevice();

	if (FAILED(d3dDevice->SetRenderState(D3DRS_TEXTUREFACTOR, 0XFFFFFFFF)))
	{
		return false;
	}

	if (FAILED(d3dDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE)))
	{
		return false;
	}

	if (FAILED(d3dDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE)))
	{
		return false;
	}

	if (FAILED(d3dDevice->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_CURRENT)))
	{
		return false;
	}

	if (FAILED(d3dDevice->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_MODULATE)))
	{
		return false;
	}

	if (FAILED(d3dDevice->SetTextureStageState(1, D3DTSS_COLORARG1, D3DTA_CURRENT)))
	{
		return false;
	}
	
	if (FAILED(d3dDevice->SetTextureStageState(1, D3DTSS_COLORARG2, D3DTA_TFACTOR)))
	{
		return false;
	}

	if (FAILED(d3dDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1)))
	{
		return false;
	}

	if (FAILED(d3dDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE)))
	{
		return false;
	}

	if (FAILED(d3dDevice->SetTextureStageState(0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR)))
	{
		return false;
	}

	if (FAILED(d3dDevice->SetTextureStageState(0, D3DTSS_MINFILTER, D3DTEXF_LINEAR)))
	{
		return false;
	}
	
	if (FAILED(d3dDevice->SetVertexShader(D3DFVF_XYZ | D3DFVF_NORMAL| D3DFVF_TEX1)))
	{
		return false;
	}

	return true;
}

bool XboxOGRenderingHelper::LoadTexture(std::wstring path, uint32_t& textureID)
{	
	std::wstring mappedPath;
	if (ConfigLoader::MapPath(path, mappedPath) == 0)
	{
		return false;
	}
	
	int width;
	int height;
	int channels;
	unsigned char* data = stbi_load(StringUtility::ToString(mappedPath).c_str(), &width, &height, &channels, STBI_rgb_alpha);
	if (!data)
	{
		return false;
	}

	IDirect3DDevice8* d3dDevice = (IDirect3DDevice8*)XboxOGWindowHelper::GetD3DDevice();

	IDirect3DTexture8 *texture;
	if (FAILED(D3DXCreateTexture(d3dDevice, width, height, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &texture)))
	{
		return false;
	}

	D3DLOCKED_RECT lockedRect;
	if (FAILED(texture->LockRect(0, &lockedRect, NULL, 0)))
	{
		return false;
	}

	memcpy(lockedRect.pBits, data, width * height * channels);

	if (FAILED(texture->UnlockRect(0)))
	{
		return false;
	}

//D3dXCreate
//	hr = D3DXCreateTextureFromFileA(m_d3dDevice, StringUtility::ToString(mappedPath).c_str(), &pTexture);
//	if (FAILED(hr)) {
//		DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, L"Create texture from file failed");
//		return 0;
//	}

	stbi_image_free(data);

	uint32_t id = m_textureContainerMap.size() + 1;
	TextureContainer textureContainer;
	textureContainer.texture = texture;
	textureContainer.width = width;
	textureContainer.height = height;
	m_textureContainerMap.insert(std::pair<int, TextureContainer>(id, textureContainer));
	return id;
}

#endif