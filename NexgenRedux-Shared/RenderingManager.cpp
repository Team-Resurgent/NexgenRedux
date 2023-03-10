#include "RenderingManager.h"
#include "OpenGLRenderingHelper.h"
#include "XboxOGRenderingHelper.h"

using namespace NexgenRedux;

bool RenderingManager::Init(uint32_t windowHandle)
{
#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 
	return OpenGLRenderingHelper::Init(windowHandle);
#elif defined NEXGEN_OG 
	return XboxOGRenderingHelper::Init(windowHandle);
#elif defined NEXGEN_360
	return true;
#else
	return false;
#endif
}