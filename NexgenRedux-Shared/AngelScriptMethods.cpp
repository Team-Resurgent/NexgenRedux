#include "AngelScriptMethods.h"
#include "AngelScriptRunner.h"
#include "WindowManager.h"
#include "JoystickManager.h"
#include "MathUtility.h"

#include <Gensys/DebugUtility.h>
#include <Gensys/TimeUtility.h>

#include <AngelScript/angelscript.h>
#include <AngelScript/addons/autowrapper/aswrappedcall.h>
#include <AngelScript/addons/scriptstdstring/scriptstdstring.h>
#include <AngelScript/addons/scriptmath/scriptmath.h>
#include <AngelScript/addons/scriptarray/scriptarray.h>
#include <AngelScript/addons/scriptdictionary/scriptdictionary.h>

using namespace Gensys;
using namespace NexgenRedux;
using namespace AngelScript;

void AngelScriptMethods::DebugPrint(asIScriptGeneric* gen)
{
	DebugUtility::LogLevel logLevel = (DebugUtility::LogLevel)gen->GetArgDWord(0);
	std::string *message = (std::string*)gen->GetArgAddress(1);
	DebugUtility::LogMessage(logLevel, *message);
}

void AngelScriptMethods::DebugPrintIf(asIScriptGeneric* gen)
{
	bool condition = gen->GetArgByte(0) == 1;
	if (condition == true) 
	{
		DebugUtility::LogLevel logLevel = (DebugUtility::LogLevel)gen->GetArgDWord(1);
		std::string *message = (std::string*)gen->GetArgAddress(2);
		DebugUtility::LogMessage(logLevel, *message);
	}
}

void AngelScriptMethods::GetAvailableMonitorCount(asIScriptGeneric* gen)
{
	uint32_t monitorCount;
	if (WindowManager::GetAvailableMonitorCount(monitorCount) == false)
	{
		asIScriptContext *context = asGetActiveContext();
		if (context) 
		{
			context->SetException("GetAvailableMonitorCount failed.");
			return;
		}
	}
	*(uint32_t*)gen->GetAddressOfReturnLocation() = monitorCount;
}

void AngelScriptMethods::GetMonitorVideoMode(asIScriptGeneric* gen)
{
	uint32_t monitorIndex = gen->GetArgDWord(0);
	WindowManager::MonitorVideoMode monitorVideoMode;
	if (WindowManager::GetMonitorVideoMode(monitorIndex, monitorVideoMode) == false)
	{
		asIScriptContext *context = asGetActiveContext();
		if (context) 
		{
			context->SetException("GetMonitorVideoMode failed.");
			return;
		}
	}
	gen->SetReturnObject(&monitorVideoMode);
}

void AngelScriptMethods::GetMonitorVideoModes(asIScriptGeneric* gen)
{
	uint32_t monitorIndex = gen->GetArgDWord(0);
	std::vector<WindowManager::MonitorVideoMode> monitorVideoModes;
	if (WindowManager::GetMonitorVideoModes(monitorIndex, monitorVideoModes) == false)
	{
		asIScriptContext *context = asGetActiveContext();
		if (context) 
		{
			context->SetException("GetMonitorVideoMode failed.");
			return;
		}
	}
	asIScriptContext *context = asGetActiveContext();
	asIScriptEngine *engine = context->GetEngine();
	asITypeInfo *arrayType = engine->GetTypeInfoByDecl("array<MonitorVideoMode>");
	CScriptArray *array = CScriptArray::Create(arrayType, (uint32_t)monitorVideoModes.size());
	for (uint32_t i = 0; i < monitorVideoModes.size(); i++)
	{
		(*(WindowManager::MonitorVideoMode*)array->At(i)) = monitorVideoModes.at(i);
	}
	gen->SetReturnObject(array);
	array->Release();
}

void AngelScriptMethods::WindowCreateWithSize(asIScriptGeneric* gen)
{
	uint32_t width = gen->GetArgDWord(0);
	uint32_t height = gen->GetArgDWord(1);
	std::string* title = (std::string*)gen->GetArgAddress(2);
	uint32_t windowHandle;
	if (WindowManager::WindowCreateWithSize(width, height, *title, windowHandle) == false) 
	{
		asIScriptContext *context = asGetActiveContext();
		if (context) 
		{
			context->SetException("WindowCreateWithSize failed.");
			return;
		}
	}
	*(uint32_t*)gen->GetAddressOfReturnLocation() = windowHandle;
}

void AngelScriptMethods::GetWindowSize(asIScriptGeneric* gen)
{
	uint32_t windowHandle = gen->GetArgDWord(0);
	
	uint32_t width;
	uint32_t height;
	if (WindowManager::GetWindowSize(windowHandle, width, height) == false) 
	{
		asIScriptContext *context = asGetActiveContext();
		if (context) 
		{
			context->SetException("GetWindowSize failed.");
			return;
		}
	}
	MathUtility::SizeI size;
	size.width = width;
	size.height = height;
	gen->SetReturnObject(&size);
}

