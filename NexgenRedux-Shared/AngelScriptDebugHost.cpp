// #include "AngelScriptDebugHost.h"

// using namespace NexgenRedux;

// void AngelScriptDebugHost::Send_Path(SocketUtility* sock)
// {
//     uint16_t packetType = 4;
//     sock->Send(&packetType, sizeof(uint16_t));

//     uint16_t lenPath = (uint16_t)_scriptPath.size();
//     sock->Send(&lenPath, sizeof(uint16_t));
//     sock->Send(_scriptPath.c_str(), lenPath);
// }

// void AngelScriptDebugHost::Send_Location(SocketUtility* sock)
// {
//     const char* sectionName = nullptr;
//     int col = 0;
//     int line = _ctx->GetLineNumber(0, &col, &sectionName);

//     uint16_t packetType = 1;
//     sock->Send(&packetType, sizeof(uint16_t));

//     uint16_t lenFilename = (uint16_t)strlen(sectionName);
//     sock->Send(&lenFilename, sizeof(uint16_t));
//     sock->Send(sectionName, lenFilename);
//     sock->Send(&line, sizeof(int));
//     sock->Send(&col, sizeof(int));
// }

// void AngelScriptDebugHost::Send_LocalClear(SocketUtility* sock)
// {
//     uint16_t packetType = 2;
//     sock->Send(&packetType, sizeof(uint16_t));
// }

// void AngelScriptDebugHost::Send_Local(SocketUtility* sock, const char* name, int typeID, void* ptr)
// {
//     uint16_t packetType = 3;
//     sock->Send(&packetType, sizeof(uint16_t));

//     uint16_t lenName = (uint16_t)strlen(name);
//     sock->Send(&lenName, sizeof(uint16_t));
//     sock->Send(name, lenName);

//     std::string typeName;
//     std::string value;

//     asITypeInfo* typeInfo = _ctx->GetEngine()->GetTypeInfoById(typeID);
//     if (typeInfo != nullptr) {
//         typeName = typeInfo->GetName();
//     }

//     switch (typeID) {
//     case asTYPEID_BOOL:
//         typeName = "bool";
//         if (*(bool*)ptr) {
//             value = "true";
//         } else {
//             value = "false";
//         }
//         break;

//     case asTYPEID_INT8: typeName = "int8"; value = std::to_string(*(int8_t*)ptr); break;
//     case asTYPEID_INT16: typeName = "int16"; value = std::to_string(*(int16_t*)ptr); break;
//     case asTYPEID_INT32: typeName = "int32"; value = std::to_string(*(int32_t*)ptr); break;
//     case asTYPEID_INT64: typeName = "int64"; value = std::to_string(*(int64_t*)ptr); break;

//     case asTYPEID_UINT8: typeName = "uint8"; value = std::to_string(*(uint8_t*)ptr); break;
//     case asTYPEID_UINT16: typeName = "uint16"; value = std::to_string(*(uint16_t*)ptr); break;
//     case asTYPEID_UINT32: typeName = "uint32"; value = std::to_string(*(uint32_t*)ptr); break;
//     case asTYPEID_UINT64: typeName = "uint64"; value = std::to_string(*(uint64_t*)ptr); break;

//     case asTYPEID_FLOAT: typeName = "float"; value = std::to_string(*(float*)ptr); break;
//     case asTYPEID_DOUBLE: typeName = "double"; value = std::to_string(*(double*)ptr); break;

//     default:
//         if ((typeID & asTYPEID_OBJHANDLE) != 0) {
//             typeName += "@";

//             asIScriptObject* obj = *(asIScriptObject**)ptr;
//             if (obj == nullptr) {
//                 value = "(null)";
//             } else {
//                 value = StrFormat("%p", ptr);
//             }
//         } else {
//             if (ptr == nullptr) {
//                 value = "(null)";
//             } else {
//                 bool found = false;
//                 for (auto &typeEncoder : _typeEncoders) {
//                     if (typeEncoder.m_typeID == typeID) {
//                         value = typeEncoder.m_encoder(ptr);
//                         found = true;
//                         break;
//                     }
//                 }
//                 if (!found) {
//                     value = StrFormat("?? (%08X)", typeID);
//                 }
//             }
//         }
//         break;
//     }

//     uint16_t lenTypeName = (uint16_t)typeName.size();
//     sock->Send(&lenTypeName, sizeof(uint16_t));
//     sock->Send(typeName.c_str(), lenTypeName);

//     uint16_t lenValue = (uint16_t)value.size();
//     sock->Send(&lenValue, sizeof(uint16_t));
//     sock->Send(value.c_str(), lenValue);
// }

// void AngelScriptDebugHost::Send_AllLocals(SocketUtility* sock)
// {
//     Send_LocalClear(sock);

