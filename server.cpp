#include <iostream>
#include <cstring>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstdint>
#include <thread>
#include <mutex>
#include <vector>
#include <memory>
#include <cstdio>
#include <gdiplus.h>
#include <atomic> 
#include <algorithm>
#include "screencapture.h"

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "gdiplus.lib")

#define PORT 49153
#define BUFFER_SIZE 4096  
#define MAX_CLIENTS 3

std::mutex mtx;
std::atomic<int> active_threads{0};

void client_thread(SOCKET client_socket) {
    char buffer[BUFFER_SIZE] = {0};

    int bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
    if (bytes_received == SOCKET_ERROR || strcmp(buffer, "start") != 0) {
        return;
    }

    int frames = 0;
    auto lastTime = std::chrono::steady_clock::now();
    while (true) { 
        frames++;
        auto currentTime = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(currentTime - lastTime);
        
        if (elapsed.count() >= 1) {
            std::cout << "Server FPS: " << frames << std::endl;
            frames = 0;
            lastTime = currentTime;
        }

        IStream* imageStream = nullptr;
        {
            std::lock_guard<std::mutex> lock(mtx);
            imageStream = captureScreenToStream();
            if (!imageStream) {
                std::cerr << "Failed to capture screen" << std::endl;
                break;
            }
        }

        // Get stream size
        STATSTG streamStats;
        imageStream->Stat(&streamStats, STATFLAG_NONAME);
        uint64_t image_size = streamStats.cbSize.QuadPart; 

        // Send image size
        uint64_t temp_size = htonl(image_size);
        int sent_bytes = send(client_socket, reinterpret_cast<char*>(&temp_size), sizeof(temp_size), 0);
        if (sent_bytes <= 0) {
            std::cout << "Client disconnected" << std::endl;
            break;  
        }
        std::cout << "Sent image size: " << image_size << std::endl;

        // Stream transmission
        char buffer[BUFFER_SIZE];
        while (image_size > 0) {
            size_t bytes_to_read = std::min(image_size, static_cast<uint64_t>(BUFFER_SIZE));
            ULONG bytesRead;
            imageStream->Read(buffer, bytes_to_read, &bytesRead);
            
            int sent_bytes = send(client_socket, buffer, bytesRead, 0);
            if (sent_bytes <= 0) {
                {
                    std::lock_guard<std::mutex> lock(mtx);
                    active_threads--;
                    std::cout << "Client disconnected during transmission" << std::endl;
                }
                closesocket(client_socket);
                return; 
            }
            image_size -= bytesRead;
        }

        imageStream->Release();
        //std::this_thread::sleep_for(std::chrono::milliseconds(33));
    }
    closesocket(client_socket);
    {
        std::lock_guard<std::mutex> lock(mtx);
        active_threads--;
    }
}

int main() {
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
    WSADATA wsa;
    SOCKET server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    int addr_len = sizeof(client_addr);
    bool first_connection = true;
    std::vector<std::thread> threads;

    // Attempts to initialize WSA
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cerr << "WSAStartup failed. Error Code: " << WSAGetLastError() << std::endl;
        return 1;
    }

    // Attempts to create the server socket
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        std::cerr << "Could not create socket: " << WSAGetLastError() << std::endl;
        return 1;
    }

    // Assigns the addresses
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // Attempts to bind the socket with the server address
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        std::cerr << "Bind failed with error code: " << WSAGetLastError() << std::endl;
        return 1;
    }

    // Listens for incoming connections (max 3)
    listen(server_socket, MAX_CLIENTS);
    std::cout << "Server listening on port " << PORT << std::endl;

    while (true) {
        if ((!first_connection && active_threads <= 0) || active_threads >= MAX_CLIENTS) {
            std::cout << "Server shutting down: " 
                  << (active_threads <= 0 ? "No active clients" : "Max clients reached") 
                  << std::endl;
            break;
        }

        // Attempts to accept any incoming connections
        if ((client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addr_len)) == INVALID_SOCKET) {
            std::cerr << "Accept failed with error code: " << WSAGetLastError() << std::endl;
            return 0;
        }

        first_connection = false;
        std::cout << "Connection accepted on port: " << PORT << std::endl;

        // Creates a thread to handle the client
        threads.emplace_back(client_thread, client_socket);
        
        // Increments the active thread counter
        {
            std::lock_guard<std::mutex> lock(mtx);
            active_threads++;
        }
    }

    for (auto &th : threads) {
        th.join();
    }

    closesocket(server_socket);  // Closes the server socket
    WSACleanup();                // Cleans up the WSA environment

    return 0;
}
