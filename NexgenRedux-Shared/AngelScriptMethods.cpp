#include "AngelScriptMethods.h"
#include "AngelScriptRunner.h"
#include "WindowManager.h"
#include "IWindowHelper.h"
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

void AngelScriptMethods::GetAvailableMonitorCount(asIScriptGeneric* gen)
{
	uint32_t monitorCount;
	if (WindowManager::GetInstance()->GetAvailableMonitorCount(monitorCount) == false)
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
	MonitorVideoMode monitorVideoMode;
	if (WindowManager::GetInstance()->GetMonitorVideoMode(monitorIndex, monitorVideoMode) == false)
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
	std::vector<MonitorVideoMode> monitorVideoModes;
	if (WindowManager::GetInstance()->GetMonitorVideoModes(monitorIndex, monitorVideoModes) == false)
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
		(*(MonitorVideoMode*)array->At(i)) = monitorVideoModes.at(i);
	}
	gen->SetReturnObject(array);
	array->Release();
}

void AngelScriptMethods::WindowCreateWithSize(asIScriptGeneric* gen)
{
	uint32_t width = gen->GetArgDWord(0);
	uint32_t height = gen->GetArgDWord(1);
	std::string* title = (std::string*)gen->GetArgAddress(2);
	if (WindowManager::GetInstance()->WindowCreateWithSize(width, height, *title) == false) 
	{
		asIScriptContext *context = asGetActiveContext();
		if (context) 
		{
			context->SetException("WindowCreateWithSize failed.");
			return;
		}
	}
}

void AngelScriptMethods::GetWindowSize(asIScriptGeneric* gen)
{
	uint32_t width;
	uint32_t height;
	if (WindowManager::GetInstance()->GetWindowSize(width, height) == false) 
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
	MonitorVideoMode* monitorVideoMode = (MonitorVideoMode*)gen->GetArgObject(0);
	std::string* title = (std::string*)gen->GetArgAddress(1);
	if (WindowManager::GetInstance()->WindowCreateWithVideoMode(*monitorVideoMode, *title) == false) 
	{
		asIScriptContext *context = asGetActiveContext();
		if (context) 
		{
			context->SetException("WindowCreateWithVideoMode failed.");
			return;
		}
	}
}

void AngelScriptMethods::SetCursorMode(asIScriptGeneric* gen)
{
	CursorMode mode = (CursorMode)gen->GetArgDWord(0);
	if (WindowManager::GetInstance()->SetCursorMode(mode) == false) 
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
	if (WindowManager::GetInstance()->WindowClose() == false) 
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
	if (WindowManager::GetInstance()->GetClipboardString(value) == false) 
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
	uint32_t key = gen->GetArgDWord(0);

	uint32_t pressed;
	if (WindowManager::GetInstance()->GetKeyPressed(key, pressed) == false) 
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
	uint32_t button = gen->GetArgDWord(0);

	uint32_t pressed;
	if (WindowManager::GetInstance()->GetMouseButtonPressed(button, pressed) == false) 
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
	double xPos;
	double yPos;
	if (WindowManager::GetInstance()->GetWindowHelper()->GetMouseCursorPosition(xPos, yPos) == false) 
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
	double xPos = gen->GetArgDouble(0);
	double yPos = gen->GetArgDouble(1);

	std::string value;
	if (WindowManager::GetInstance()->SetMouseCursorPosition(xPos, yPos) == false) 
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
	if (WindowManager::GetInstance()->SetClipboardString(*value) == false) 
	{
		asIScriptContext *context = asGetActiveContext();
		if (context) 
		{
			context->SetException("SetClipboardString failed.");
			return;
		}
	}
}

void AngelScriptMethods::JoystickIsPresent(asIScriptGeneric* gen)
{
	uint32_t joystickID = gen->GetArgDWord(0);

	uint32_t present;
	if (WindowManager::GetInstance()->GetWindowHelper()->JoystickIsPresent(joystickID, present) == false) 
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
	if (WindowManager::GetInstance()->GetWindowHelper()->JoystickIsGamepad(joystickID, gamepad) == false) 
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

	JoystickButtonStates joystickButtonStates;
	if (WindowManager::GetInstance()->GetWindowHelper()->GetJoystickButtonStates(joystickID, joystickButtonStates) == false)
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

	JoystickAxisStates joystickAxisStates;
	if (WindowManager::GetInstance()->GetWindowHelper()->GetJoystickAxisStates(joystickID, joystickAxisStates) == false)
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
	if (WindowManager::GetInstance()->GetWindowHelper()->GetJoystickHatCount(joystickID, count) == false)
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

	if (WindowManager::GetInstance()->GetWindowHelper()->GetJoystickHatDirection(joystickID, hatIndex, direction) == false)
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