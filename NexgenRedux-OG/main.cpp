#include "BootLoader.h"
#include "angelscript.h"

#include "addons/autowrapper/aswrappedcall.h"
#include "addons/scriptstdstring/scriptstdstring.h"
#include "DebugUtility.h"
#include "StringUtility.h"
#include "FileSystem.h"
#include <xtl.h>


using namespace NexgenRedux;
using namespace Gensys;
using namespace AngelScript;

void PrintString(std::string &str);

void MessageCallback(const asSMessageInfo *msg, void *param)
{
	const char *type = "ERR ";
	if( msg->type == asMSGTYPE_WARNING ) 
		type = "WARN";
	else if( msg->type == asMSGTYPE_INFORMATION ) 
		type = "INFO";

	DebugUtility::LogMessage(DebugUtility::LOGLEVEL_INFO, StringUtility::ToWideString(type));
	DebugUtility::LogMessage(DebugUtility::LOGLEVEL_INFO, StringUtility::ToWideString(msg->section));
	DebugUtility::LogMessage(DebugUtility::LOGLEVEL_INFO, StringUtility::ToWideString(msg->message));

	//printf("%s (%d, %d) : %s : %s\n", msg->section, msg->row, msg->col, type, msg->message);
}


void LineCallback(asIScriptContext *ctx, unsigned long *timeOut)
{
	// If the time out is reached we abort the script
	if( *timeOut < timeGetTime() )
		ctx->Abort();

	// It would also be possible to only suspend the script,
	// instead of aborting it. That would allow the application
	// to resume the execution where it left of at a later 
	// time, by simply calling Execute() again.
}

// Function implementation with native calling convention
void PrintString(std::string &str)
{
	DebugUtility::LogMessage(DebugUtility::LOGLEVEL_INFO, StringUtility::ToWideString(str));
}

// Function implementation with generic script interface
void PrintString_Generic(asIScriptGeneric *gen)
{
	std::string *str = (std::string*)gen->GetArgAddress(0);
	DebugUtility::LogMessage(DebugUtility::LOGLEVEL_INFO, StringUtility::ToWideString(*str));
}

// Function wrapper is needed when native calling conventions are not supported
void timeGetTime_Generic(asIScriptGeneric *gen)
{
	gen->SetReturnDWord(timeGetTime());
}

void ConfigureEngine(asIScriptEngine *engine)
{
	int r;

	// Register the script string type
	// Look at the implementation for this function for more information  
	// on how to register a custom string type, and other object types.
	RegisterStdString(engine);

	//r = engine->RegisterGlobalFunction("void Print(string &in)", asFUNCTION(PrintString), asCALL_CDECL);
	//assert( r >= 0 );
	//r = engine->RegisterGlobalFunction("uint GetSystemTime()", asFUNCTION(timeGetTime), asCALL_STDCALL);
	//assert( r >= 0 );
		r = engine->RegisterGlobalFunction("void Print(string &in)", asFUNCTION(PrintString_Generic), asCALL_GENERIC);
		//r = engine->RegisterGlobalFunction("uint GetSystemTime()", asFUNCTION(timeGetTime_Generic), asCALL_GENERIC);

}

int CompileScript(asIScriptEngine *engine)
{
	int r;

	// We will load the script from a file on the disk.
	FILE *f = fopen("E:\\Devkit\\NexgenRedux-OG\\Media\\Default\\main.nxg", "rb");
	if( f == 0 )
	{
		//cout << "Failed to open the script file 'script.as'." << endl;
		return -1;
	}

	// Determine the size of the file	
	fseek(f, 0, SEEK_END);
	int len = ftell(f);
	fseek(f, 0, SEEK_SET);

	// On Win32 it is possible to do the following instead
	// int len = _filelength(_fileno(f));

	// Read the entire file
	std::string script;
	script.resize(len);
	size_t c = fread(&script[0], len, 1, f);
	fclose(f);

	if( c == 0 ) 
	{
		//cout << "Failed to load script file." << endl;
		return -1;
	}

	// Add the script sections that will be compiled into executable code.
	// If we want to combine more than one file into the same script, then 
	// we can call AddScriptSection() several times for the same module and
	// the script engine will treat them all as if they were one. The script
	// section name, will allow us to localize any errors in the script code.
	asIScriptModule *mod = engine->GetModule(0, asGM_ALWAYS_CREATE);
	r = mod->AddScriptSection("script", &script[0], len);
	if( r < 0 ) 
	{
		//cout << "AddScriptSection() failed" << endl;
		return -1;
	}
	
	// Compile the script. If there are any compiler messages they will
	// be written to the message stream that we set right after creating the 
	// script engine. If there are no errors, and no warnings, nothing will
	// be written to the stream.
	r = mod->Build();
	if( r < 0 )
	{
		//cout << "Build() failed" << endl;
		return -1;
	}

	// The engine doesn't keep a copy of the script sections after Build() has
	// returned. So if the script needs to be recompiled, then all the script
	// sections must be added again.

	// If we want to have several scripts executing at different times but 
	// that have no direct relation with each other, then we can compile them
	// into separate script modules. Each module use their own namespace and 
	// scope, so function names, and global variables will not conflict with
	// each other.

	return 0;
}


