#pragma once

#include <Gensys/Int.h>

namespace NexgenRedux
{
	class JoystickManager
	{		
	public:

		typedef struct JoystickButtonStates
		{
			uint32_t buttonA;
			uint32_t buttonB;
			uint32_t buttonX;
			uint32_t buttonY; 
			uint32_t buttonLeftBumper;
			uint32_t buttonRightBumper;
			uint32_t buttonBack;
			uint32_t buttonStart;
			uint32_t buttonGuide;
			uint32_t buttonLeftThumb;
			uint32_t buttonRightThumb;
			uint32_t buttonDpadUp;
			uint32_t buttonDpadRight;
			uint32_t buttonDpadDown;
			uint32_t buttonDpadLeft;
		} JoystickButtonStates;

		typedef struct JoystickAxisStates
		{
			float axisLeftX;
			float axisLeftY;
			float axisRightX;
			float axisRightY; 
			float axisLeftTrigger;
			float axisRightTrigger;
		} JoystickAxisStates;

		static bool JoystickIsPresent(uint32_t joystickID, uint32_t& present);
		static bool JoystickIsGamepad(uint32_t joystickID, uint32_t& gamepad);
		static bool GetJoystickButtonStates(uint32_t joystickID, JoystickButtonStates& joystickButtonStates);
		static bool GetJoystickAxisStates(uint32_t joystickID, JoystickAxisStates& joystickAxisStates);
	};
}
