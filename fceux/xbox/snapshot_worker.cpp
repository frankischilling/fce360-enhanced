#include <xtl.h>
#include "snapshot_worker.h"
#include "snapshot_png.h"
#include <map>
#include <string>
#include <cstdio>
#include <cstring>
#include <new>

namespace {
enum { IDLE, COPYING, READY, WRITING };
volatile LONG state = IDLE;
volatile LONG stopping = 0;
HANDLE wake = NULL, worker = NULL;
CRITICAL_SECTION filesLock;
bool lockInitialized = false;
SnapshotImage queuedImage;
char queuedName[MAX_PATH];
std::map<std::string, unsigned int> nextIndex;

bool CopyImage(SnapshotImage& image, char* name, const unsigned char* pixels,
    unsigned int height, const unsigned char* palette, const char* zeroName)
{
    if (!pixels || !palette || !zeroName || height == 0 || height > 256) return false;
    size_t length = strlen(zeroName);
    if (length < 5 || length + 4 >= MAX_PATH || strcmp(zeroName + length - 5, "0.png")) return false;
    memcpy(image.pixels, pixels, height * 256);
    memcpy(image.palette, palette, 768);
    image.height = height;
    memcpy(name, zeroName, length + 1);
    return true;
}

int SaveImage(const SnapshotImage& image, const char* zeroName)
{
    // Lua's synchronous numbered captures share this lock and allocator.
    EnterCriticalSection(&filesLock);
    std::string prefix(zeroName, strlen(zeroName) - 5);
    unsigned int index = nextIndex[prefix];
    HANDLE file = INVALID_HANDLE_VALUE;
    char name[MAX_PATH];
    for (; index < 99999; ++index) {
        sprintf(name, "%s%u.png", prefix.c_str(), index);
        file = CreateFileA(name, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
        if (file != INVALID_HANDLE_VALUE) break;
        DWORD error = GetLastError();
        if (error != ERROR_FILE_EXISTS && error != ERROR_ALREADY_EXISTS) {
            LeaveCriticalSection(&filesLock);
            return 0;
        }
    }
    if (file == INVALID_HANDLE_VALUE) {
        LeaveCriticalSection(&filesLock);
        return 0;
    }
    std::vector<unsigned char> png;
    DWORD written = 0;
    bool ok = EncodeSnapshotPNG(image, png) &&
        WriteFile(file, &png[0], static_cast<DWORD>(png.size()), &written, NULL) && written == png.size();
    if (!CloseHandle(file)) ok = false;
    if (ok) nextIndex[prefix] = index + 1;
    else DeleteFileA(name); // Only the file reserved by this call can be removed.
    LeaveCriticalSection(&filesLock);
    return ok ? static_cast<int>(index + 1) : 0;
}

DWORD WINAPI WriteSnapshots(void*)
{
    for (;;) {
        WaitForSingleObject(wake, INFINITE);
        if (InterlockedCompareExchange(&state, WRITING, READY) == READY) {
            if (!SaveImage(queuedImage, queuedName))
                OutputDebugStringA("Screenshot save failed. Check the snaps directory.\n");
            InterlockedExchange(&state, IDLE);
        }
        if (InterlockedCompareExchange(&stopping, 0, 0)) break;
    }
    return 0;
}
}

bool XboxSnapshotInitialize()
{
    if (worker) return true;
    if (!lockInitialized) {
        InitializeCriticalSection(&filesLock);
        lockInitialized = true;
    }
    InterlockedExchange(&stopping, 0);
    InterlockedExchange(&state, IDLE);
    wake = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!wake) return false;
    worker = CreateThread(NULL, 512 * 1024, WriteSnapshots, NULL, CREATE_SUSPENDED, NULL);
    if (!worker) { CloseHandle(wake); wake = NULL; return false; }
    SetThreadPriority(worker, THREAD_PRIORITY_BELOW_NORMAL);
    ResumeThread(worker);
    return true;
}

void XboxSnapshotShutdown()
{
    if (worker) {
        InterlockedExchange(&stopping, 1);
        SetEvent(wake);
        WaitForSingleObject(worker, INFINITE); // Drain the owned frame before releasing storage.
        CloseHandle(worker);
        CloseHandle(wake);
        worker = wake = NULL;
    }
    if (lockInitialized) {
        DeleteCriticalSection(&filesLock);
        lockInitialized = false;
    }
    nextIndex.clear();
}

bool XboxSnapshotQueue(const unsigned char* pixels, unsigned int height,
    const unsigned char* palette, const char* zeroName)
{
    // Never start threads, wait for I/O, or allocate compression buffers on this path.
    if (!worker || InterlockedCompareExchange(&state, COPYING, IDLE) != IDLE) return false;
    if (!CopyImage(queuedImage, queuedName, pixels, height, palette, zeroName)) {
        InterlockedExchange(&state, IDLE);
        return false;
    }
    InterlockedExchange(&state, READY);
    SetEvent(wake);
    return true;
}

int XboxSnapshotSave(const unsigned char* pixels, unsigned int height,
    const unsigned char* palette, const char* zeroName)
{
    if (!lockInitialized) return 0;
    SnapshotImage* image = new (std::nothrow) SnapshotImage;
    if (!image) return 0;
    char name[MAX_PATH];
    int result = CopyImage(*image, name, pixels, height, palette, zeroName) ? SaveImage(*image, name) : 0;
    delete image;
    return result;
}
