#pragma once

#include "AngelScriptDebuggerCore.h"

#include <AngelScript/angelscript.h>

#include <vector>

using namespace AngelScript;

namespace NexgenRedux
{
	class AngelScriptDebugger
	{
	public:
		
		static void Init(asIScriptEngine *engine);
		static void Close();

		static asIScriptContext* RequestContextCallback(asIScriptEngine *engine, void * /*param*/);
		static void ReturnContextCallback(asIScriptEngine *engine, asIScriptContext *context, void * /*param*/);

	private:

		static std::string AngelScriptDebugger::StringToString(void *obj, int /* expandMembers */, AngelScriptDebuggerCore * /* dbg */);
		static std::string AngelScriptDebugger::ArrayToString(void *obj, int expandMembers, AngelScriptDebuggerCore *dbg);
		static std::string AngelScriptDebugger::DictionaryToString(void *obj, int expandMembers, AngelScriptDebuggerCore *dbg);

	private:


    };
}

// #pragma once

// #include <AngelScript/angelscript.h>
// #include <Gensys/SocketUtility.h>

// #include <string>
// #include <vector>
// #include <functional>
// #include <thread>
// #include <mutex>

// using namespace AngelScript;
// using namespace Gensys;

// namespace NexgenRedux
// {
// 	class AngelScriptDebugHost
// 	{
// 	public:

    
//     	typedef std::function<std::string(void*)> FuncEncoder;
// 	    typedef std::function<void(void*, const char*)> FuncDecoder;

//         static void Send_Path(SocketUtility* sock);
//         static void Send_Location(SocketUtility* sock);
//         static void Send_LocalClear(SocketUtility* sock);
//         static void Send_Local(SocketUtility* sock, const char* name, int typeID, void* ptr);
//         static void Send_AllLocals(SocketUtility* sock);
//         static void Send_Callstack(SocketUtility* sock);
//         static void Send_Breakpoints(SocketUtility* sock);
//         static void ScriptLineCallback(asIScriptContext* ctx);
//         static void ClientPacket_Step(SocketUtility* sock);
//         static void ClientPacket_StepOver(SocketUtility* sock);
//         static void ClientPacket_StepOut(SocketUtility* sock);
//         static void ClientPacket_Pause(SocketUtility* sock);
//         static void ClientPacket_Resume(SocketUtility* sock);
//         static void ClientPacket_AddBreakpoint(SocketUtility* sock);
//         static void ClientPacket_DeleteBreakpoint(SocketUtility* sock);
//         static void ClientPacket_SetValue(SocketUtility* sock);
//         static void ClientThreadFunction(SocketUtility* sock);
//         static void NetworkThreadFunction();
//         static void Initialize(asIScriptContext* ctx);
//         static void Release();
//         static bool IsInitialized();
//         static std::string ScriptPath();
//         static void ScriptPath(const char* path);
//         static void Encoder(int typeID, FuncEncoder encoder, FuncDecoder decoder);
//         static void Break();

//     private:

//         struct TypeEncoder
//         {
//             int m_typeID;
//             FuncEncoder m_encoder;
//             FuncDecoder m_decoder;
//         };

//         static std::string _scriptPath;

//         static std::mutex _logMutex;
//         static std::mutex _clientsMutex;
//         static std::mutex _breakpointMutex;

//         static std::atomic<bool> _initialized;
//         static std::thread _networkThread;
//         static std::vector<std::thread> _clientThreads;
//         static std::vector<SocketUtility*> _clientSockets;

//         static std::vector<TypeEncoder> _typeEncoders;

//         static std::atomic<bool> _dbgStateBroken;
//         static std::atomic<int> _dbgStateBrokenDepth;

//         static std::mutex _dbgStateNotifyStepMutex;
//         static std::unique_lock<std::mutex> _dbgStateNotifyStepLock;  //(_dbgStateNotifyStepMutex);
//         static std::condition_variable _dbgStateNotifyStep;

//         struct Breakpoint
//         {
//             std::string m_filename;
//             int m_line;
//         };
//         static std::vector<Breakpoint> _dbgStateBreakpoints;
 
//     };
// }

// // https://github.com/codecat/asdbg/blob/master/asdbg_host/asdbg.h