#include <bits/stdc++.h>
#include "connection.h"
#include "../common/common.h"
#include <cstdint>
#include <netinet/in.h>
#include <sys/socket.h>

using namespace std;

int start_connection(uint16_t listen_port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return -1;
    }
    
    int yes = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) < 0) {
        perror("setsockopt");
        return -1;
    }
    
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(listen_port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        return -1;
    }

    if (listen(sockfd, 10) < 0) {
        perror("listen");
        return -1;
    }

    sin_size=sizeof(their_addr);
    new_fd=accept(sockfd, (struct sockaddr*)&their_addr, &sin_size);

    if (new_fd==-1) {
        perror("accept");
        return -1;
    }

    close(sockfd);
    return new_fd;
};

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

    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        return -1;
    }
    
    return sockfd;
};

void sendTaskMessage(const TaskMessage& task_message, const vector<char>& executable_data, int socket) {
    // Prepare TaskMessage with network byte order
    TaskMessage msg_to_send;
    msg_to_send.task_id = htonl(task_message.task_id);
    msg_to_send.subtask_id = htonl(task_message.subtask_id);
    msg_to_send.mod = htonl(task_message.mod);
    msg_to_send.r = htonl(task_message.r);
    msg_to_send.executable_size = htonl(task_message.executable_size);

    // Send TaskMessage struct
    send(socket, &msg_to_send, sizeof(msg_to_send), 0);

    // Send executable binary data
    if (!executable_data.empty()) {
        size_t total_sent = 0;
        while (total_sent < executable_data.size()) {
            ssize_t bytes = send(socket, executable_data.data() + total_sent, 
                                 executable_data.size() - total_sent, 0);
            if (bytes <= 0) {
                perror("send executable data");
                break;
            }
            total_sent += bytes;
        }
    }
}

pair<TaskMessage, vector<char>> receiveTaskMessage(int socket) {
    TaskMessage msg_received;
    vector<char> executable_data;
    
    // Receive TaskMessage struct
    ssize_t bytes = recv(socket, &msg_received, sizeof(msg_received), 0);
    if (bytes <= 0) {
        // Return invalid TaskMessage if receive fails
        msg_received.task_id = -1;
        return make_pair(msg_received, executable_data);
    }
    
    // Convert from network byte order
    TaskMessage msg_converted;
    msg_converted.task_id = ntohl(msg_received.task_id);
    msg_converted.subtask_id = ntohl(msg_received.subtask_id);
    msg_converted.mod = ntohl(msg_received.mod);
    msg_converted.r = ntohl(msg_received.r);
    msg_converted.executable_size = ntohl(msg_received.executable_size);
    
    // Receive executable binary data
    if (msg_converted.executable_size > 0) {
        executable_data.resize(msg_converted.executable_size);
        size_t total_received = 0;
        while (total_received < msg_converted.executable_size) {
            bytes = recv(socket, executable_data.data() + total_received, 
                        msg_converted.executable_size - total_received, 0);
            if (bytes <= 0) {
                executable_data.clear();
                break;
            }
            total_received += bytes;
        }
    }
    
    return make_pair(msg_converted, executable_data);
}