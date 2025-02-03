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

void PreLoader::LoadTexture(const std::string value)
{
	// Only load if we dont a ref to key ourselves
	if (std::find(m_textureKeys.begin(), m_textureKeys.end(), value) != m_textureKeys.end()) 
	{
        return;
    }

	RenderStateManager* renderStateManager = RenderStateManager::GetInstance();

	uint32_t textureID = 0;

	bool textureExists = renderStateManager->TextureExists(StringUtility::ToWideString(value), textureID);
	renderStateManager->CreateTextureReference(StringUtility::ToWideString(value), textureID);
	if (textureExists == false)
	{
		TextureManager::Request(StringUtility::ToWideString(value), textureID);
	}
}

void PreLoader::UnLoadTexture(const std::string value)
{
	// Only unload if we have a ref to key ourselves
	if (std::find(m_textureKeys.begin(), m_textureKeys.end(), value) == m_textureKeys.end()) 
	{
        return;
    }

	RenderStateManager* renderStateManager = RenderStateManager::GetInstance();

	uint32_t textureID = 0;

	bool textureExists = renderStateManager->TextureExists(StringUtility::ToWideString(value), textureID);
	if (textureExists == true)
	{
		renderStateManager->DeleteTextureReference(textureID);
	}
}

void PreLoader::WaitTexturesLoaded()
{
	// This should not be called until render engine / window created

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
				TimeUtility::SleepMilliseconds(100);
			}
		}
    }
}