#include "PreLoader.h"
#include "RenderStateManager.h"
#include "TextureManager.h"

#include <Gensys/DebugUtility.h>
#include <Gensys/StringUtility.h>
#include <Gensys/TimeUtility.h>
#include <stdexcept>
#include <algorithm>
#include <string>
#include <vector>

using namespace Gensys;
using namespace NexgenRedux;

namespace
{
    std::vector<std::string> m_textureKeys;
}

void PreLoader::Close(void)
{
	RenderStateManager* renderStateManager = RenderStateManager::GetInstance();

	for (size_t i = 0; i < m_textureKeys.size(); ++i)
	{
		const std::string value = m_textureKeys[i];

		uint32_t textureID = 0;
		bool textureExists = renderStateManager->TextureExists(StringUtility::ToWideString(value), textureID);
		if (textureExists == true)
		{
			renderStateManager->DeleteTextureReference(textureID);
		}
    }
}

bool PreLoader::LoadTexture(const std::string value)
{
	// Only load if we dont a ref to key ourselves
	if (std::find(m_textureKeys.begin(), m_textureKeys.end(), value) != m_textureKeys.end()) 
	{
        return false;
    }

	m_textureKeys.push_back(value);

	RenderStateManager* renderStateManager = RenderStateManager::GetInstance();

	uint32_t textureID = 0;

	bool textureExists = renderStateManager->TextureExists(StringUtility::ToWideString(value), textureID);
	renderStateManager->CreateTextureReference(StringUtility::ToWideString(value), textureID);
	if (textureExists == false)
	{
		TextureManager::Request(StringUtility::ToWideString(value), textureID);
	}

	return true;
}

bool PreLoader::UnLoadTexture(const std::string value)
{
	// Only unload if we have a ref to key ourselves
	std::vector<std::string>::iterator it = std::find(m_textureKeys.begin(), m_textureKeys.end(), value);
	if (it == m_textureKeys.end()) 
	{
        return false;
    }

	m_textureKeys.erase(it);

	RenderStateManager* renderStateManager = RenderStateManager::GetInstance();

	uint32_t textureID = 0;

	bool textureExists = renderStateManager->TextureExists(StringUtility::ToWideString(value), textureID);
	if (textureExists == true)
	{
		renderStateManager->DeleteTextureReference(textureID);
	}

	return true;
}

bool PreLoader::AllTexturesLoaded()
{
	// This should only be called from render loop otherwise texture will be waiting to load for ever

	RenderStateManager* renderStateManager = RenderStateManager::GetInstance();

	for (size_t i = 0; i < m_textureKeys.size(); ++i)
	{
		const std::string value = m_textureKeys[i];

		uint32_t textureID = 0;
		bool textureExists = renderStateManager->TextureExists(StringUtility::ToWideString(value), textureID);
		if (textureExists == true)
		{
			while (renderStateManager->IsTextureLoaded(textureID) == false)
			{
				return false;
			}
		}
    }

	return true;
}