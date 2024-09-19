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
#include "screencapture.h"

#pragma comment(lib, "Ws2_32.lib")

#define PORT 49153                 // Defines the port number    
#define BUFFER_SIZE 1024           // Defines the max buffer size
#define FILEPATH "server_images/"  // Path to image directory

std::mutex mtx;
int active_threads = 0;

void client_thread(SOCKET client_socket) {
    char buffer[BUFFER_SIZE] = {0};
    std::string ack;

    // Receives information from the client
    int bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);

    // Checks if the information is valid
    if (bytes_received == SOCKET_ERROR) {
        std::cerr << "Receive failed with error code: " << WSAGetLastError() << std::endl;
        return;
    } else if (strcmp(buffer, "exit") == 0) {
        std::cout << "Client has chosen to quit" << std::endl;
        return;
    } else if (strcmp(buffer, "start") == 0) {
        std::cout << "Client has chosen to start" << std::endl;
    }   

    ack = "Server ACK. Beginning Stream";

    if(send(client_socket, ack.c_str(), ack.length(), 0)) {
        std::cout << "Sending acknowledgement for stream start" << std::endl;
    }

    while (true) { 
        // Clears the buffer
        memset(buffer, 0, BUFFER_SIZE);

        if(recv(client_socket, buffer, BUFFER_SIZE, 0)) {
            std::cout << buffer << std::endl;
        }

        // Take screenshot to generate screen.jpeg
        {
            std::lock_guard<std::mutex> lock(mtx);
            getScreen();
        } 

        // Opens the image file for reading
        FILE *image_file = fopen("server_images/screen.jpeg", "rb");
        if (!image_file) {
            std::cerr << "Error opening image file: screen.jpeg" << std::endl;
            break;
        }

        // Clears the buffer
        memset(buffer, 0, BUFFER_SIZE);

        // Handles sending image size to the client
        if (fseek(image_file, 0, SEEK_END) != 0) {
            std::cerr << "Seek failed" << std::endl;
            break;
        }

        long image_size = ftell(image_file);
        if (image_size < 0) {
            std::cerr << "Tell failed" << std::endl;
            break;
        }

        rewind(image_file);

        uint64_t temp_size = htonl(image_size);
        memcpy(buffer, &temp_size, sizeof(temp_size));
        send(client_socket, buffer, BUFFER_SIZE, 0);

        {
            std::lock_guard<std::mutex> lock(mtx);
            std::cout << "Sent image size." << std::endl;
        }

        while (image_size > 0) {
            // Clears the buffer
            memset(buffer, 0, sizeof(buffer));

            // Reads the image file into the buffer
            size_t bytes_read = fread(buffer, 1, BUFFER_SIZE, image_file);
            if (bytes_read == SOCKET_ERROR) {
                std::cerr << "Read failed. Error code: " << WSAGetLastError() << std::endl;
                break;
            }

            // Sends the image stored in buffer back to the client
            if (send(client_socket, buffer, bytes_read, 0) == SOCKET_ERROR) {
                std::cerr << "Send failed with error code: " << WSAGetLastError() << std::endl;
                break;
            }

            image_size -= bytes_read;
        }

        std::cout << "Image sent to client" << std::endl;
        fclose(image_file);

        // Receives acknowledgement from the client
        memset(buffer, 0, BUFFER_SIZE);
        if ((recv(client_socket, buffer, BUFFER_SIZE, 0)) < 0) {
            std::cerr << "Receive failed with error code: " << WSAGetLastError() << std::endl;
            break;
        } else {
            std::cout << "Acknowledgement: " << buffer << std::endl;
        }
    }

    closesocket(client_socket);

    // Decrements the active thread counter when the thread finishes
    {
        std::lock_guard<std::mutex> lock(mtx);
        active_threads--;
    }
}

int main() {
    WSADATA wsa;
    SOCKET server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    int addr_len = sizeof(client_addr);
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
    listen(server_socket, 3);
    std::cout << "Server listening on port " << PORT << std::endl;

    while (true) {
        if (active_threads >= 3) {
            break;
        }

        // Attempts to accept any incoming connections
        if ((client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addr_len)) == INVALID_SOCKET) {
            std::cerr << "Accept failed with error code: " << WSAGetLastError() << std::endl;
            return 0;
        }

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
