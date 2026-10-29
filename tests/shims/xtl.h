#ifndef SNAPSHOT_TEST_XTL_H
#define SNAPSHOT_TEST_XTL_H
#include <cstdint>
#include <mutex>
typedef std::uint32_t DWORD;
typedef std::int32_t LONG;
typedef int BOOL;
typedef void* HANDLE;
#define WINAPI
#define TRUE 1
#define FALSE 0
#define INFINITE 0xffffffffu
#define MAX_PATH 260
#define INVALID_HANDLE_VALUE reinterpret_cast<HANDLE>(-1)
#define GENERIC_WRITE 1
#define CREATE_NEW 1
#define FILE_ATTRIBUTE_NORMAL 0
#define ERROR_FILE_EXISTS 80
#define ERROR_ALREADY_EXISTS 183
#define CREATE_SUSPENDED 4
#define THREAD_PRIORITY_BELOW_NORMAL (-1)
typedef std::recursive_mutex CRITICAL_SECTION;
void InitializeCriticalSection(CRITICAL_SECTION*);
void DeleteCriticalSection(CRITICAL_SECTION*);
void EnterCriticalSection(CRITICAL_SECTION*);
void LeaveCriticalSection(CRITICAL_SECTION*);
LONG InterlockedCompareExchange(volatile LONG*, LONG, LONG);
LONG InterlockedExchange(volatile LONG*, LONG);
HANDLE CreateEvent(void*, BOOL, BOOL, const char*);
BOOL SetEvent(HANDLE);
HANDLE CreateThread(void*, DWORD, DWORD (*)(void*), void*, DWORD, DWORD*);
BOOL SetThreadPriority(HANDLE, int);
DWORD ResumeThread(HANDLE);
DWORD WaitForSingleObject(HANDLE, DWORD);
BOOL CloseHandle(HANDLE);
HANDLE CreateFileA(const char*, DWORD, DWORD, void*, DWORD, DWORD, HANDLE);
DWORD GetLastError();
BOOL WriteFile(HANDLE, const void*, DWORD, DWORD*, void*);
BOOL DeleteFileA(const char*);
void OutputDebugStringA(const char*);
#endif