void AngelScriptMethods::WindowCreateWithVideoMode(asIScriptGeneric* gen)
{
	WindowManager::MonitorVideoMode* monitorVideoMode = (WindowManager::MonitorVideoMode*)gen->GetArgObject(0);
	std::string* title = (std::string*)gen->GetArgAddress(1);
	uint32_t windowHandle;
	if (WindowManager::WindowCreateWithVideoMode(*monitorVideoMode, *title, windowHandle) == false) 
	{
		asIScriptContext *context = asGetActiveContext();
		if (context) 
		{
			context->SetException("WindowCreateWithVideoMode failed.");
			return;
		}
	}
	*(uint32_t*)gen->GetAddressOfReturnLocation() = windowHandle;
}

void AngelScriptMethods::SetCursorMode(asIScriptGeneric* gen)
{
	uint32_t windowHandle = gen->GetArgDWord(0);
	uint32_t mode = gen->GetArgDWord(1);
	if (WindowManager::SetCursorMode(windowHandle, mode) == false) 
	{
		asIScriptContext *context = asGetActiveContext();
		if (context) 
		{
			context->SetException("SetCursorMode failed.");
			return;
		}
	}
}

void AngelScriptMethods::WindowClose(asIScriptGeneric* gen)
{
	uint32_t windowHandle = gen->GetArgDWord(0);
	if (WindowManager::WindowClose(windowHandle) == false) 
	{
		asIScriptContext *context = asGetActiveContext();
		if (context) 
		{
			context->SetException("WindowClose failed.");
			return;
		}
	}
}

void AngelScriptMethods::GetClipboardString(asIScriptGeneric* gen)
{
	std::string value;
	if (WindowManager::GetClipboardString(value) == false) 
	{
		asIScriptContext *context = asGetActiveContext();
		if (context) 
		{
			context->SetException("GetClipboardString failed.");
			return;
		}
	}
	*(std::string*)gen->GetAddressOfReturnLocation() = value;
}

void AngelScriptMethods::GetKeyPressed(asIScriptGeneric* gen)
{
	uint32_t windowHandle = gen->GetArgDWord(0);
	uint32_t key = gen->GetArgDWord(1);

	uint32_t pressed;
	if (WindowManager::GetKeyPressed(windowHandle, key, pressed) == false) 
	{
		asIScriptContext *context = asGetActiveContext();
		if (context) 
		{
			context->SetException("GetKeyPressed failed.");
			return;
		}
	}
	*(uint32_t*)gen->GetAddressOfReturnLocation() = pressed;
}

void AngelScriptMethods::GetMouseButtonPressed(asIScriptGeneric* gen)
{
	uint32_t windowHandle = gen->GetArgDWord(0);
	uint32_t button = gen->GetArgDWord(1);

	uint32_t pressed;
	if (WindowManager::GetMouseButtonPressed(windowHandle, button, pressed) == false) 
	{
		asIScriptContext *context = asGetActiveContext();
		if (context) 
		{
			context->SetException("GetMouseButtonPressed failed.");
			return;
		}
	}
	*(uint32_t*)gen->GetAddressOfReturnLocation() = pressed;
}

void AngelScriptMethods::GetMouseCursorPosition(asIScriptGeneric* gen)
{
	uint32_t windowHandle = gen->GetArgDWord(0);

	double xPos;
	double yPos;
	if (WindowManager::GetMouseCursorPosition(windowHandle, xPos, yPos) == false) 
	{
		asIScriptContext *context = asGetActiveContext();
		if (context) 
		{
			context->SetException("GetMouseCursorPosition failed.");
			return;
		}
	}

	MathUtility::Vec2D vec2D = MathUtility::Vec2D(xPos, yPos);
	gen->SetReturnObject(&vec2D);
}

void AngelScriptMethods::SetMouseCursorPosition(asIScriptGeneric* gen)
{
	uint32_t windowHandle = gen->GetArgDWord(0);
	double xPos = gen->GetArgDouble(1);
	double yPos = gen->GetArgDouble(2);

	std::string value;
	if (WindowManager::SetMouseCursorPosition(windowHandle, xPos, yPos) == false) 
	{
		asIScriptContext *context = asGetActiveContext();
		if (context) 
		{
			context->SetException("SetMouseCursorPosition failed.");
			return;
		}
	}
}

void AngelScriptMethods::SetClipboardString(asIScriptGeneric* gen)
{
	std::string* value = (std::string*)gen->GetArgObject(0);
	if (WindowManager::SetClipboardString(*value) == false) 
	{
		asIScriptContext *context = asGetActiveContext();
		if (context) 
		{
			context->SetException("SetClipboardString failed.");
			return;
		}
	}
}

void AngelScriptMethods::GetTimeNow(asIScriptGeneric* gen)
{
	TimeUtility::Time time = TimeUtility::GetTimeNow();
	gen->SetReturnObject(&time);
}

void AngelScriptMethods::GetMillisecondsNow(asIScriptGeneric* gen)
{
	uint64_t millisecondsNow = TimeUtility::GetMillisecondsNow();
	*(uint64_t*)gen->GetAddressOfReturnLocation() = millisecondsNow;
}

