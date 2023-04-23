#include "TextureManager.h"
#include "ConfigLoader.h"

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
    std::vector<TextureManager::TextureRequest> m_textureRequestMap;
}
        
void TextureManager::Init()
{
    m_stopRequested = false;
    HANDLE thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)TestThread, NULL, 0, NULL);
}

void TextureManager::Close()
{
    m_stopRequested = true;

    // wait exit
	// for (std::map<TextureLoadedCallback, TextureRequest>::iterator it = m_textureRequestMap.begin(); it != m_textureRequestMap.end(); ++it)
	// {
	// 	if (it->second.data == NULL)
	// 	{
	// 		continue;
	// 	}
	// 	stbi_image_free(it->second.data);
	// }
}

void TextureManager::TestThread()
{
    while (m_stopRequested == false)
    {
        if (m_textureRequestMap.size() > 0)
        {
            TextureRequest textureRequest = m_textureRequestMap.front();

            std::wstring mappedPath;
            if (ConfigLoader::MapPath(textureRequest.path, mappedPath) == false)
            {
                continue;
            }
            
            int width;
            int height;
            int channels;
            unsigned char* data = stbi_load(StringUtility::ToString(mappedPath).c_str(), &width, &height, &channels, STBI_rgb_alpha);
            if (!data)
            {
                continue;
            }

            TextureLoadedCallback callback = (TextureLoadedCallback)textureRequest.callback;
            callback(textureRequest.instance, width, height, data);

            m_textureRequestMap.erase(m_textureRequestMap.begin());
        }
        TimeUtility::SleepMilliseconds(100);
    }
}

void TextureManager::Request(const std::wstring& path, void* instance, TextureLoadedCallback textureLoadedCallback)
{
	TextureRequest textureRequest;
	textureRequest.path = path;
    textureRequest.instance = instance;
    textureRequest.callback = textureLoadedCallback;
	m_textureRequestMap.push_back(textureRequest);
}

void TextureManager::FreeData(uint8_t* data)
{
    stbi_image_free(data);
}