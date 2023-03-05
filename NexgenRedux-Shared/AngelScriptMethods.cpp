#include "AngelScriptMethods.h"
#include "AngelScriptRunner.h"
#include "WindowManager.h"
#include "JoystickManager.h"
#include "MathUtility.h"

#include <Gensys/DebugUtility.h>

#include <AngelScript/angelscript.h>
#include <AngelScript/addons/autowrapper/aswrappedcall.h>
#include <AngelScript/addons/scriptstdstring/scriptstdstring.h>
#include <AngelScript/addons/scriptmath/scriptmath.h>
#include <AngelScript/addons/scriptarray/scriptarray.h>
#include <AngelScript/addons/scriptdictionary/scriptdictionary.h>

using namespace Gensys;
using namespace NexgenRedux;
using namespace AngelScript;

void AngelScriptMethods::DebugPrint(asIScriptGeneric* generic)
{
	DebugUtility::LogLevel logLevel = (DebugUtility::LogLevel)generic->GetArgDWord(0);
	std::string *message = (std::string*)generic->GetArgAddress(1);
	DebugUtility::LogMessage(logLevel, *message);
}

void AngelScriptMethods::GetAvailableMonitorCount(asIScriptGeneric* generic)
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
	*(uint32_t*)generic->GetAddressOfReturnLocation() = monitorCount;
}

void AngelScriptMethods::GetMonitorVideoMode(asIScriptGeneric* generic)
{
	uint32_t monitorIndex = generic->GetArgDWord(0);
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
	generic->SetReturnObject(&monitorVideoMode);
}

void AngelScriptMethods::GetMonitorVideoModes(asIScriptGeneric* generic)
{
	uint32_t monitorIndex = generic->GetArgDWord(0);
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
	CScriptArray *array = CScriptArray::Create(arrayType, monitorVideoModes.size());
	for (uint32_t i = 0; i < monitorVideoModes.size(); i++)
	{
		(*(WindowManager::MonitorVideoMode*)array->At(i)) = monitorVideoModes.at(i);
	}
	generic->SetReturnObject(array);
	array->Release();
}

void AngelScriptMethods::WindowCreateWithSize(asIScriptGeneric* generic)
{
	uint32_t width = generic->GetArgDWord(0);
	uint32_t height = generic->GetArgDWord(1);
	std::string* title = (std::string*)generic->GetArgAddress(2);
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
	*(uint32_t*)generic->GetAddressOfReturnLocation() = windowHandle;
}

void AngelScriptMethods::GetWindowSize(asIScriptGeneric* generic)
{
	uint32_t windowHandle = generic->GetArgDWord(0);
	
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
	MathUtility::Size size;
	size.width = width;
	size.height = height;
	generic->SetReturnObject(&size);
}

void AngelScriptMethods::WindowCreateWithVideoMode(asIScriptGeneric* generic)
{
	WindowManager::MonitorVideoMode* monitorVideoMode = (WindowManager::MonitorVideoMode*)generic->GetArgObject(0);
	std::string* title = (std::string*)generic->GetArgAddress(1);
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
	*(uint32_t*)generic->GetAddressOfReturnLocation() = windowHandle;
}

void AngelScriptMethods::SetCursorMode(asIScriptGeneric* generic)
{
	uint32_t windowHandle = generic->GetArgDWord(0);
	uint32_t mode = generic->GetArgDWord(1);
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

void AngelScriptMethods::WindowClose(asIScriptGeneric* generic)
{
	uint32_t windowHandle = generic->GetArgDWord(0);
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

void AngelScriptMethods::GetClipboardString(asIScriptGeneric* generic)
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
	*(std::string*)generic->GetAddressOfReturnLocation() = value;
}

void AngelScriptMethods::GetKeyPressed(asIScriptGeneric* generic)
{
	uint32_t windowHandle = generic->GetArgDWord(0);
	uint32_t key = generic->GetArgDWord(1);

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
	*(uint32_t*)generic->GetAddressOfReturnLocation() = pressed;
}

void AngelScriptMethods::GetMouseButtonPressed(asIScriptGeneric* generic)
{
	uint32_t windowHandle = generic->GetArgDWord(0);
	uint32_t button = generic->GetArgDWord(1);

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
	*(uint32_t*)generic->GetAddressOfReturnLocation() = pressed;
}

void AngelScriptMethods::GetMouseCursorPosition(asIScriptGeneric* generic)
{
	uint32_t windowHandle = generic->GetArgDWord(0);

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

	MathUtility::Vec2D vec2D;
	vec2D.x = xPos;
	vec2D.y = yPos;
	generic->SetReturnObject(&vec2D);
}

void AngelScriptMethods::SetMouseCursorPosition(asIScriptGeneric* generic)
{
	uint32_t windowHandle = generic->GetArgDWord(0);
	double xPos = generic->GetArgDouble(1);
	double yPos = generic->GetArgDouble(2);

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

void AngelScriptMethods::SetClipboardString(asIScriptGeneric* generic)
{
	std::string* value = (std::string*)generic->GetArgObject(0);
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

void AngelScriptMethods::JoystickIsPresent(asIScriptGeneric* generic)
{
	uint32_t joystickID = generic->GetArgDWord(0);

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
	*(uint32_t*)generic->GetAddressOfReturnLocation() = present;
}

void AngelScriptMethods::JoystickIsGamepad(asIScriptGeneric* generic)
{
	uint32_t joystickID = generic->GetArgDWord(0);

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
	*(uint32_t*)generic->GetAddressOfReturnLocation() = gamepad;
}

void AngelScriptMethods::GetJoystickButtonStates(asIScriptGeneric* generic)
{
	uint32_t joystickID = generic->GetArgDWord(0);

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
	generic->SetReturnObject(&joystickButtonStates);
}

void AngelScriptMethods::GetJoystickAxisStates(asIScriptGeneric* generic)
{
	uint32_t joystickID = generic->GetArgDWord(0);

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
	generic->SetReturnObject(&joystickAxisStates);
}

// some reference tests

void AngelScriptMethods::Test1(asIScriptGeneric* generic)
{
	asIScriptContext *context = asGetActiveContext();
	asIScriptEngine *engine = context->GetEngine();
	asITypeInfo *arrayType = engine->GetTypeInfoByDecl("array<uint>");
	CScriptArray *array = CScriptArray::Create(arrayType, 3);
	(*(int*)array->At(0)) = 123;
	(*(int*)array->At(1)) = 234;
	(*(int*)array->At(2)) = 345;
	generic->SetReturnObject(array);
	array->Release();
}

void AngelScriptMethods::Test2(asIScriptGeneric* generic)
{
	AngelScriptRunner::Vec2 vec2;
	vec2.x = 1.123f;
	vec2.y = 2.234f;
	generic->SetReturnObject(&vec2);
}

void AngelScriptMethods::Test3(asIScriptGeneric* generic)
{
	asIScriptContext *context = asGetActiveContext();
	asIScriptEngine *engine = context->GetEngine();
	asITypeInfo *arrayType = engine->GetTypeInfoByDecl("array<Vec2>");
	CScriptArray *array = CScriptArray::Create(arrayType, 3);

	for (uint32_t i = 0; i < 3; i++)
	{
		AngelScriptRunner::Vec2 vec2;
		vec2.x = 1.123f + i;
		vec2.y = 2.234f + i;
		(*(AngelScriptRunner::Vec2*)array->At(i)) = vec2;
	}

	generic->SetReturnObject(array);
	array->Release();
}