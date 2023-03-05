#pragma once

#include <Gensys/Int.h>

namespace NexgenRedux
{
	class JoystickManager
	{		
	public:
		static bool JoystickIsPresent(uint32_t joystickID, uint32_t& state);
		static bool JoystickIsGamepad(uint32_t joystickID, uint32_t& state);
	};
}
