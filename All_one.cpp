#include "CoarseList.hpp"
#include "FineList.hpp"
#include "LazyList.hpp"
#include "OptimisticList.hpp"

#define LOCK_FREE_LIST_NO_MAIN
#include "LockFreeList.cpp"
#undef LOCK_FREE_LIST_NO_MAIN

#ifdef _WIN32
#define WATCHER_SERVER_NO_MAIN
#include "WatcherServer.cpp"
#undef WATCHER_SERVER_NO_MAIN

#define SOCKET_CLIENT_NO_MAIN
#include "SocketClient.cpp"
#undef SOCKET_CLIENT_NO_MAIN
#endif

#include <atomic>
#include <condition_variable>
#include <cstdlib>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <Windows.h>
#endif

namespace {

std::mutex outputMutex;
std::atomic<unsigned int> completedOperations(0);

template <typename List>
class SynchronizedList {
public:
    SynchronizedList() : list(new List()) {}

    bool add(int value) {
        std::lock_guard<std::mutex> guard(listMutex);
        return list->add(value);
    }

    bool remove(int value) {
        std::lock_guard<std::mutex> guard(listMutex);
        return list->remove(value);
    }

    bool contains(int value) {
        std::lock_guard<std::mutex> guard(listMutex);
        return list->contains(value);
    }

private:
    std::unique_ptr<List> list;
    std::mutex listMutex;
};

template <typename List>
void runList(const std::string& name) {
    const int threadCount = 4;
    const int valuesPerThread = 20;
    SynchronizedList<List> list;
    std::mutex startMutex;
    std::condition_variable startCondition;
    bool start = false;
    std::vector<std::thread> workers;

    for (int threadIndex = 0; threadIndex < threadCount; ++threadIndex) {
        workers.push_back(std::thread([&, threadIndex]() {
            {
                std::unique_lock<std::mutex> lock(startMutex);
                startCondition.wait(lock, [&start]() { return start; });
            }

            const int firstValue = threadIndex * valuesPerThread + 1;
            for (int offset = 0; offset < valuesPerThread; ++offset) {
                list.add(firstValue + offset);
                completedOperations.fetch_add(1, std::memory_order_relaxed);
            }
        }));
    }

    {
        std::lock_guard<std::mutex> guard(startMutex);
        start = true;
    }
    startCondition.notify_all();

    for (std::vector<std::thread>::iterator worker = workers.begin();
         worker != workers.end(); ++worker) {
        worker->join();
    }

    int found = 0;
    for (int value = 1; value <= threadCount * valuesPerThread; ++value) {
        if (list.contains(value)) {
            ++found;
        }
    }

    {
        std::lock_guard<std::mutex> guard(outputMutex);
        std::cout << name << ": " << found << " synchronized values found\n";
    }
}

#ifdef _WIN32
struct WindowsThreadContext {
    CRITICAL_SECTION criticalSection;
    long counter;
};

DWORD WINAPI windowsWorker(LPVOID parameter) {
    WindowsThreadContext* context = static_cast<WindowsThreadContext*>(parameter);
    for (int iteration = 0; iteration < 10000; ++iteration) {
        EnterCriticalSection(&context->criticalSection);
        ++context->counter;
        LeaveCriticalSection(&context->criticalSection);
    }
    return 0;
}

void runWindowsThreads() {
    WindowsThreadContext context;
    context.counter = 0;
    InitializeCriticalSection(&context.criticalSection);

    HANDLE handles[2] = {
        CreateThread(NULL, 0, windowsWorker, &context, 0, NULL),
        CreateThread(NULL, 0, windowsWorker, &context, 0, NULL)
    };

    if (handles[0] == NULL || handles[1] == NULL) {
        if (handles[0] != NULL) {
            CloseHandle(handles[0]);
        }
        if (handles[1] != NULL) {
            CloseHandle(handles[1]);
        }
        DeleteCriticalSection(&context.criticalSection);
        throw std::runtime_error("CreateThread failed");
    }

    WaitForMultipleObjects(2, handles, TRUE, INFINITE);
    CloseHandle(handles[0]);
    CloseHandle(handles[1]);
    DeleteCriticalSection(&context.criticalSection);

    std::lock_guard<std::mutex> guard(outputMutex);
    std::cout << "Windows CreateThread counter: " << context.counter << "\n";
}

std::wstring widen(const char* value) {
    if (value == NULL || *value == '\0') {
        return std::wstring();
    }

    const int size = MultiByteToWideChar(CP_UTF8, 0, value, -1, NULL, 0);
    if (size == 0) {
        throw std::runtime_error("Invalid UTF-8 directory path");
    }

    std::vector<wchar_t> buffer(static_cast<std::size_t>(size));
    MultiByteToWideChar(CP_UTF8, 0, value, -1, &buffer[0], size);
    return std::wstring(&buffer[0]);
}
#endif

} // namespace

int main(int argc, char* argv[]) {
#ifdef _WIN32
    if (argc > 1 && std::string(argv[1]) == "watcher") {
        const std::wstring directory = argc > 2 ? widen(argv[2]) : L".";
        const int port = argc > 3 ? std::atoi(argv[3]) : 5050;
        return runWatcherServer(directory, port);
    }

    if (argc > 1 && std::string(argv[1]) == "client") {
        const char* host = argc > 2 ? argv[2] : "127.0.0.1";
        const char* port = argc > 3 ? argv[3] : "5050";
        return runSocketClient(host, port);
    }
#endif

    std::vector<std::future<void> > tasks;
    tasks.push_back(std::async(std::launch::async, runList<CoarseList<int> >, "CoarseList"));
    tasks.push_back(std::async(std::launch::async, runList<FineList<int> >, "FineList"));
    tasks.push_back(std::async(std::launch::async, runList<LazyList<int> >, "LazyList"));
    tasks.push_back(std::async(std::launch::async, runList<OptimisticList<int> >, "OptimisticList"));
    tasks.push_back(std::async(std::launch::async, runList<LockFreeList<int> >, "LockFreeList"));

    for (std::vector<std::future<void> >::iterator task = tasks.begin();
         task != tasks.end(); ++task) {
        task->get();
    }

#ifdef _WIN32
    runWindowsThreads();
#else
    std::cout << "Windows API example skipped on this platform.\n";
#endif

    std::cout << "Completed list operations: "
              << completedOperations.load(std::memory_order_relaxed) << "\n";
    return 0;
}