void __cdecl main(int, char **)
{

	BootLoader::Run();


	//FileSystem::FindInfo findInfo;
	//bool a = FileSystem::FileFindFirst(L"E:\\Devkit\\NexgenRedux-OG\\Media\\Default\\", findInfo); 
	std::vector<FileSystem::FileInfoDetail> fileInfoDetails;
	bool x = FileSystem::FileGetFileInfoDetails(L"E:\\Devkit\\NexgenRedux-OG\\Media\\Default", fileInfoDetails);

	asIScriptEngine *engine = asCreateScriptEngine();
	if( engine == 0 )
	{
		return;
	}

	// The script compiler will write any compiler messages to the callback.
	engine->SetMessageCallback(asFUNCTION(MessageCallback), 0, asCALL_CDECL);

	ConfigureEngine(engine);

	int r = CompileScript(engine);
	if( r < 0 )
	{
		engine->Release();
		return;
	}

	asIScriptContext *ctx = engine->CreateContext();
	if( ctx == 0 ) 
	{
		//cout << "Failed to create the context." << endl;
		engine->Release();
		return;
	}

	DWORD timeOut;
	r = ctx->SetLineCallback(asFUNCTION(LineCallback), &timeOut, asCALL_CDECL);
	if( r < 0 )
	{
		//cout << "Failed to set the line callback function." << endl;
		ctx->Release();
		engine->Release();
		return;
	}

	asIScriptFunction *func = engine->GetModule(0)->GetFunctionByDecl("float calc(float, float)");
	if( func == 0 )
	{
		//cout << "The function 'float calc(float, float)' was not found." << endl;
		ctx->Release();
		engine->Release();
		return;
	}

	r = ctx->Prepare(func);
	if( r < 0 ) 
	{
		//cout << "Failed to prepare the context." << endl;
		ctx->Release();
		engine->Release();
		return;
	}

	ctx->SetArgFloat(0, 3.14159265359f);
	ctx->SetArgFloat(1, 2.71828182846f);

	timeOut = timeGetTime() + 1000;
	r = ctx->Execute();
if( r != asEXECUTION_FINISHED )
	{
		// The execution didn't finish as we had planned. Determine why.
		if( r == asEXECUTION_ABORTED ) {
			int x =1;
			//cout << "The script was aborted before it could finish. Probably it timed out." << endl;
		}
		else if( r == asEXECUTION_EXCEPTION )
		{
			//cout << "The script ended with an exception." << endl;

			// Write some information about the script exception
			asIScriptFunction *func = ctx->GetExceptionFunction();
			//cout << "func: " << func->GetDeclaration() << endl;
			//cout << "modl: " << func->GetModuleName() << endl;
			//cout << "sect: " << func->GetScriptSectionName() << endl;
			//cout << "line: " << ctx->GetExceptionLineNumber() << endl;
			//cout << "desc: " << ctx->GetExceptionString() << endl;
		}
		else {
			int x =1;
		//	cout << "The script ended for some unforeseen reason (" << r << ")." << endl;
		}
	}
	else
	{
		// Retrieve the return value from the context
		float returnValue = ctx->GetReturnFloat();
		int x =1;
		//cout << "The script function returned: " << returnValue << endl;
	}

	ctx->Release();
	engine->ShutDownAndRelease();
}