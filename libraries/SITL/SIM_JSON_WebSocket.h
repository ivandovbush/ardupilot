#pragma once

#include <AP_HAL/AP_HAL_Boards.h>

#ifndef HAL_SIM_WEBSOCKET_ENABLED
#define HAL_SIM_WEBSOCKET_ENABLED (CONFIG_HAL_BOARD == HAL_BOARD_SITL)
#endif

#if HAL_SIM_WEBSOCKET_ENABLED

#include <AP_HAL/utility/Socket_native.h>
#include <stdint.h>
#include <string>
#include <vector>
#include <cstddef>

namespace SITL {

// SHA1 implementation
class SHA1 {
private:
    uint32_t state[5];
    uint8_t buffer[64];
    uint64_t transforms;
    uint64_t bytes;

    static uint32_t rol(const uint32_t value, const size_t bits);
    void transform();

public:
    SHA1();
    void update(const std::string& s);
    std::string final();
};

class JSONWebSocket {
public:
    JSONWebSocket();
    ~JSONWebSocket();

    // Initialize and start the WebSocket server
    bool connect(const char* address, uint16_t port);

    // Send data to all connected clients
    ssize_t send(const void* buffer, size_t size);

    // Receive data from any connected client with timeout
    ssize_t recv(void* buffer, size_t size, uint32_t timeout_ms);

    // Close all connections
    void close();

private:
    static constexpr size_t BUFFER_SIZE = 4096;
    static constexpr size_t MAX_HEADER_SIZE = 1024;
    static constexpr size_t MAX_CLIENTS = 10;  // Maximum number of simultaneous clients

    struct ClientInfo {
        int fd;                     // Client socket file descriptor
        bool handshake_complete;    // Whether WebSocket handshake is complete
        uint8_t rx_buffer[BUFFER_SIZE];  // Receive buffer for this client
        size_t rx_buffer_filled;    // Amount of data in receive buffer
        bool active;                // Whether this client slot is in use
    };

    int server_fd;                  // Server socket file descriptor
    ClientInfo clients[MAX_CLIENTS];  // Array of client connections
    
    // WebSocket handshake and frame handling
    bool handle_handshake(ClientInfo& client);
    void check_new_connections();
    void remove_client(size_t client_idx);
    ssize_t send_to_client(const ClientInfo& client, const void* buffer, size_t size);
    
    // Helper functions
    std::string base64_encode(const std::string& input);
    std::string generate_websocket_accept(const std::string& key);
    std::vector<uint8_t> parse_websocket_frame(const std::vector<uint8_t>& data, bool& is_text);
    std::vector<uint8_t> create_websocket_frame(const std::vector<uint8_t>& payload, bool is_text);
};

} // namespace SITL

#endif // HAL_SIM_WEBSOCKET_ENABLED
