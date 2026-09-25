#include <winsock2.h>
#include <ws2tcpip.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#pragma comment(lib, "Ws2_32.lib")

namespace {

std::mutex clientsMutex;
std::vector<SOCKET> clients;

const wchar_t* actionName(DWORD action) {
    switch (action) {
    case FILE_ACTION_ADDED: return L"ADDED";
    case FILE_ACTION_REMOVED: return L"REMOVED";
    case FILE_ACTION_MODIFIED: return L"MODIFIED";
    case FILE_ACTION_RENAMED_OLD_NAME: return L"RENAMED_FROM";
    case FILE_ACTION_RENAMED_NEW_NAME: return L"RENAMED_TO";
    default: return L"UNKNOWN";
    }
}

std::string narrow(const std::wstring& value) {
    if (value.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    std::string result(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), &result[0], size, nullptr, nullptr);
    return result;
}

std::wstring entryType(const std::wstring& directory, const std::wstring& name) {
    std::wstring path = directory + L"\\" + name;
    DWORD attributes = GetFileAttributesW(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) return L"UNKNOWN_OR_DELETED";
    return (attributes & FILE_ATTRIBUTE_DIRECTORY) ? L"DIRECTORY" : L"FILE";
}

void broadcast(const std::string& line) {
    std::lock_guard<std::mutex> lock(clientsMutex);
    for (auto it = clients.begin(); it != clients.end();) {
        if (send(*it, line.data(), static_cast<int>(line.size()), 0) == SOCKET_ERROR) {
            closesocket(*it);
            it = clients.erase(it);
        } else {
            ++it;
        }
    }
}

void acceptClients(SOCKET server) {
    while (true) {
        SOCKET client = accept(server, nullptr, nullptr);
        if (client == INVALID_SOCKET) return;
        std::lock_guard<std::mutex> lock(clientsMutex);
        clients.push_back(client);
        const std::string welcome = "CONNECTED | watcher server\n";
        send(client, welcome.data(), static_cast<int>(welcome.size()), 0);
    }
}

void watchDirectory(const std::wstring& directory) {
    HANDLE handle = CreateFileW(directory.c_str(), FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
        OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        std::wcerr << L"CreateFileW failed for " << directory << L" (" << GetLastError() << L")\n";
        return;
    }

    std::wcout << L"Watching " << directory << L"\n";
    BYTE buffer[64 * 1024];
    DWORD bytesReturned = 0;
    while (ReadDirectoryChangesW(handle, buffer, sizeof(buffer), TRUE,
        FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE,
        &bytesReturned, nullptr, nullptr)) {
        DWORD offset = 0;
        do {
            auto* notification = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(buffer + offset);
            std::wstring name(notification->FileName, notification->FileNameLength / sizeof(wchar_t));
            std::wstring type = entryType(directory, name);
            std::string line = narrow(actionName(notification->Action)) + " | " + narrow(type) + " | " + narrow(name) + "\n";
            std::cout << line;
            broadcast(line);
            if (notification->NextEntryOffset == 0) break;
            offset += notification->NextEntryOffset;
        } while (offset < bytesReturned);
    }

    CloseHandle(handle);
}

} // namespace

int runWatcherServer(const std::wstring& directory, int port) {
    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) return 1;

    SOCKET server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server == INVALID_SOCKET) return 1;

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(static_cast<u_short>(port));
    if (::bind(server, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR || listen(server, 8) == SOCKET_ERROR) {
        closesocket(server);
        WSACleanup();
        return 1;
    }

    std::cout << "Watcher server listening on port " << port << "\n";
    std::thread clientThread(acceptClients, server);
    clientThread.detach();
    watchDirectory(directory);

    closesocket(server);
    WSACleanup();
    return 0;
}

#ifndef WATCHER_SERVER_NO_MAIN
int wmain(int argc, wchar_t* argv[]) {
    const std::wstring directory = argc > 1 ? argv[1] : L".";
    const int port = argc > 2 ? _wtoi(argv[2]) : 5050;
    return runWatcherServer(directory, port);
}
#endif
