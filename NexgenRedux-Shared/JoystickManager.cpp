#include "JoystickManager.h"
#include "OpenGLDeviceHelper.h"
#include "XboxOGDeviceHelper.h"

#include <Gensys/Int.h>

using namespace NexgenRedux;

bool JoystickManager::JoystickIsPresent(uint32_t joystickID, uint32_t& state)
{
#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 
	return OpenGLDeviceHelper::JoystickIsPresent(joystickID, state);
#elif defined NEXGEN_OG
	return XboxOGDeviceHelper::JoystickIsPresent(joystickID, state);
#elif defined NEXGEN_360
	return true;
#else
	return false;
#endif
}

bool JoystickManager::JoystickIsGamepad(uint32_t joystickID, uint32_t& state)
{
#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 
	return OpenGLDeviceHelper::JoystickIsGamepad(joystickID, state);
#elif defined NEXGEN_OG
	return XboxOGDeviceHelper::JoystickIsGamepad(joystickID, state);
#elif defined NEXGEN_360
	return true;
#else
	return false;
#endif
}

bool JoystickManager::GetJoystickButtonStates(uint32_t joystickID, JoystickButtonStates& joystickButtonStates)
{
#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 
	return OpenGLDeviceHelper::GetJoystickButtonStates(joystickID, joystickButtonStates);
#elif defined NEXGEN_OG
	return XboxOGDeviceHelper::GetJoystickButtonStates(joystickID, joystickButtonStates);
#elif defined NEXGEN_360
	return true;
#else
	return false;
#endif
}

bool JoystickManager::GetJoystickAxisStates(uint32_t joystickID, JoystickAxisStates& joystickAxisStates)
{
#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 
	return OpenGLDeviceHelper::GetJoystickAxisStates(joystickID, joystickAxisStates);
#elif defined NEXGEN_OG
	return XboxOGDeviceHelper::GetJoystickAxisStates(joystickID, joystickAxisStates);
#elif defined NEXGEN_360
	return true;
#else
	return false;
#endif
}