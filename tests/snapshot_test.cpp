#include <xtl.h>
#include "snapshot_worker.h"
#include "snapshot_png.h"
#include <cassert>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <fstream>
#include <map>
#include <thread>
#include <vector>
#include <iostream>

struct Handle { enum Kind { EVENT, THREAD, FILE } kind; explicit Handle(Kind k):kind(k) {} virtual ~Handle() {} };
struct Event : Handle { std::mutex mutex; std::condition_variable cv; bool ready; Event():Handle(EVENT),ready(false) {} };
struct Thread : Handle { std::thread thread; Event start; Thread():Handle(THREAD) {} };
struct File : Handle { std::string name; File(const char* n):Handle(FILE),name(n) {} };
static std::map<std::string, std::vector<unsigned char> > files;
static std::mutex storage;
static thread_local DWORD lastError;
static std::atomic<bool> failThread(false), failWrite(false), failOpen(false);
static std::atomic<int> fileOps(0), messages(0);
static Event writeEntered, allowWrite;
static std::atomic<bool> blockWrite(false);
void InitializeCriticalSection(CRITICAL_SECTION*) {}
void DeleteCriticalSection(CRITICAL_SECTION*) {}
void EnterCriticalSection(CRITICAL_SECTION* cs) { cs->lock(); }
void LeaveCriticalSection(CRITICAL_SECTION* cs) { cs->unlock(); }
LONG InterlockedCompareExchange(volatile LONG* p, LONG value, LONG expected) { return __sync_val_compare_and_swap(p,expected,value); }
LONG InterlockedExchange(volatile LONG* p, LONG value) { return __sync_lock_test_and_set(p,value); }
HANDLE CreateEvent(void*, BOOL, BOOL, const char*) { return new Event; }
BOOL SetEvent(HANDLE h) { Event* e=static_cast<Event*>(h); { std::lock_guard<std::mutex> lock(e->mutex); e->ready=true; } e->cv.notify_one(); return TRUE; }
HANDLE CreateThread(void*, DWORD, DWORD (*fn)(void*), void* arg, DWORD, DWORD*) {
    if(failThread) return NULL;
    Thread* t=new Thread;
    t->thread=std::thread([=]{ WaitForSingleObject(&t->start, INFINITE); fn(arg); });
    return t;
}
BOOL SetThreadPriority(HANDLE, int) { return TRUE; }
DWORD ResumeThread(HANDLE h) { return SetEvent(&static_cast<Thread*>(h)->start); }
DWORD WaitForSingleObject(HANDLE h, DWORD timeout) {
    Handle* base=static_cast<Handle*>(h);
    if(base->kind==Handle::THREAD) { static_cast<Thread*>(h)->thread.join(); return 0; }
    Event* e=static_cast<Event*>(h); std::unique_lock<std::mutex> lock(e->mutex);
    if(timeout==INFINITE) e->cv.wait(lock,[=]{return e->ready;});
    else if(!e->cv.wait_for(lock,std::chrono::milliseconds(timeout),[=]{return e->ready;})) return 258;
    e->ready=false; return 0;
}
BOOL CloseHandle(HANDLE h) { delete static_cast<Handle*>(h); return TRUE; }
HANDLE CreateFileA(const char* name,DWORD,DWORD,void*,DWORD creation,DWORD,HANDLE) {
    assert(creation==CREATE_NEW); ++fileOps;
    std::lock_guard<std::mutex> lock(storage);
    if(failOpen) { lastError=5; return INVALID_HANDLE_VALUE; }
    if(files.count(name)) { lastError=ERROR_FILE_EXISTS; return INVALID_HANDLE_VALUE; }
    files[name]=std::vector<unsigned char>(); return new File(name);
}
DWORD GetLastError() { return lastError; }
BOOL WriteFile(HANDLE h,const void* bytes,DWORD size,DWORD* written,void*) {
    if(blockWrite) { SetEvent(&writeEntered); WaitForSingleObject(&allowWrite,INFINITE); }
    std::lock_guard<std::mutex> lock(storage);
    const unsigned char* data=static_cast<const unsigned char*>(bytes);
    *written=failWrite?size/2:size;
    files[static_cast<File*>(h)->name].assign(data,data+*written); return TRUE;
}
BOOL DeleteFileA(const char* name) { std::lock_guard<std::mutex> lock(storage); return files.erase(name)!=0; }
void OutputDebugStringA(const char*) { ++messages; }
static void Fixture(const char* path, const std::vector<unsigned char>& bytes) {
    std::ofstream out(path,std::ios::binary); out.write(reinterpret_cast<const char*>(&bytes[0]),bytes.size());
}
int main() {
    unsigned char pixels[256*256], palette[768];
    std::memset(pixels,17,sizeof(pixels));
    for(int i=0;i<768;++i) palette[i]=static_cast<unsigned char>(i);
    assert(!XboxSnapshotQueue(pixels,240,palette,"A - 0.png"));
    failThread=true; assert(!XboxSnapshotInitialize()); failThread=false;
    assert(XboxSnapshotInitialize());
    assert(!XboxSnapshotQueue(NULL,240,palette,"A - 0.png"));
    assert(!XboxSnapshotQueue(pixels,257,palette,"A - 0.png"));
    assert(!XboxSnapshotQueue(pixels,240,palette,"wrong-name"));
    assert(!XboxSnapshotQueue(pixels,240,palette,(std::string(260,'x')+"0.png").c_str()));
    files["A - 0.png"]=std::vector<unsigned char>(4,42);
    files["A - 99999.png"]=std::vector<unsigned char>(4,99);
    int before=fileOps;
    blockWrite=true;
    auto start=std::chrono::steady_clock::now();
    assert(XboxSnapshotQueue(pixels,240,palette,"A - 0.png"));
    auto duration=std::chrono::steady_clock::now()-start;
    assert(duration<std::chrono::milliseconds(100));
    assert(WaitForSingleObject(&writeEntered,2000)==0);
    assert(fileOps>before); // I/O has started on the worker while the producer continues.
    assert(!XboxSnapshotQueue(pixels,240,palette,"B - 0.png"));
    std::memset(pixels,33,sizeof(pixels)); std::memset(palette,9,sizeof(palette));
    std::atomic<bool> syncReturned(false);
    std::thread sync([&]{ assert(XboxSnapshotSave(pixels,256,palette,"A - 0.png")==3); syncReturned=true; });
    blockWrite=false; SetEvent(&allowWrite);
    sync.join(); assert(syncReturned);
    XboxSnapshotShutdown();
    assert(files["A - 0.png"]==std::vector<unsigned char>(4,42));
    assert(files["A - 99999.png"]==std::vector<unsigned char>(4,99));
    Fixture("snapshot-async.png",files["A - 1.png"]);
    Fixture("snapshot-sync.png",files["A - 2.png"]);
    assert(XboxSnapshotInitialize());
    failWrite=true;
    assert(XboxSnapshotSave(pixels,240,palette,"B - 0.png")==0);
    assert(!files.count("B - 0.png"));
    failWrite=false; failOpen=true;
    assert(XboxSnapshotSave(pixels,240,palette,"B - 0.png")==0);
    failOpen=false;
    assert(XboxSnapshotQueue(pixels,240,palette,"B - 0.png"));
    XboxSnapshotShutdown(); // A queued capture must finish before shutdown returns.
    assert(files["B - 0.png"].size()>8);
    assert(!XboxSnapshotQueue(pixels,240,palette,"B - 0.png"));
    std::cout<<"Snapshot worker tests passed: owned frames, bounded producer, busy queue, numbering, synchronous sharing, write failures and shutdown.\n";
}
