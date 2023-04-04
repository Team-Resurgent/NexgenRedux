#include "FontManager.h"
#include "ConfigLoader.h"

#include <Gensys/DebugUtility.h>
#include <Gensys/StringUtility.h>
#include <Gensys/FileSystem.h>

#define SSFN_IMPLEMENTATION
#define SSFN_MAXLINES 4096
#define SSFN_memcmp memcmp
#define SSFN_memset memset
#define SSFN_realloc realloc
#define SSFN_free free
#include <SSFN/ssfn.h>

using namespace Gensys;
using namespace NexgenRedux;

namespace
{
    SSFN::Font* m_font = NULL;
}

void FontManager::Close(void)
{
    if (m_font != NULL)
    {
        delete m_font;
    }
}

bool FontManager::LoadFont(const std::string fontPath)
{
    if (m_font == NULL)
    {
        m_font = new SSFN::Font();
    }

    std::wstring mappedPath;
    if (ConfigLoader::MapPath(StringUtility::ToWideString(fontPath), mappedPath) == 0)
    {
        DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, StringUtility::FormatString("Failed to map path '%s'.", fontPath.c_str()));
        return false;
    }

    std::vector<uint8_t> buffer;
    if (FileSystem::FileReadAllAsData(mappedPath, buffer) == false)
    {
        DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, StringUtility::FormatString("Failed to load font file '%s'.", fontPath.c_str()));
        return false;
    }

    int status = m_font->Load(&buffer[0]);
    if (status != 0)
    {
        DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, StringUtility::FormatString("Failed to load font data '%s'.", m_font->ErrorStr(status).c_str()));
        return false;
    }

    return true;
}

bool FontManager::SelectFont(const std::string fontName, const uint32_t fontStyle, const uint32_t fontSize)
{
    if (m_font == NULL)
    {
        DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, "Failed to select font as no fonts have been loaded.");
        return false;
    }

    int status = m_font->Select(SSFN_FAMILY_ANY, fontName.c_str(), fontStyle, fontSize);
    if (status != 0)
    {
        DebugUtility::LogMessage(DebugUtility::LOGLEVEL_ERROR, StringUtility::FormatString("Failed to select font '%s'.", m_font->ErrorStr(status).c_str()));
        return false;
    }

    return true;
}

bool FontManager::RenderText(const std::string text, void** textHandle)
{
    ssfn_buf_t* buffer = m_font->Text(text.c_str(), 0xffffffff);
    *textHandle = (void*)buffer;
    return buffer != NULL;
}

void FontManager::GetTextPixelBuffer(void* textHandle, uint8_t** pixelBuffer, uint32_t& width, uint32_t& height)
{
    if (textHandle == NULL)
    {
        return;
    }
    ssfn_buf_t* buffer = (ssfn_buf_t*)textHandle;
    *pixelBuffer = buffer->ptr;
    width = buffer->w;
    height = buffer->h;
}

void FontManager::DeleteRenderText(void* textHandle)
{
    if (textHandle == NULL)
    {
        return;
    }
    ssfn_buf_t* buffer = (ssfn_buf_t*)textHandle;
	if (buffer->ptr != NULL)
	{
		free(buffer->ptr);
	}
	if (buffer != NULL)
	{
		free(buffer);
	}
}