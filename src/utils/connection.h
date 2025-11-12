#ifndef CONNECTION_H
#define CONNECTION_H

#include "../common/common.h"
#include "base_connection.h"
#include <bits/stdc++.h>

using namespace std;

// ============================================================================
// Task Message Protocol
// ============================================================================

// Send TaskMessage with executable file to a worker
// task_message: The task message with executable_size field set
// executable_data: The binary executable file data
// socket: The socket to send to
void sendTaskMessage(const TaskMessage& task_message, const vector<char>& executable_data, int socket);

// Receive TaskMessage with executable file from distributor
// socket: The socket to receive from
// Returns: pair containing (TaskMessage, executable_data)
pair<TaskMessage, vector<char>> receiveTaskMessage(int socket);

// ============================================================================
// Result Protocol
// ============================================================================

// Send a Result to distributor
void sendResult(const Result& result, int socket);

// Receive a Result from worker
Result receiveResult(int socket);

// ============================================================================
// File Transfer Protocol
// ============================================================================

// Send a file from input_path and specify where it should be saved (output_path)
void send_file(int socket, const string& input_path, const string& output_path);

// Receive a file and save it to the path specified by sender
void receive_file(int socket);

// Send task details to worker
void send_task_details(int socket, vector<Task>& taskData);

// Receive task details from distributor
void receive_task_details(int socket, vector<Task>& taskData);

#endif // CONNECTION_H