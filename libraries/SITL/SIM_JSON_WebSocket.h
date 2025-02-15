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

    // Send data to connected client
    ssize_t send(const void* buffer, size_t size);

    // Receive data from connected client with timeout
    ssize_t recv(void* buffer, size_t size, uint32_t timeout_ms);

    // Check if client is connected
    bool is_connected() const { return client_fd >= 0; }

    // Close connection
    void close();

private:
    static constexpr size_t BUFFER_SIZE = 4096;
    static constexpr size_t MAX_HEADER_SIZE = 1024;

    int server_fd;  // Server socket file descriptor
    int client_fd;  // Client socket file descriptor
    
    // WebSocket handshake and frame handling
    bool handle_handshake();
    bool decode_websocket_frame(uint8_t* buffer, size_t size, uint8_t* payload, size_t& payload_length);
    bool encode_websocket_frame(const uint8_t* payload, size_t payload_length, uint8_t* frame, size_t& frame_length);
    
    // Helper functions
    std::string generate_websocket_key(const char* client_key);
    void accept_client();
    
    uint8_t rx_buffer[BUFFER_SIZE];
    size_t rx_buffer_filled;

    std::string base64_encode(const std::string& input);
    std::string generate_websocket_accept(const std::string& key);
    std::vector<uint8_t> parse_websocket_frame(const std::vector<uint8_t>& data, bool& is_text);
    std::vector<uint8_t> create_websocket_frame(const std::vector<uint8_t>& payload, bool is_text);
};

} // namespace SITL

#endif // HAL_SIM_WEBSOCKET_ENABLED
