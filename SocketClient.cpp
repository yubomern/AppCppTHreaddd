#include <winsock2.h>
#include <ws2tcpip.h>

#include <iostream>
#include <string>

#include "TerminalColors.hpp"

#pragma comment(lib, "Ws2_32.lib")

int runSocketClient(const char* host, const char* port) {
    terminal::enableColors();
    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) return 1;

    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* result = nullptr;
    if (getaddrinfo(host, port, &hints, &result) != 0) {
        WSACleanup();
        return 1;
    }

    SOCKET socketHandle = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (socketHandle == INVALID_SOCKET || connect(socketHandle, result->ai_addr, static_cast<int>(result->ai_addrlen)) == SOCKET_ERROR) {
        if (socketHandle != INVALID_SOCKET) closesocket(socketHandle);
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }
    freeaddrinfo(result);

    char buffer[4096];
    int received;
    while ((received = recv(socketHandle, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[received] = '\0';
        std::cout << terminal::green << buffer << terminal::reset << std::flush;
    }

    closesocket(socketHandle);
    WSACleanup();
    return 0;
}

#ifndef SOCKET_CLIENT_NO_MAIN
int main(int argc, char* argv[]) {
    const char* host = argc > 1 ? argv[1] : "127.0.0.1";
    const char* port = argc > 2 ? argv[2] : "5050";
    return runSocketClient(host, port);
}
#endif
