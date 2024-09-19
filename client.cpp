#include <iostream>
#include <string>
#include <cstring>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstdint>
#include <cstdio>
#include <thread>
#include <chrono>

#pragma comment(lib, "Ws2_32.lib")

#define SERVER_IP "192.168.2.25"  // Defines a local IP
#define PORT 49153                   // Defines the port number    
#define BUFFER_SIZE 1024             // Defines the max buffer size
#define FILE_PATH "requested_images/screen.jpeg" // Defines the path to the directory where images will be saved

int main() {
    WSADATA wsa;
    SOCKET client_socket;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE] = {0};
    std::string cmd = "start requested_images/";
    std::string temp = "start requested_images/";
    std::string response;

    // Attempts to initialize WSA
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cerr << "WSAStartup failed. Error Code: " << WSAGetLastError() << std::endl;
        return 1;
    }

    // Attempts to create the client socket
    if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        std::cerr << "Could not create socket: " << WSAGetLastError() << std::endl;
        return 1;
    }

    // Assigns the addresses
    server_addr.sin_family = AF_INET;                   // Defines the address family (IPv4)
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP); // Accepts connections from the specific IP
    server_addr.sin_port = htons(PORT);                 // Converts the port number into network byte order

    // Attempts to connect to the server
    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Connect failed. Error Code: " << WSAGetLastError() << std::endl;
        return 1;
    }

    // Displays that it connected to the server
    std::cout << "Connected to server" << std::endl;

    // Allows the user to send strings of text
    std::cout << "Do you wish to start the stream? (type 'start to start and 'exit' to quit): ";
    std::getline(std::cin, response);

    // Send the message to the server
    if (send(client_socket, response.c_str(), response.length(), 0) < 0) {
        std::cerr << "Send failed. Error Code: " << WSAGetLastError() << std::endl;
    }

    if(recv(client_socket, buffer, BUFFER_SIZE, 0)) {
        std::cout << buffer << std::endl;
    }

    while (true) {
        response = "Client is ready";
        if (send(client_socket, response.c_str(), response.length(), 0) < 0) {
            std::cerr << "Send failed. Error Code: " << WSAGetLastError() << std::endl; 
        }

        // Constructs the file path to where the received image will be stored
        std::string file_path = FILE_PATH;

        // Opens a file to save the received image
        FILE* image_file = fopen(file_path.c_str(), "wb");
        if (!image_file) {
            std::cerr << "Error opening file for writing." << std::endl;
            return 1;
        }

        // Clears the buffer
        memset(buffer, 0, BUFFER_SIZE);

        uint64_t image_size;
        if (recv(client_socket, buffer, BUFFER_SIZE, 0) < 0) {
            std::cerr << "Receive failed. Error code: " << WSAGetLastError() << std::endl;
            break;
        }

        memcpy(&image_size, buffer, sizeof(image_size));
        image_size = ntohl(image_size);
        std::cout << "Image size received: " << image_size << std::endl;

        while (image_size > 0) {
            // Clears the buffer
            memset(buffer, 0, sizeof(buffer));

            // Receives a chunk of the image
            int bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);

            if (bytes_received < 0) {
                std::cerr << "Receive failed. Error code: " << WSAGetLastError() << std::endl;
                break;
            }

            // Writes the received image data to the file
            fwrite(buffer, 1, bytes_received, image_file);

            image_size -= bytes_received;
        }

        fclose(image_file);
        std::cout << "Image received and saved. Opening Image..." << std::endl;
        system((cmd + "screen.jpeg").c_str());

        // Resets strings
        cmd = temp;

        // Clears the buffer
        memset(buffer, 0, BUFFER_SIZE);
        strcpy(buffer, "Image Received");

        // Delay on client side to buffer image transmission
        std::this_thread::sleep_for(std::chrono::seconds(5));

        // Sends acknowledgement
        if (send(client_socket, buffer, strlen(buffer), 0) < 0) {
            std::cerr << "Send failed. Error Code: " << WSAGetLastError() << std::endl;
            break;
        }
    }

    closesocket(client_socket); // Closes the client socket
    WSACleanup();               // Cleans up the WSA environment

    return 0;
}
