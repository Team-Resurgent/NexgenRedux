#include "TextureManager.h"
#include "ConfigLoader.h"
#include "RenderStateManager.h"

#include <Gensys/TimeUtility.h>
#include <Gensys/StringUtility.h>

#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ASSERT(x)
#include <STB/stb_image.h>

#if defined NEXGEN_WIN
#include <windows.h>
#elif defined NEXGEN_OG
#include <xtl.h>
#endif

using namespace Gensys;
using namespace NexgenRedux;

namespace
{
    bool m_stopRequested;
    bool m_stopped;
    std::vector<TextureManager::TextureRequest> m_textureRequestMap;
	HANDLE m_mutex = NULL; 
}
        
void TextureManager::Init()
{
    m_stopRequested = false;
    m_stopped = false;
	m_mutex = CreateMutex(NULL, FALSE, NULL);
    HANDLE thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ProcessThread, NULL, 0, NULL);
}

void TextureManager::Close()
{
    m_stopRequested = true;
    while (m_stopped == false)
    {
        TimeUtility::SleepMilliseconds(100);
    }
	if (m_mutex != NULL)
	{
		CloseHandle(m_mutex);
	}
}

void TextureManager::ProcessThread()
{
    while (m_stopRequested == false)
    {
        if (m_textureRequestMap.size() > 0)
        {
			WaitForSingleObject(m_mutex, INFINITE);
            TextureRequest* textureRequest = &m_textureRequestMap.front();
			ReleaseMutex(m_mutex);

            if (textureRequest->data != NULL)
            {
                TimeUtility::SleepMilliseconds(100);
                continue;
            }

            std::wstring mappedPath;
            if (ConfigLoader::MapPath(textureRequest->path, mappedPath) == false)
            {
                textureRequest->processed = true;
                TimeUtility::SleepMilliseconds(100);
                continue;
            }
            
            textureRequest->data = stbi_load(StringUtility::ToString(mappedPath).c_str(), &textureRequest->width, &textureRequest->height, NULL, STBI_rgb_alpha);
            textureRequest->processed = true;

        }
        TimeUtility::SleepMilliseconds(100);
    }

    m_stopped = true;
}

void TextureManager::Request(const std::wstring& path, uint32_t textureID)
{
	TextureRequest textureRequest;
	textureRequest.path = path;
    textureRequest.processed = false;
    textureRequest.textureID = textureID;
    textureRequest.data = NULL;
    textureRequest.width = 0;
    textureRequest.height = 0;
	m_textureRequestMap.push_back(textureRequest);
}

void TextureManager::ApplyTextures()
{
    if (m_textureRequestMap.size() > 0)
    {
        TextureRequest* textureRequest = &m_textureRequestMap.front();
        if (textureRequest->processed == false)
        {
            return;
        }

        if (textureRequest->data != NULL)
        {
            RenderStateManager* renderStateManager = RenderStateManager::GetInstance();
            renderStateManager->LoadTextureData(textureRequest->textureID, textureRequest->data, textureRequest->width, textureRequest->height);
            stbi_image_free(textureRequest->data);
        }

		WaitForSingleObject(m_mutex, INFINITE);
        m_textureRequestMap.erase(m_textureRequestMap.begin());
		ReleaseMutex(m_mutex);
    }
}
