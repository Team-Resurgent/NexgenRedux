#pragma once

// What AngelScript asks of <windows.h> for its threads, on the Xbox 360, where the real one needs the whole console SDK and
// would change how AngelScript is configured. Each call is carried out in base/thread_as360.cpp, which does have it.

typedef unsigned long DWORD;
typedef long LONG;
typedef int BOOL;
typedef void* HANDLE;
typedef void* LPVOID;

#define WINAPI
#define INFINITE 0xFFFFFFFFul
#define FALSE 0
#define TRUE 1

// Larger than the console's own, which is all that matters: it is only ever passed by its address.
typedef struct
{
    __attribute__((aligned(8))) unsigned char storage[64];
} CRITICAL_SECTION;

#ifdef __cplusplus
extern "C" {
#endif

void nx360InitializeCriticalSection(CRITICAL_SECTION* section);
void nx360DeleteCriticalSection(CRITICAL_SECTION* section);
void nx360EnterCriticalSection(CRITICAL_SECTION* section);
void nx360LeaveCriticalSection(CRITICAL_SECTION* section);
BOOL nx360TryEnterCriticalSection(CRITICAL_SECTION* section);
DWORD nx360TlsAlloc(void);
BOOL nx360TlsFree(DWORD key);
LPVOID nx360TlsGetValue(DWORD key);
BOOL nx360TlsSetValue(DWORD key, LPVOID value);
HANDLE nx360CreateSemaphore(void* attributes, LONG initial, LONG most, const void* name);
BOOL nx360ReleaseSemaphore(HANDLE semaphore, LONG count, LONG* previous);
DWORD nx360WaitForSingleObject(HANDLE handle, DWORD milliseconds);
BOOL nx360CloseHandle(HANDLE handle);

#ifdef __cplusplus
}
#endif

#define InitializeCriticalSection nx360InitializeCriticalSection
#define DeleteCriticalSection nx360DeleteCriticalSection
#define EnterCriticalSection nx360EnterCriticalSection
#define LeaveCriticalSection nx360LeaveCriticalSection
#define TryEnterCriticalSection nx360TryEnterCriticalSection
#define TlsAlloc nx360TlsAlloc
#define TlsFree nx360TlsFree
#define TlsGetValue nx360TlsGetValue
#define TlsSetValue nx360TlsSetValue
#define CreateSemaphoreW nx360CreateSemaphore
#define ReleaseSemaphore nx360ReleaseSemaphore
#define WaitForSingleObject nx360WaitForSingleObject
#define WaitForSingleObjectEx(handle, milliseconds, alertable) nx360WaitForSingleObject(handle, milliseconds)
#define CloseHandle nx360CloseHandle