void AngelScriptMethods::GetDurationSeconds(asIScriptGeneric* gen)
{
	uint64_t start = gen->GetArgQWord(0);
	uint64_t end = gen->GetArgQWord(1);
	double durationSeconds = TimeUtility::GetDurationSeconds(start, end);
	*(double*)gen->GetAddressOfReturnLocation() = durationSeconds;
}

void AngelScriptMethods::CalculateFramesPerSecond(asIScriptGeneric* gen)
{
	uint32_t frameCount = gen->GetArgDWord(0);
	double duration = gen->GetArgDouble(1);
	double fps = TimeUtility::CalculateFramesPerSecond(frameCount, duration);
	*(double*)gen->GetAddressOfReturnLocation() = fps;
}

void AngelScriptMethods::SleepMilliseconds(asIScriptGeneric* gen)
{
	uint32_t milliseconds = gen->GetArgDWord(0);
	TimeUtility::SleepMilliseconds(milliseconds);
}

void AngelScriptMethods::SeedRandom(asIScriptGeneric* gen)
{
	MathUtility::SeedRandom();
}

void AngelScriptMethods::SeedRandomWithValue(asIScriptGeneric* gen)
{
	uint32_t value = gen->GetArgDWord(0);
	MathUtility::SeedRandomWithValue(value);
}

void AngelScriptMethods::GetRandomDouble(asIScriptGeneric* gen)
{
	double value = MathUtility::GetRandomDouble();
	*(double*)gen->GetAddressOfReturnLocation() = value;
}

void AngelScriptMethods::GetRandomIntInRange(asIScriptGeneric* gen)
{
	int32_t start = gen->GetArgDWord(0);
	int32_t end = gen->GetArgDWord(1);
	int32_t value = MathUtility::GetRandomIntInRange(start, end);
	*(int32_t*)gen->GetAddressOfReturnLocation() = value;
}

void AngelScriptMethods::JoystickIsPresent(asIScriptGeneric* gen)
{
	uint32_t joystickID = gen->GetArgDWord(0);

	uint32_t present;
	if (JoystickManager::JoystickIsPresent(joystickID, present) == false) 
	{
		asIScriptContext *context = asGetActiveContext();
		if (context) 
		{
			context->SetException("JoystickIsPresent failed.");
			return;
		}
	}
	*(uint32_t*)gen->GetAddressOfReturnLocation() = present;
}

void AngelScriptMethods::JoystickIsGamepad(asIScriptGeneric* gen)
{
	uint32_t joystickID = gen->GetArgDWord(0);

	uint32_t gamepad;
	if (JoystickManager::JoystickIsGamepad(joystickID, gamepad) == false) 
	{
		asIScriptContext *context = asGetActiveContext();
		if (context) 
		{
			context->SetException("JoystickIsPresent failed.");
			return;
		}
	}
	*(uint32_t*)gen->GetAddressOfReturnLocation() = gamepad;
}

void AngelScriptMethods::GetJoystickButtonStates(asIScriptGeneric* gen)
{
	uint32_t joystickID = gen->GetArgDWord(0);

	JoystickManager::JoystickButtonStates joystickButtonStates;
	if (JoystickManager::GetJoystickButtonStates(joystickID, joystickButtonStates) == false)
	{
		asIScriptContext *context = asGetActiveContext();
		if (context) 
		{
			context->SetException("GetJoystickButtonStates failed.");
			return;
		}
	}
	gen->SetReturnObject(&joystickButtonStates);
}

void AngelScriptMethods::GetJoystickAxisStates(asIScriptGeneric* gen)
{
	uint32_t joystickID = gen->GetArgDWord(0);

	JoystickManager::JoystickAxisStates joystickAxisStates;
	if (JoystickManager::GetJoystickAxisStates(joystickID, joystickAxisStates) == false)
	{
		asIScriptContext *context = asGetActiveContext();
		if (context) 
		{
			context->SetException("GetJoystickAxisStates failed.");
			return;
		}
	}
	gen->SetReturnObject(&joystickAxisStates);
}

void AngelScriptMethods::GetJoystickHatCount(asIScriptGeneric* gen)
{
	uint32_t joystickID = gen->GetArgDWord(0);

	uint32_t count;
	if (JoystickManager::GetJoystickHatCount(joystickID, count) == false)
	{
		asIScriptContext *context = asGetActiveContext();
		if (context) 
		{
			context->SetException("GetJoystickHatCount failed.");
			return;
		}
	}
	*(uint32_t*)gen->GetAddressOfReturnLocation() = count;
}

void AngelScriptMethods::GetJoystickHatDirection(asIScriptGeneric* gen)
{
	uint32_t joystickID = gen->GetArgDWord(0);
	uint32_t hatIndex = gen->GetArgDWord(1);

	uint32_t direction;
	if (JoystickManager::GetJoystickHatDirection(joystickID, hatIndex, direction) == false)
	{
		asIScriptContext *context = asGetActiveContext();
		if (context) 
		{
			context->SetException("GetJoystickHatDirection failed.");
			return;
		}
	}
	*(uint32_t*)gen->GetAddressOfReturnLocation() = direction;
}