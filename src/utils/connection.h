#ifndef CONNECTION_H
#define CONNECTION_H

#include <cstdint>
#include <netinet/in.h>
#include "../common/common.h"
#include <bits/stdc++.h>
#include <arpa/inet.h>
using namespace std;

// return fd on TCP connection success
// otherwise return -1
string ip_to_string(in_addr_t address);
int listen_on_port(uint16_t listen_port);
int accept_connection(int sockfd);
int connect_to(in_addr_t address, uint16_t remote_port);


// Send TaskMessage with executable file to a worker
// task_message: The task message with executable_size field set
// executable_data: The binary executable file data
// socket: The socket to send to
void sendTaskMessage(const TaskMessage& task_message, const vector<char>& executable_data, int socket);

// Receive TaskMessage with executable file from a worker
// socket: The socket to receive from
// Returns: pair containing (TaskMessage with network byte order converted, executable_data)
// Returns empty vector and invalid TaskMessage if receive fails
pair<TaskMessage, vector<char>> receiveTaskMessage(int socket);

#endif