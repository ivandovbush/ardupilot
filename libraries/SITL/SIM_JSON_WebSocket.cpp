/*
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "SIM_JSON_WebSocket.h"

#if HAL_SIM_WEBSOCKET_ENABLED

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/select.h>
#include <cstdint>
#include <algorithm>

using namespace SITL;

// SHA1 implementation
uint32_t SHA1::rol(const uint32_t value, const size_t bits) {
    return (value << bits) | (value >> (32 - bits));
}

void SHA1::transform() {
    uint32_t block[80];
    for (size_t i = 0; i < 16; i++) {
        block[i] = (buffer[i*4 + 3]) |
                  (buffer[i*4 + 2] << 8) |
                  (buffer[i*4 + 1] << 16) |
                  (buffer[i*4 + 0] << 24);
    }

    for (size_t i = 16; i < 80; i++) {
        block[i] = rol(block[i-3] ^ block[i-8] ^ block[i-14] ^ block[i-16], 1);
    }

    uint32_t a = state[0], b = state[1], c = state[2], d = state[3], e = state[4];

    for (size_t i = 0; i < 80; i++) {
        uint32_t f, k;
        if (i < 20) {
            f = (b & c) | (~b & d);
            k = 0x5A827999;
        } else if (i < 40) {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1;
        } else if (i < 60) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDC;
        } else {
            f = b ^ c ^ d;
            k = 0xCA62C1D6;
        }

        uint32_t temp = rol(a, 5) + f + e + k + block[i];
        e = d;
        d = c;
        c = rol(b, 30);
        b = a;
        a = temp;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    transforms++;
}

SHA1::SHA1() : transforms(0), bytes(0) {
    state[0] = 0x67452301;
    state[1] = 0xEFCDAB89;
    state[2] = 0x98BADCFE;
    state[3] = 0x10325476;
    state[4] = 0xC3D2E1F0;
}

void SHA1::update(const std::string& s) {
    const uint8_t* data = reinterpret_cast<const uint8_t*>(s.c_str());
    size_t len = s.length();
    
    for (size_t i = 0; i < len; i++) {
        buffer[bytes % 64] = data[i];
        bytes++;
        if (bytes % 64 == 0) {
            transform();
        }
    }
}

std::string SHA1::final() {
    uint64_t total_bits = bytes * 8;
    buffer[bytes % 64] = 0x80;
    size_t pad_to = bytes % 64;
    if (pad_to > 56) {
        memset(buffer + pad_to + 1, 0, 64 - pad_to - 1);
        transform();
        memset(buffer, 0, 56);
    } else {
        memset(buffer + pad_to + 1, 0, 56 - pad_to - 1);
    }
    for (int i = 0; i < 8; i++) {
        buffer[56 + i] = (total_bits >> ((7 - i) * 8)) & 0xFF;
    }
    transform();

    uint8_t hash[20];
    for (int i = 0; i < 5; i++) {
        hash[i*4] = (state[i] >> 24) & 0xFF;
        hash[i*4 + 1] = (state[i] >> 16) & 0xFF;
        hash[i*4 + 2] = (state[i] >> 8) & 0xFF;
        hash[i*4 + 3] = state[i] & 0xFF;
    }
    return std::string(reinterpret_cast<char*>(hash), 20);
}

// JSONWebSocket implementation
JSONWebSocket::JSONWebSocket() :
    server_fd(-1)
{
    printf("JSONWebSocket: Initializing server\n");
    // Initialize client array
    for (size_t i = 0; i < MAX_CLIENTS; i++) {
        clients[i].fd = -1;
        clients[i].handshake_complete = false;
        clients[i].rx_buffer_filled = 0;
        clients[i].active = false;
    }
}

JSONWebSocket::~JSONWebSocket() {
    close();
}

std::string JSONWebSocket::base64_encode(const std::string& input) {
    static const char base64_chars[] = 
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string ret;
    int i = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];
    size_t length = input.size();
    const unsigned char* bytes_to_encode = 
        reinterpret_cast<const unsigned char*>(input.c_str());

    while (length--) {
        char_array_3[i++] = *(bytes_to_encode++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for(i = 0; i < 4; i++)
                ret += base64_chars[char_array_4[i]];
            i = 0;
        }
    }

    if (i) {
        for(int j = i; j < 3; j++)
            char_array_3[j] = '\0';

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;

        for (int j = 0; j < i + 1; j++)
            ret += base64_chars[char_array_4[j]];

        while(i++ < 3)
            ret += '=';
    }
    return ret;
}

std::string JSONWebSocket::generate_websocket_accept(const std::string& key) {
    std::string magic = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    std::string combined = key + magic;
    
    SHA1 sha1;
    sha1.update(combined);
    return base64_encode(sha1.final());
}

bool JSONWebSocket::connect(const char* address, uint16_t port) {
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        printf("Failed to create WebSocket server socket\n");
        return false;
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        printf("Failed to set socket options\n");
        close();
        return false;
    }

    sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_fd, reinterpret_cast<struct sockaddr*>(&server_addr), sizeof(server_addr)) < 0) {
        printf("Failed to bind WebSocket server socket\n");
        close();
        return false;
    }

    if (listen(server_fd, MAX_CLIENTS) < 0) {
        printf("Failed to listen on WebSocket server socket\n");
        close();
        return false;
    }

    // Set non-blocking
    int flags = fcntl(server_fd, F_GETFL, 0);
    fcntl(server_fd, F_SETFL, flags | O_NONBLOCK);

    printf("WebSocket server listening on port %u\n", port);
    return true;
}

void JSONWebSocket::check_new_connections() {
    sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    // Try to accept new connection
    int new_fd = accept(server_fd, reinterpret_cast<struct sockaddr*>(&client_addr), &client_len);
    if (new_fd < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            printf("Failed to accept WebSocket client connection\n");
        }
        return;
    }

    // Find free client slot
    int slot = -1;
    for (size_t i = 0; i < MAX_CLIENTS; i++) {
        if (!clients[i].active) {
            slot = i;
            break;
        }
    }

    if (slot == -1) {
        printf("No free client slots available, rejecting connection\n");
        ::close(new_fd);
        return;
    }

    // Initialize new client
    clients[slot].fd = new_fd;
    clients[slot].handshake_complete = false;
    clients[slot].rx_buffer_filled = 0;
    clients[slot].active = true;

    printf("New WebSocket client connected (slot %d)\n", slot);
}

bool JSONWebSocket::handle_handshake(ClientInfo& client) {
    std::vector<char> buffer;
    buffer.reserve(MAX_HEADER_SIZE);
    buffer.resize(MAX_HEADER_SIZE);
    
    size_t total_bytes = 0;
    const char* end_marker = "\r\n\r\n";  // End of HTTP headers
    const size_t end_marker_len = strlen(end_marker);
    bool headers_complete = false;
    
    // Keep reading until we get the full headers
    while (total_bytes < MAX_HEADER_SIZE) {
        ssize_t bytes_read = ::recv(client.fd, buffer.data() + total_bytes, 
                                  buffer.size() - total_bytes, 0);
        
        if (bytes_read < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // No data available right now, try again
                continue;
            }
            printf("Error reading handshake data: %s\n", strerror(errno));
            return false;
        }
        
        if (bytes_read == 0) {
            printf("Client closed connection during handshake\n");
            return false;
        }
        
        total_bytes += bytes_read;
        buffer[total_bytes] = '\0';  // Null terminate for string operations
        
        // Check if we have the complete headers
        if (memmem(buffer.data(), total_bytes, end_marker, end_marker_len) != nullptr) {
            headers_complete = true;
            break;
        }
        
        // If buffer is full but no end marker, headers are too large
        if (total_bytes >= MAX_HEADER_SIZE - 1) {
            printf("WebSocket headers too large (> %zu bytes)\n", MAX_HEADER_SIZE);
            return false;
        }
        
        // Ensure we have room for more data
        if (total_bytes + 1024 > buffer.size()) {
            buffer.resize(buffer.size() + MAX_HEADER_SIZE);
        }
    }
    
    if (!headers_complete) {
        printf("Failed to receive complete WebSocket headers\n");
        return false;
    }

    std::string request(buffer.data(), total_bytes);
    
    // Check for required WebSocket headers
    if (request.find("GET") == std::string::npos ||
        request.find("Upgrade: websocket") == std::string::npos ||
        request.find("Connection: Upgrade") == std::string::npos) {
        printf("Missing required WebSocket headers\n");
        return false;
    }

    // Find the WebSocket key more robustly
    const char* key_header = "Sec-WebSocket-Key: ";
    size_t key_start = request.find(key_header);
    if (key_start == std::string::npos) {
        printf("WebSocket key header not found\n");
        return false;
    }
    key_start += strlen(key_header);
    
    // Find the end of the key (will be terminated by \r\n)
    size_t key_end = request.find("\r\n", key_start);
    if (key_end == std::string::npos) {
        printf("WebSocket key not properly terminated\n");
        return false;
    }

    std::string client_key = request.substr(key_start, key_end - key_start);
    // Remove any potential whitespace
    client_key.erase(std::remove_if(client_key.begin(), client_key.end(), ::isspace), client_key.end());
    
    if (client_key.empty()) {
        printf("Empty WebSocket key\n");
        return false;
    }

    std::string accept_key = generate_websocket_accept(client_key);

    std::string response = 
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: " + accept_key + "\r\n\r\n";

    // Send the response, handling partial writes
    size_t total_sent = 0;
    const char* response_data = response.c_str();
    const size_t response_len = response.length();
    
    while (total_sent < response_len) {
        ssize_t sent = ::send(client.fd, response_data + total_sent, 
                            response_len - total_sent, 0);
        if (sent < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            printf("Failed to send WebSocket handshake response: %s\n", strerror(errno));
            return false;
        }
        if (sent == 0) {
            printf("Client closed connection while sending handshake response\n");
            return false;
        }
        total_sent += sent;
    }
    
    return true;
}

void JSONWebSocket::remove_client(size_t client_idx) {
    if (clients[client_idx].fd >= 0) {
        ::close(clients[client_idx].fd);
    }
    clients[client_idx].fd = -1;
    clients[client_idx].handshake_complete = false;
    clients[client_idx].rx_buffer_filled = 0;
    clients[client_idx].active = false;
    printf("WebSocket client disconnected (slot %zu)\n", client_idx);
}

ssize_t JSONWebSocket::send_to_client(const ClientInfo& client, const void* buffer, size_t size) {
    std::vector<uint8_t> payload(static_cast<const uint8_t*>(buffer), 
                                static_cast<const uint8_t*>(buffer) + size);
    std::vector<uint8_t> frame = create_websocket_frame(payload, false);
    return ::send(client.fd, frame.data(), frame.size(), 0);
}

ssize_t JSONWebSocket::send(const void* buffer, size_t size) {
    check_new_connections();

    ssize_t total_sent = 0;
    bool any_success = false;

    // Send to all connected clients
    for (size_t i = 0; i < MAX_CLIENTS; i++) {
        if (!clients[i].active) {
            continue;
        }

        if (!clients[i].handshake_complete) {
            if (!handle_handshake(clients[i])) {
                remove_client(i);
                continue;
            }
            clients[i].handshake_complete = true;
        }

        ssize_t sent = send_to_client(clients[i], buffer, size);
        if (sent > 0) {
            total_sent += sent;
            any_success = true;
        } else {
            remove_client(i);
        }
    }

    return any_success ? total_sent : -1;
}

ssize_t JSONWebSocket::recv(void* buffer, size_t size, uint32_t timeout_ms) {
    check_new_connections();

    fd_set read_fds;
    struct timeval tv;
    int max_fd = server_fd;
    
    FD_ZERO(&read_fds);
    FD_SET(server_fd, &read_fds);

    // Add all active clients to fd_set
    for (size_t i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active && clients[i].fd >= 0) {
            FD_SET(clients[i].fd, &read_fds);
            max_fd = std::max(max_fd, clients[i].fd);
        }
    }

    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    int ret = select(max_fd + 1, &read_fds, nullptr, nullptr, &tv);
    if (ret <= 0) {
        return ret;
    }

    // Check each client for data
    for (size_t i = 0; i < MAX_CLIENTS; i++) {
        if (!clients[i].active || clients[i].fd < 0) {
            continue;
        }

        if (!FD_ISSET(clients[i].fd, &read_fds)) {
            continue;
        }

        if (!clients[i].handshake_complete) {
            if (!handle_handshake(clients[i])) {
                remove_client(i);
                continue;
            }
            clients[i].handshake_complete = true;
            continue;
        }

        std::vector<uint8_t> frame_buffer(BUFFER_SIZE);
        ssize_t bytes_read = ::recv(clients[i].fd, frame_buffer.data(), frame_buffer.size(), 0);
        
        if (bytes_read <= 0) {
            remove_client(i);
            continue;
        }

        frame_buffer.resize(bytes_read);
        bool is_text;
        std::vector<uint8_t> payload = parse_websocket_frame(frame_buffer, is_text);
        
        if (payload.empty()) {
            continue;
        }

        size_t copy_size = std::min(size, payload.size());
        memcpy(buffer, payload.data(), copy_size);
        return copy_size;
    }

    return -1;
}

void JSONWebSocket::close() {
    // Close all client connections
    for (size_t i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active) {
            remove_client(i);
        }
    }

    // Close server socket
    if (server_fd >= 0) {
        ::close(server_fd);
        server_fd = -1;
    }
}

std::vector<uint8_t> JSONWebSocket::parse_websocket_frame(const std::vector<uint8_t>& data, bool& is_text) {
    if (data.size() < 2) return std::vector<uint8_t>();

    is_text = (data[0] & 0x0F) == 0x01;
    bool masked = (data[1] & 0x80) != 0;
    uint64_t payload_length = data[1] & 0x7F;
    size_t header_length = 2;

    if (payload_length == 126) {
        if (data.size() < 4) return std::vector<uint8_t>();
        payload_length = (data[2] << 8) | data[3];
        header_length = 4;
    } else if (payload_length == 127) {
        if (data.size() < 10) return std::vector<uint8_t>();
        payload_length = 0;
        for (int i = 0; i < 8; i++) {
            payload_length = (payload_length << 8) | data[2 + i];
        }
        header_length = 10;
    }

    if (masked) header_length += 4;
    if (data.size() < header_length + payload_length) return std::vector<uint8_t>();

    std::vector<uint8_t> payload(data.begin() + header_length, 
                               data.begin() + header_length + payload_length);

    if (masked) {
        uint8_t mask[4];
        std::copy(data.begin() + header_length - 4, 
                 data.begin() + header_length, mask);
        for (size_t i = 0; i < payload.size(); i++) {
            payload[i] ^= mask[i % 4];
        }
    }

    return payload;
}

std::vector<uint8_t> JSONWebSocket::create_websocket_frame(const std::vector<uint8_t>& payload, bool is_text) {
    std::vector<uint8_t> frame;
    frame.push_back(0x80 | (is_text ? 0x01 : 0x02)); // FIN + opcode

    if (payload.size() <= 125) {
        frame.push_back(payload.size());
    } else if (payload.size() <= 65535) {
        frame.push_back(126);
        frame.push_back((payload.size() >> 8) & 0xFF);
        frame.push_back(payload.size() & 0xFF);
    } else {
        frame.push_back(127);
        uint64_t len = payload.size();
        for (int i = 7; i >= 0; i--) {
            frame.push_back((len >> (i * 8)) & 0xFF);
        }
    }

    frame.insert(frame.end(), payload.begin(), payload.end());
    return frame;
}

#endif // HAL_SIM_WEBSOCKET_ENABLED