//     int numVars = _ctx->GetVarCount();
//     for (int i = 0; i < numVars; i++) {
//         const char* varName = _ctx->GetVarName(i);
//         int varTypeID = _ctx->GetVarTypeId(i);
//         void* varPtr = _ctx->GetAddressOfVar(i);

//         Send_Local(sock, varName, varTypeID, varPtr);
//     }
// }

// void AngelScriptDebugHost::Send_Callstack(SocketUtility* sock)
// {
//     uint16_t packetType = 5;
//     sock->Send(&packetType, sizeof(uint16_t));

//     uint16_t stackSize = _ctx->GetCallstackSize();
//     sock->Send(&stackSize, sizeof(uint16_t));
//     for (int i = 0; i < stackSize; i++) {
//         const char* filename = nullptr;
//         int line = _ctx->GetLineNumber(i, nullptr, &filename);
//         asIScriptFunction* func = _ctx->GetFunction(i);

//         std::string decl;
//         if (func != nullptr) {
//             decl = func->GetDeclaration(true, true, true);
//         }

//         uint16_t lenDecl = (uint16_t)decl.size();
//         sock->Send(&lenDecl, sizeof(uint16_t));
//         sock->Send(decl.c_str(), lenDecl);

//         uint16_t lenFilename = (uint16_t)strlen(filename);
//         sock->Send(&lenFilename, sizeof(uint16_t));
//         sock->Send(filename, lenFilename);

//         sock->Send(&line, sizeof(int));
//     }
// }

// void AngelScriptDebugHost::Send_Breakpoints(SocketUtility* sock)
// {
//     uint16_t packetType = 6;
//     sock->Send(&packetType, sizeof(uint16_t));

//     _breakpointMutex.lock();
//     uint16_t numBreakpoints = (uint16_t)_dbgStateBreakpoints.size();
//     sock->Send(&numBreakpoints, sizeof(uint16_t));
//     for (auto &bp : _dbgStateBreakpoints) {
//         uint16_t lenFilename = (uint16_t)bp.m_filename.size();
//         sock->Send(&lenFilename, sizeof(uint16_t));
//         sock->Send(bp.m_filename.c_str(), lenFilename);

//         sock->Send(&bp.m_line, sizeof(int));
//     }
//     _breakpointMutex.unlock();
// }

// void AngelScriptDebugHost::ScriptLineCallback(SocketUtility* ctx)
// {
//     // Check breakpoints
//     bool breakPointHit = false;
//     _breakpointMutex.lock();
//     for (auto &bp : _dbgStateBreakpoints) {
//         const char* filename = nullptr;
//         int line = ctx->GetLineNumber(0, nullptr, &filename);

//         if (bp.m_line == line && !strcmp(bp.m_filename.c_str() + bp.m_filename.size() - strlen(filename), filename)) {
//             breakPointHit = true;
//             break;
//         }
//     }
//     _breakpointMutex.unlock();

//     // If we hit a breakpoint on this line
//     if (breakPointHit) {
//         // Break now
//         _dbgStateBroken = true;
//     } else {
//         // Otherwise, check if we are stepping over
//         if (_dbgStateBrokenDepth != -1) {
//             // If we're still a frame ahead
//             if ((int)ctx->GetCallstackSize() > _dbgStateBrokenDepth) {
//                 // Continue execution
//                 return;
//             } else {
//                 // We've returned on our original frame, we should break now
//                 _dbgStateBroken = true;
//                 _dbgStateBrokenDepth = -1;
//             }
//         }
//     }

//     // If we're not broken
//     if (!_dbgStateBroken) {
//         // Continue execution
//         return;
//     }

//     _clientsMutex.lock();
//     for (SocketUtility* sock : _clientSockets) {
//         Send_Location(sock);
//         Send_Callstack(sock);
//         Send_AllLocals(sock);
//     }
//     _clientsMutex.unlock();

//     // Wait for step notification
//     _dbgStateNotifyStep.wait(_dbgStateNotifyStepLock);
// }

// void AngelScriptDebugHost::ClientPacket_Step(SocketUtility* sock)
// {
//     _dbgStateNotifyStep.notify_one();
// }

// void AngelScriptDebugHost::ClientPacket_StepOver(SocketUtility* sock)
// {
//     _dbgStateBrokenDepth = _ctx->GetCallstackSize();
//     _dbgStateNotifyStep.notify_one();
// }

// void AngelScriptDebugHost::ClientPacket_StepOut(SocketUtility* sock)
// {
//     if (_ctx->GetCallstackSize() == 1) {
//         _dbgStateBroken = false;
//         _dbgStateNotifyStep.notify_one();
//         return;
//     }
//     _dbgStateBrokenDepth = _ctx->GetCallstackSize() - 1;
//     _dbgStateNotifyStep.notify_one();
// }

