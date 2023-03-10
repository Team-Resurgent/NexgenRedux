#if defined NEXGEN_OG

#include "XboxOGRenderingHelper.h"
#include "WindowManager.h"

#include <Gensys/Int.h>

#include <xtl.h>

using namespace NexgenRedux;

bool XboxOGRenderingHelper::Init(uint32_t windowHandle)
{
	WindowManager::WindowContainer* windowContainer = WindowManager::GetWindowContainer(windowHandle);
	IDirect3DDevice8* d3dDevice = (IDirect3DDevice8*)windowContainer->window;

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

#endif