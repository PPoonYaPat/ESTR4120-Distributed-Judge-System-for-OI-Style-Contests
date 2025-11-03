#include <bits/stdc++.h>
#include "connection.h"
#include <cstdint>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <cerrno>
#include <cstring>

using namespace std;

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

    return sockfd;
};

int accept_connection(int sockfd) {
    struct sockaddr_in their_addr;
    socklen_t sin_size = sizeof(their_addr);
    int new_fd = accept(sockfd, (struct sockaddr*)&their_addr, &sin_size);

    if (new_fd==-1) {
        perror("accept");
        return -1;
    }

    return new_fd;
} 

int connect_to(in_addr_t address, uint16_t remote_port) { // expect in network order
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
                close(socket);
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
        size_t exec_size = static_cast<size_t>(msg_converted.executable_size);
        while (total_received < exec_size) {
            bytes = recv(socket, executable_data.data() + total_received, 
                        exec_size - total_received, 0);
            if (bytes <= 0) {
                executable_data.clear();
                close(socket);
                break;
            }
            total_received += bytes;
        }
    }
    
    return make_pair(msg_converted, executable_data);
}

void send_file(int fd, string input_path, string output_path) {
    // Open the file to send
    ifstream input_file(input_path, ios::binary | ios::ate);
    if (!input_file) {
        cerr << "[send_file] Error: Cannot open file: " << input_path << endl;
        // Send error indicator
        uint32_t output_path_len = 0;
        send(fd, &output_path_len, sizeof(output_path_len), 0);
        return;
    }
    
    // Get file size
    size_t file_size = input_file.tellg();
    input_file.seekg(0, ios::beg);
    
    // Read file data
    vector<char> file_data(file_size);
    input_file.read(file_data.data(), file_size);
    input_file.close();
    
    // 1. Send output_path length (network byte order)
    uint32_t output_path_len = htonl(output_path.length());
    send(fd, &output_path_len, sizeof(output_path_len), 0);
    
    // 2. Send output_path string
    send(fd, output_path.c_str(), output_path.length(), 0);
    
    // 3. Send file size (network byte order)
    uint32_t file_size_network = htonl(file_size);
    send(fd, &file_size_network, sizeof(file_size_network), 0);
    
    // 4. Send file data
    size_t total_sent = 0;
    while (total_sent < file_size) {
        ssize_t bytes = send(fd, file_data.data() + total_sent, 
                            file_size - total_sent, 0);
        if (bytes <= 0) {
            perror("[send_file] send file data");
            return;
        }
        total_sent += bytes;
    }
    
    cerr << "[send_file] Sent " << file_size << " bytes from " 
         << input_path << " -> " << output_path << endl;
}

void receive_file(int fd) {
    // 1. Receive output_path length
    uint32_t output_path_len_network;
    ssize_t bytes = recv(fd, &output_path_len_network, sizeof(output_path_len_network), 0);
    if (bytes <= 0) {
        cerr << "[receive_file] Error: Failed to receive path length" << endl;
        return;
    }
    
    uint32_t output_path_len = ntohl(output_path_len_network);
    
    // Check for error indicator (length = 0)
    if (output_path_len == 0) {
        cerr << "[receive_file] Error: Sender failed to open file" << endl;
        return;
    }
    
    // 2. Receive output_path string
    vector<char> path_buffer(output_path_len);
    size_t total_received = 0;
    while (total_received < output_path_len) {
        bytes = recv(fd, path_buffer.data() + total_received, 
                    output_path_len - total_received, 0);
        if (bytes <= 0) {
            cerr << "[receive_file] Error: Failed to receive path" << endl;
            return;
        }
        total_received += bytes;
    }
    string output_path(path_buffer.begin(), path_buffer.end());
    
    // 3. Receive file size
    uint32_t file_size_network;
    bytes = recv(fd, &file_size_network, sizeof(file_size_network), 0);
    if (bytes <= 0) {
        cerr << "[receive_file] Error: Failed to receive file size" << endl;
        return;
    }
    
    uint32_t file_size = ntohl(file_size_network);
    
    // 4. Receive file data
    vector<char> file_data(file_size);
    total_received = 0;
    while (total_received < file_size) {
        bytes = recv(fd, file_data.data() + total_received, 
                    file_size - total_received, 0);
        if (bytes <= 0) {
            cerr << "[receive_file] Error: Failed to receive file data" << endl;
            return;
        }
        total_received += bytes;
    }
    
    // 5. Create directory if needed
    size_t last_slash = output_path.find_last_of('/');
    if (last_slash != string::npos) {
        string dir = output_path.substr(0, last_slash);
        int result = system(("mkdir -p " + dir).c_str());
        if (result != 0) {
            cerr << "[receive_file] Error: Failed to create directory: " << dir << endl;
            return;
        }
    }
    
    // 6. Write file to disk
    ofstream output_file(output_path, ios::binary);
    if (!output_file) {
        cerr << "[receive_file] Error: Cannot create file: " << output_path << endl;
        return;
    }
    
    output_file.write(file_data.data(), file_size);
    output_file.close();
    
    cerr << "[receive_file] Received " << file_size << " bytes -> " 
         << output_path << endl;
}