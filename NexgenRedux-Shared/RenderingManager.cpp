#include "RenderingManager.h"
#include "OpenGLRenderingHelper.h"
#include "XboxOGRenderingHelper.h"

using namespace NexgenRedux;

void RenderingManager::Close(void)
{
#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 
	OpenGLRenderingHelper::Close();
#elif defined NEXGEN_OG 
	XboxOGRenderingHelper::Close();
#elif defined NEXGEN_360
	return;
#endif
}

bool RenderingManager::SetShader(std::string shaderName)
{
#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 
	return OpenGLRenderingHelper::SetShader(shaderName);
#elif defined NEXGEN_OG 
	return XboxOGRenderingHelper::SetShader(shaderName);
#elif defined NEXGEN_360
	return true;
#else
	return false;
#endif
}