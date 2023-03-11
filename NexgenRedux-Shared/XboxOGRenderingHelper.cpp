#if defined NEXGEN_OG

#include "XboxOGRenderingHelper.h"
#include "XboxOGWindowHelper.h"
#include "WindowManager.h"
#include "MeshUtility.h"
#include "ConfigLoader.h"

#include <Gensys/Int.h>
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

	d3dDevice->SetRenderState(D3DRS_TEXTUREFACTOR, D3DCOLOR_RGBA(255, 255, 255, 255));
	d3dDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	d3dDevice->SetRenderState(D3DRS_LIGHTING, FALSE);

	MathUtility::Vec3F eye = MathUtility::Vec3F(0, 0, 2);
	MathUtility::Vec3F target = MathUtility::Vec3F(0, 0, 0);
	MathUtility::Vec3F up = MathUtility::Vec3F(0, 1, 0);
	MathUtility::Matrix4x4 viewMatrix = MathUtility::Matrix4x4::LookAtLH(eye, target, up);
	MathUtility::Matrix4x4 modelMatrix = MathUtility::Matrix4x4();
	MathUtility::Matrix4x4 perspectiveMatrix = MathUtility::Matrix4x4::OrthoOffCenterLH(0, 640, 0, 480, 1, 100);

	D3DXMATRIX tempMatrix;
	
	MathUtility::Vec3F scale = MathUtility::Vec3F(-1.0f, 1.0f, 1.0f);
	MathUtility::Matrix4x4 scaleMatrix = MathUtility::Matrix4x4::Scale(scale);
	MathUtility::Matrix4x4 scaledViewMatrix = scaleMatrix * viewMatrix;

	tempMatrix = D3DXMATRIX(scaledViewMatrix.values);
	if (FAILED(d3dDevice->SetTransform(D3DTS_VIEW, &tempMatrix)))
	{
		return false;
	}

	tempMatrix = D3DXMATRIX(modelMatrix.values);
	if (FAILED(d3dDevice->SetTransform(D3DTS_WORLD, &tempMatrix)))
	{
		return false;
	}

	tempMatrix = D3DXMATRIX(perspectiveMatrix.values);
	if (FAILED(d3dDevice->SetTransform(D3DTS_PROJECTION, &tempMatrix)))
	{
		return false;
	}

	/*uint32_t value;
	GetShaderLookupValue("Default", "uModelMatrix", value);
	glUniformMatrix4fv(value, 1, GL_FALSE, modelMatrix.values);
	GetShaderLookupValue("Default", "uViewMatrix", value);
	glUniformMatrix4fv(value, 1, GL_FALSE, viewMatrix.values);
	GetShaderLookupValue("Default", "uProjectionMatrix", value);
	glUniformMatrix4fv(value, 1, GL_FALSE, perspectiveMatrix.values);
	GetShaderLookupValue("Default", "uCameraPosition", value);
	glUniform4f(value, -viewMatrix.values[12], -viewMatrix.values[13], -viewMatrix.values[14], 0);
	GetShaderLookupValue("Default", "uAmbientColor", value);
	glUniform4f(value, 0.1f, 0.1f, 0.1f, 0);
	GetShaderLookupValue("Default", "uLightPosition0", value);
	glUniform4f(value, 0, 0, 0, 0);
	GetShaderLookupValue("Default", "uLightDiffuseColor0", value);
	glUniform4f(value, 0, 0, 0, -1.0f);
	GetShaderLookupValue("Default", "uLightPosition1", value);
	glUniform4f(value, 0, 0, 0, 0);
	GetShaderLookupValue("Default", "uLightDiffuseColor1", value);
	glUniform4f(value, 0, 0, 0, -1.0f);
	GetShaderLookupValue("Default", "uLightPosition2", value);
	glUniform4f(value, 0, 0, 0, 0);
	GetShaderLookupValue("Default", "uLightDiffuseColor2", value);
	glUniform4f(value, 0, 0, 0, -1.0f);
	GetShaderLookupValue("Default", "uLightPosition3", value);
	glUniform4f(value, 0, 0, 0, 0);
	GetShaderLookupValue("Default", "uLightDiffuseColor3", value);
	glUniform4f(value, 0, 0, 0, -1.0f);
	GetShaderLookupValue("Default", "uLightsEnable", value);
	glUniform1i(value, 0);
	GetShaderLookupValue("Default", "uFogMode", value);
	glUniform1i(value, 0);
	GetShaderLookupValue("Default", "uFogColor", value);
	glUniform4f(value, 0, 0, 0, 1);
	GetShaderLookupValue("Default", "uFogStart", value);
	glUniform1f(value, 0);
	GetShaderLookupValue("Default", "uFogEnd", value);
	glUniform1f(value, 0);
	GetShaderLookupValue("Default", "uFogDensity", value);
	glUniform1f(value, 0);
	GetShaderLookupValue("Default", "uTintColor", value);
	glUniform4f(value, 1, 1, 1, 1);*/

	return true;
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

	TextureContainer* textureContainer = GetTextureContainer(mesh->textureID);
	if (textureContainer == NULL)
	{
		return false;
	}

	uint32_t requestedSize =  mesh->vertices.size() * sizeof(MeshUtility::Vertex);
	ResizeDynamicBufferIfNeeded(requestedSize);

	/*uint32_t aPosition;
	GetShaderLookupValue("Default", "aPosition", aPosition);
	uint32_t aNormal;
	GetShaderLookupValue("Default", "aNormal", aNormal);
	uint32_t aTexCoord;
	GetShaderLookupValue("Default", "aTexCoord", aTexCoord);

	glActiveTexture(GL_TEXTURE0 + 0);
	glBindTexture(GL_TEXTURE_2D, textureContainer->texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glBindBuffer(GL_ARRAY_BUFFER, m_dynamicBuffer);
	glBufferSubData(GL_ARRAY_BUFFER, 0, mesh->vertices.size() * sizeof(MeshUtility::Vertex), mesh->vertices.data());

	glBindBuffer(GL_ARRAY_BUFFER, m_dynamicBuffer);
	glEnableVertexAttribArray(aPosition);
	glVertexAttribPointer(aPosition, 3, GL_FLOAT, GL_FALSE, sizeof(MeshUtility::Vertex), (void*)offsetof(MeshUtility::Vertex, position));
	glEnableVertexAttribArray(aNormal);
	glVertexAttribPointer(aNormal, 3, GL_FLOAT, GL_FALSE, sizeof(MeshUtility::Vertex), (void*)offsetof(MeshUtility::Vertex, normal));
	glEnableVertexAttribArray(aTexCoord);
	glVertexAttribPointer(aTexCoord, 2, GL_FLOAT, GL_FALSE, sizeof(MeshUtility::Vertex), (void*)offsetof(MeshUtility::Vertex, texcoord));

	uint32_t vertexCount = mesh->vertices.size();
	glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vertexCount);*/

	IDirect3DDevice8* d3dDevice = (IDirect3DDevice8*)XboxOGWindowHelper::GetD3DDevice();

	if (FAILED(d3dDevice->SetTexture(0, textureContainer->texture))) 
	{
		return false;
	}

	if (FAILED(d3dDevice->SetTextureStageState(0, D3DTSS_MAGFILTER, D3DTEXF_POINT)))
	{
		return false;
	}

	if (FAILED(d3dDevice->SetTextureStageState(0, D3DTSS_MINFILTER, D3DTEXF_POINT)))
	{
		return false;
	}

	uint32_t vertexCount = mesh->vertices.size();
	d3dDevice->DrawPrimitiveUP(D3DPT_TRIANGLELIST, vertexCount / 3, &mesh->vertices[0], sizeof(MeshUtility::Vertex));
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