// void AngelScriptDebugHost::ClientPacket_Pause(SocketUtility* sock)
// {
//     _dbgStateBroken = true;
// }

// void AngelScriptDebugHost::ClientPacket_Resume(SocketUtility* sock)
// {
//     bool wasBroken = _dbgStateBroken;
//     _dbgStateBroken = false;
//     if (wasBroken) {
//         _dbgStateNotifyStep.notify_one();
//     }
// }

// void AngelScriptDebugHost::ClientPacket_AddBreakpoint(SocketUtility* sock)
// {
//     uint16_t lenFilename = 0;
//     sock->Receive(&lenFilename, sizeof(uint16_t));

//     char* filename = (char*)malloc(lenFilename + 1);
//     sock->Receive(filename, lenFilename);
//     filename[lenFilename] = '\0';

//     int line = 0;
//     sock->Receive(&line, sizeof(int));

//     _breakpointMutex.lock();
//     Breakpoint bp;
//     bp.m_filename = filename;
//     bp.m_line = line;
//     _dbgStateBreakpoints.push_back(bp);
//     Log("Added breakpoint at %s line %d", filename, line);
//     _breakpointMutex.unlock();

//     free(filename);
// }

// void AngelScriptDebugHost::ClientPacket_DeleteBreakpoint(SocketUtility* sock)
// {
//     uint16_t lenFilename = 0;
//     sock->Receive(&lenFilename, sizeof(uint16_t));

//     char* filename = (char*)malloc(lenFilename + 1);
//     sock->Receive(filename, lenFilename);
//     filename[lenFilename] = '\0';

//     int line = 0;
//     sock->Receive(&line, sizeof(int));

//     _breakpointMutex.lock();
//     auto it = std::find_if(_dbgStateBreakpoints.begin(), _dbgStateBreakpoints.end(), [filename, line](const Breakpoint &bp) {
//         return (bp.m_line == line && bp.m_filename == filename);
//     });
//     if (it != _dbgStateBreakpoints.end()) {
//         Log("Erased breakpoint at %s line %d", it->m_filename.c_str(), it->m_line);
//         _dbgStateBreakpoints.erase(it);
//     } else {
//         Log("Couldn't find any breakpoint at %s line %d", filename, line);
//     }
//     _breakpointMutex.unlock();

//     free(filename);
// }

// void AngelScriptDebugHost::ClientPacket_SetValue(SocketUtility* sock)
// {
//     uint16_t lenName = 0;
//     sock->Receive(&lenName, sizeof(uint16_t));

//     char* name = (char*)malloc(lenName + 1);
//     sock->Receive(name, lenName);
//     name[lenName] = '\0';

//     uint16_t lenValue = 0;
//     sock->Receive(&lenValue, sizeof(uint16_t));

//     char* value = (char*)malloc(lenValue + 1);
//     sock->Receive(value, lenValue);
//     value[lenValue] = '\0';

//     int varIndex = -1;
//     int numVars = _ctx->GetVarCount();
//     for (int i = 0; i < numVars; i++) {
//         const char* varName = _ctx->GetVarName(i);
//         if (!strcmp(varName, name)) {
//             varIndex = i;
//             break;
//         }
//     }

//     if (varIndex != -1) {
//         Log("Changing local variable %s to %s", name, value);

//         void* varPtr = _ctx->GetAddressOfVar(varIndex);
//         int typeID = _ctx->GetVarTypeId(varIndex);

//         switch (typeID) {
//         case asTYPEID_BOOL: *(bool*)varPtr = (strcmp(value, "true") == 0); break;

//         case asTYPEID_INT8: *(int8_t*)varPtr = std::stoi(value); break;
//         case asTYPEID_INT16: *(int16_t*)varPtr = std::stoi(value); break;
//         case asTYPEID_INT32: *(int32_t*)varPtr = std::stoi(value); break;
//         case asTYPEID_INT64: *(int64_t*)varPtr = std::stoll(value); break;

//         case asTYPEID_UINT8: *(uint8_t*)varPtr = (uint8_t)std::stoul(value); break;
//         case asTYPEID_UINT16: *(uint16_t*)varPtr = (uint16_t)std::stoul(value); break;
//         case asTYPEID_UINT32: *(uint32_t*)varPtr = std::stoul(value); break;
//         case asTYPEID_UINT64: *(uint64_t*)varPtr = std::stoull(value); break;

//         case asTYPEID_FLOAT: *(float*)varPtr = std::stof(value); break;
//         case asTYPEID_DOUBLE: *(double*)varPtr = std::stod(value); break;

