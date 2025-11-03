#include "base_connection.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <stdexcept>
#include <iostream>

using namespace std;

// ============================================================================
// Low-level socket operations
// ============================================================================

void send_all(int socket, const void* data, size_t len) {
    const char* ptr = (const char*)data;
    size_t remaining = len;
    
    while (remaining > 0) {
        ssize_t sent = send(socket, ptr, remaining, 0);
        if (sent <= 0) {
            throw runtime_error("send_all failed: " + string(strerror(errno)));
        }
        ptr += sent;
        remaining -= sent;
    }
}

void recv_all(int socket, void* data, size_t len) {
    char* ptr = (char*)data;
    size_t remaining = len;
    
    while (remaining > 0) {
        ssize_t received = recv(socket, ptr, remaining, 0);
        if (received <= 0) {
            if (received == 0) {
                throw runtime_error("recv_all: Connection closed by peer");
            } else {
                throw runtime_error("recv_all failed: " + string(strerror(errno)));
            }
        }
        ptr += received;
        remaining -= received;
    }
}

// ============================================================================
// Integer send/receive with network byte order conversion
// ============================================================================

void send_int(int socket, int value) {
    int network_value = htonl(value);
    send_all(socket, &network_value, sizeof(network_value));
}

int recv_int(int socket) {
    int network_value;
    recv_all(socket, &network_value, sizeof(network_value));
    return ntohl(network_value);
}

// ============================================================================
// String send/receive
// ============================================================================

void send_string(int socket, const string& str) {
    send_int(socket, str.size());
    if (str.size() > 0) {
        send_all(socket, str.c_str(), str.size());
    }
}

string recv_string(int socket) {
    int len = recv_int(socket);
    if (len == 0) {
        return "";
    }
    
    vector<char> buffer(len);
    recv_all(socket, buffer.data(), len);
    return string(buffer.begin(), buffer.end());
}

// ============================================================================
// Binary data send/receive
// ============================================================================

void send_binary(int socket, const vector<char>& data) {
    send_int(socket, data.size());
    if (data.size() > 0) {
        send_all(socket, data.data(), data.size());
    }
}

vector<char> recv_binary(int socket) {
    int len = recv_int(socket);
    if (len == 0) {
        return vector<char>();
    }
    
    vector<char> data(len);
    recv_all(socket, data.data(), len);
    return data;
}

// ============================================================================
// Connection establishment
// ============================================================================

string ip_to_string(in_addr_t address) {
    char addr_str[INET_ADDRSTRLEN];
    struct in_addr addr;
    addr.s_addr = address;
    inet_ntop(AF_INET, &addr, addr_str, INET_ADDRSTRLEN);
    return string(addr_str);
}

int listen_on_port(uint16_t listen_port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return -1;
    }
    
    int yes = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) < 0) {
        perror("setsockopt");
        close(sockfd);
        return -1;
    }
    
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(listen_port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(sockfd);
        return -1;
    }

    if (listen(sockfd, 10) < 0) {
        perror("listen");
        close(sockfd);
        return -1;
    }

    return sockfd;
}

int accept_connection(int sockfd) {
    struct sockaddr_in their_addr;
    socklen_t sin_size = sizeof(their_addr);
    int new_fd = accept(sockfd, (struct sockaddr*)&their_addr, &sin_size);

    if (new_fd == -1) {
        perror("accept");
        return -1;
    }

    return new_fd;
}

int connect_to(in_addr_t address, uint16_t remote_port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(remote_port);
    server_addr.sin_addr.s_addr = address;

    int connect_result = connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    
    if (connect_result < 0) {
        perror("connect");
        close(sockfd);
        return -1;
    }

    return sockfd;
}