#include <iostream>
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <vector>
#include <atomic>
#include <memory>
#include <mutex>
#include <wx/mstream.h>
#include <wx/wx.h>
#include <wx/image.h>
#include <wx/buffer.h>

#pragma comment(lib, "Ws2_32.lib")

#define SERVER_IP "127.0.0.1"
#define PORT 49153
#define BUFFER_SIZE 4096

class ImageBuffer {
public:
    void updateImage(const std::vector<char>& imageData) {
        std::lock_guard<std::mutex> lock(mutex);
        currentImage = imageData;
    }

    std::vector<char> getImage() {
        std::lock_guard<std::mutex> lock(mutex);
        return currentImage;
    }

private:
    std::vector<char> currentImage;
    std::mutex mutex;
};

// Shared image buffer
ImageBuffer globalImageBuffer;
std::atomic<bool> isRunning(true);

void networkThread() {
    WSADATA wsa;
    SOCKET client_socket;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];

    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cerr << "WSAStartup failed" << std::endl;
        return;
    }

    // Create socket
    if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        std::cerr << "Could not create socket" << std::endl;
        WSACleanup();
        return;
    }

    // Prepare server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    server_addr.sin_port = htons(PORT);

    // Connect to server
    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Connect failed" << std::endl;
        closesocket(client_socket);
        WSACleanup();
        return;
    }

    // Send start command
    std::string start_msg = "start";
    send(client_socket, start_msg.c_str(), start_msg.length(), 0);

    while (isRunning) {
        uint64_t image_size;
        if (recv(client_socket, reinterpret_cast<char*>(&image_size), sizeof(image_size), 0) <= 0) {
            std::cerr << "Failed to receive image size" << std::endl;
            break;
        }
        image_size = ntohl(image_size);

        // Receive image data
        std::vector<char> imageData(image_size);
        size_t total_received = 0;

        while (total_received < image_size) {
            int bytes_received = recv(client_socket, 
                                      imageData.data() + total_received, 
                                      image_size - total_received, 
                                      0);
            
            if (bytes_received <= 0) {
                break;
            }
            total_received += bytes_received;
        }

        std::cout << "Received complete image of size: " << total_received << std::endl;
        // Update global image buffer
        globalImageBuffer.updateImage(imageData);
    }

    closesocket(client_socket);
    WSACleanup();
}

class StreamFrame : public wxFrame {
public:
    StreamFrame() : wxFrame(NULL, wxID_ANY, "Screen Stream", 
                          wxDefaultPosition, wxSize(800, 600)) 
    {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        SetBackgroundColour(*wxBLACK);

        // Create panel and bitmap
        panel = new wxPanel(this, wxID_ANY);
        panel->SetBackgroundColour(*wxBLACK);

        // Timer setup
        refreshTimer = new wxTimer(this, wxID_ANY);
        refreshTimer->Start(33);

        // Event bindings
        panel->Bind(wxEVT_PAINT, &StreamFrame::OnPaint, this);
        Bind(wxEVT_TIMER, &StreamFrame::OnRefresh, this);
        Bind(wxEVT_SIZE, &StreamFrame::OnResize, this);
        Bind(wxEVT_CLOSE_WINDOW, &StreamFrame::OnClose, this);
    }

    ~StreamFrame() {
        if (refreshTimer) {
            refreshTimer->Stop();
            delete refreshTimer;
        }
    }

    void OnClose(wxCloseEvent& event) {
        isRunning = false;  // Stop network thread
        if (refreshTimer) {
            refreshTimer->Stop();
            delete refreshTimer;
        }
        // Allow window to close
        event.Skip();
    }

private:
    wxPanel* panel;
    wxTimer* refreshTimer;
    wxBitmap currentBitmap;

    void OnPaint(wxPaintEvent& evt) {
        wxPaintDC dc(panel);
        if(currentBitmap.IsOk()) {
            wxSize size = panel->GetSize();
            int x = (size.GetWidth() - currentBitmap.GetWidth()) / 2;
            int y = (size.GetHeight() - currentBitmap.GetHeight()) / 2;
            dc.DrawBitmap(currentBitmap, x, y, false);
        }
    }

    void OnRefresh(wxTimerEvent& event) {
        static int frames = 0;
        static auto lastTime = std::chrono::steady_clock::now();
        
        frames++;
        auto currentTime = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(currentTime - lastTime);
        
        if (elapsed.count() >= 1) {
            std::cout << "Client FPS: " << frames << std::endl;
            frames = 0;
            lastTime = currentTime;
        }

        std::vector<char> imageData = globalImageBuffer.getImage();
        if (!imageData.empty()) {
            wxMemoryInputStream mis(imageData.data(), imageData.size());
            wxImage img(mis, wxBITMAP_TYPE_JPEG);
            if (img.IsOk()) {
                wxSize size = GetClientSize();
                img.Rescale(size.GetWidth(), size.GetHeight(), wxIMAGE_QUALITY_HIGH);
                currentBitmap = wxBitmap(img);
                panel->Refresh(false);
            }
        }
    }

    void OnResize(wxSizeEvent& event) {
        if (panel) {
            panel->SetSize(GetClientSize());
        }
        event.Skip();
    }
};

class StreamApp : public wxApp {
public:
    bool OnInit() {
        wxInitAllImageHandlers();
        
        std::thread network(networkThread);
        network.detach();

        // Create and show main frame
        StreamFrame* frame = new StreamFrame();
        frame->Show(true);
        return true;
    }
};

wxIMPLEMENT_APP(StreamApp);