//         default:
//             bool found = false;
//             for (auto &typeEncoder : _typeEncoders) {
//                 if (typeEncoder.m_typeID == typeID) {
//                     typeEncoder.m_decoder(varPtr, value);
//                     found = true;
//                     break;
//                 }
//             }
//             if (!found) {
//                 Log("Couldn't find decoder for type %08x", typeID);
//             }
//             break;
//         }
//     } else {
//         Log("Couldn't find local variable %s", name);
//     }

//     free(name);
//     free(value);
// }

// void AngelScriptDebugHost::ClientThreadFunction(SocketUtility* sock)
// {
//     Log("Connection accepted from: %s", inet_ntoa(sock->addr.sin_addr));

//     Send_Path(sock);
//     Send_Breakpoints(sock);

//     if (_dbgStateBroken) {
//         Send_Location(sock);
//         Send_AllLocals(sock);
//     }

//     while (_initialized) {
//         uint16_t packetType = 0;
//         if (sock->Receive(&packetType, sizeof(uint32_t)) == -1) {
//             break;
//         }

//         if (packetType == 0) {
//             break;
//         }

//         bool invalidPacket = false;
//         switch (packetType) {
//         case 1: ClientPacket_Step(sock); break;
//         case 2: ClientPacket_Pause(sock); break;
//         case 3: ClientPacket_Resume(sock); break;
//         case 4: ClientPacket_AddBreakpoint(sock); break;
//         case 5: ClientPacket_DeleteBreakpoint(sock); break;
//         case 6: ClientPacket_SetValue(sock); break;
//         case 7: ClientPacket_StepOver(sock); break;
//         case 8: ClientPacket_StepOut(sock); break;
//         default: invalidPacket = true; break;
//         }

//         if (invalidPacket) {
//             Log("Invalid packet type received from client: %hu", packetType);
//             break;
//         }
//     }

//     sock->close();
//     //delete sock; //TODO
//     Log("Connection closed");
// }

// void AngelScriptDebugHost::NetworkThreadFunction()
// {
//     const int sockPort = 8912;

//     SocketUtility sock;
//     sock.create();
//     sock.bind(sockPort);
//     if (!sock.listen()) {
//         Log("Failed to listen on port %d", sockPort);
//         return;
//     }
//     Log("Socket listening on port %d", sockPort);

//     while (_initialized) {
//         SocketUtility* client = new SocketUtility;
//         if (!sock.accept(client)) {
//             Log("Failed to accept client!");
//             continue;
//         }

//         _clientsMutex.lock();

//         _clientSockets.push_back(client);
//         _clientThreads.push_back(std::thread(ClientThreadFunction, client));

//         _clientsMutex.unlock();
//     }

//     sock.close();
//     Log("Socket closed");
// }

// void AngelScriptDebugHost::Initialize(asIScriptContext* ctx)
// {
//     _ctx = ctx;
//     _ctx->SetLineCallback(asFUNCTION(dbg::ScriptLineCallback), nullptr, asCALL_CDECL);

//     if (_ctx->GetEngine()->GetEngineProperty(asEP_BUILD_WITHOUT_LINE_CUES) == 1) {
//         Log("WARNING: Engine property asEP_BUILD_WITHOUT_LINE_CUES is set! This will cause strange behavior while debugging.");
//     }

// #ifdef _MSC_VER
//     char path[1024];
//     GetCurrentDirectoryA(1024, path);
//     _scriptPath = path;
// #else
//     //TODO
// #endif

//     _dbgStateBrokenDepth = -1;

//     _initialized = true;
//     _networkThread = std::thread(NetworkThreadFunction);
//     _typeEncoders.clear();

//     Log("Debugger initialized");
// }

// void AngelScriptDebugHost::Release()
// {
//     _initialized = false;

//     _clientsMutex.lock();
//     for (std::thread &clientThread : _clientThreads) {
//         clientThread.join();
//     }
//     _clientsMutex.unlock();
//     _networkThread.join();

//     _ctx->ClearLineCallback();
//     _ctx = nullptr;

//     Log("Debugger released");
// }

// bool AngelScriptDebugHost::IsInitialized()
// {
//     return _initialized;
// }

// std::string AngelScriptDebugHost::ScriptPath()
// {
//     return _scriptPath;
// }

// void AngelScriptDebugHost::ScriptPath(const char* path)
// {
//     _scriptPath = path;
// }

// void AngelScriptDebugHost::Encoder(int typeID, FuncEncoder encoder, FuncDecoder decoder)
// {
//     TypeEncoder typeEncoder;
//     typeEncoder.m_typeID = typeID;
//     typeEncoder.m_encoder = encoder;
//     typeEncoder.m_decoder = decoder;
//     _typeEncoders.push_back(typeEncoder);
// }

// void AngelScriptDebugHost::Break()
// {
//     _dbgStateBroken = true;
// }