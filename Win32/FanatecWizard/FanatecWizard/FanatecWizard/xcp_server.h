#pragma once
// xcp_server.h - Actual XCP TCP server
#pragma once
#include <winsock2.h>
#include <thread>
#include <atomic>

#pragma comment(lib, "ws2_32.lib")

class XcpServer {
private:
    std::atomic<bool> running_{ false };
    std::thread server_thread_;
    SOCKET server_socket_ = INVALID_SOCKET;

public:
    bool start() {
        // Initialize Winsock
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            return false;
        }

        // Create TCP socket
        server_socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (server_socket_ == INVALID_SOCKET) {
            return false;
        }

        // Bind to port 5555 (XCP standard)
        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(5555);

        if (bind(server_socket_, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
            closesocket(server_socket_);
            return false;
        }

        // Listen for connections
        if (listen(server_socket_, 1) == SOCKET_ERROR) {
            closesocket(server_socket_);
            return false;
        }

        running_ = true;
        server_thread_ = std::thread(&XcpServer::server_loop, this);
        return true;
    }

    void stop() {
        running_ = false;
        if (server_thread_.joinable()) {
            server_thread_.join();
        }
        if (server_socket_ != INVALID_SOCKET) {
            closesocket(server_socket_);
            server_socket_ = INVALID_SOCKET;
        }
        WSACleanup();
    }

private:
    void server_loop() {
        while (running_) {
            // Accept ControlDesk connection
            sockaddr_in client_addr{};
            int client_size = sizeof(client_addr);
            SOCKET client_socket = accept(server_socket_, (sockaddr*)&client_addr, &client_size);

            if (client_socket != INVALID_SOCKET) {
                handle_client(client_socket);
                closesocket(client_socket);
            }
            Sleep(10);
        }
    }

    void handle_client(SOCKET client_socket) {
        // Basic XCP command handling
        uint8_t rx_buffer[64];
        uint8_t tx_buffer[64];

        while (running_) {
            int bytes_received = recv(client_socket, (char*)rx_buffer, sizeof(rx_buffer), 0);

            if (bytes_received > 0) {
                // Simple XCP command processing
                if (rx_buffer[0] == 0xFF) { // CONNECT
                    tx_buffer[0] = 0xFF; // Positive response
                    tx_buffer[1] = 0x00;
                    send(client_socket, (char*)tx_buffer, 2, 0);
                }
                // Add more XCP commands as needed
            }
            else if (bytes_received == 0) {
                break; // Client disconnected
            }
            Sleep(1);
        }
    }
};

static XcpServer g_xcp_server;