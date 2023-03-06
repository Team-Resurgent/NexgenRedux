#include "JoystickManager.h"
#include "OpenGLDeviceHelper.h"
#include "XboxOGDeviceHelper.h"

#include <Gensys/Int.h>

using namespace NexgenRedux;

bool JoystickManager::JoystickIsPresent(uint32_t joystickID, uint32_t& present)
{
#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 
	return OpenGLDeviceHelper::JoystickIsPresent(joystickID, present);
#elif defined NEXGEN_OG
	return XboxOGDeviceHelper::JoystickIsPresent(joystickID, present);
#elif defined NEXGEN_360
	return true;
#else
	return false;
#endif
}

bool JoystickManager::JoystickIsGamepad(uint32_t joystickID, uint32_t& gamepad)
{
#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 
	return OpenGLDeviceHelper::JoystickIsGamepad(joystickID, gamepad);
#elif defined NEXGEN_OG
	return XboxOGDeviceHelper::JoystickIsGamepad(joystickID, gamepad);
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

bool JoystickManager::GetJoystickHatCount(uint32_t joystickID, uint32_t& count)
{
#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 
	return OpenGLDeviceHelper::GetJoystickHatCount(joystickID, count);
#elif defined NEXGEN_OG
	return XboxOGDeviceHelper::GetJoystickHatCount(joystickID, count);
#elif defined NEXGEN_360
	return true;
#else
	return false;
#endif
}

bool JoystickManager::GetJoystickHatDirection(uint32_t joystickID, uint32_t hatIndex, uint32_t& direction)
{
#if defined NEXGEN_WIN || defined NEXGEN_MAC || defined NEXGEN_LINUX 
	return OpenGLDeviceHelper::GetJoystickHatDirection(joystickID, hatIndex, direction);
#elif defined NEXGEN_OG
	return XboxOGDeviceHelper::GetJoystickHatDirection(joystickID, hatIndex, direction);
#elif defined NEXGEN_360
	return true;
#else
	return false;
#endif
}
