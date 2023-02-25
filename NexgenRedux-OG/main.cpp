#include "BootLoader.h"
#include "AngelScriptRunner.h"

#include <Gensys/DebugUtility.h>
#include <Gensys/StringUtility.h>
#include <Gensys/FileSystem.h>
#include <Gensys/Test.h>

#include <xtl.h>


using namespace NexgenRedux;
using namespace Gensys;
using namespace AngelScript;

void PrintString(std::string &str);

void __cdecl main(int, char **)
{
	Test::RunTests();

	BootLoader::Run();

	AngelScriptRunner runner = AngelScriptRunner();
	runner.Run();
}