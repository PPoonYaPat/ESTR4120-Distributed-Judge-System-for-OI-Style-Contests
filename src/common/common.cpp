#include <bits/stdc++.h>
#include "common.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
using namespace std;

Result receiveResult(int socket) {
    Result result;
    
    // Step 1: Receive header
    int task_id, subtask_id, max_time, max_memory, is_accepted, num_outputs;
    
    recv(socket, &task_id, sizeof(task_id), 0);
    recv(socket, &subtask_id, sizeof(subtask_id), 0);
    recv(socket, &max_time, sizeof(max_time), 0);
    recv(socket, &max_memory, sizeof(max_memory), 0);
    recv(socket, &is_accepted, sizeof(is_accepted), 0);
    recv(socket, &num_outputs, sizeof(num_outputs), 0);
    
    result.task_id = ntohl(task_id);
    result.subtask_id = ntohl(subtask_id);
    result.maximum_time = ntohl(max_time);
    result.maximum_memory = ntohl(max_memory);
    result.is_accepted = (ntohl(is_accepted) == 1);
    int count = ntohl(num_outputs);
    
    // Step 2: Receive each Output
    for (int i = 0; i < count; i++) {
        Output output;
        
        // Receive fixed fields
        int time_usage, memory_usage;
        recv(socket, &time_usage, sizeof(time_usage), 0);
        recv(socket, &memory_usage, sizeof(memory_usage), 0);
        
        output.time_usage = ntohl(time_usage);
        output.memory_usage = ntohl(memory_usage);
        
        // Receive verdict string
        int verdict_len;
        recv(socket, &verdict_len, sizeof(verdict_len), 0);
        verdict_len = ntohl(verdict_len);
        
        char* verdict_buf = new char[verdict_len + 1];
        recv(socket, verdict_buf, verdict_len, 0);
        verdict_buf[verdict_len] = '\0';
        output.verdict = string(verdict_buf);
        delete[] verdict_buf;
        
        // Receive user_output string
        int user_output_len;
        recv(socket, &user_output_len, sizeof(user_output_len), 0);
        user_output_len = ntohl(user_output_len);
        
        char* user_output_buf = new char[user_output_len + 1];
        recv(socket, user_output_buf, user_output_len, 0);
        user_output_buf[user_output_len] = '\0';
        output.user_output = string(user_output_buf);
        delete[] user_output_buf;
        
        result.test_output.push_back(output);
    }
    
    return result;
}

void sendResult(const Result& result, int socket) {
    int task_id = htonl(result.task_id);
    int subtask_id = htonl(result.subtask_id);
    int max_time = htonl(result.maximum_time);
    int max_memory = htonl(result.maximum_memory);
    int is_accepted = htonl(result.is_accepted ? 1 : 0);
    int num_outputs = htonl(result.test_output.size());
    
    send(socket, &task_id, sizeof(task_id), 0);
    send(socket, &subtask_id, sizeof(subtask_id), 0);
    send(socket, &max_time, sizeof(max_time), 0);
    send(socket, &max_memory, sizeof(max_memory), 0);
    send(socket, &is_accepted, sizeof(is_accepted), 0);
    send(socket, &num_outputs, sizeof(num_outputs), 0);
    
    for (const auto& output : result.test_output) {
        int time_usage = htonl(output.time_usage);
        int memory_usage = htonl(output.memory_usage);
        
        send(socket, &time_usage, sizeof(time_usage), 0);
        send(socket, &memory_usage, sizeof(memory_usage), 0);
        
        int verdict_len = htonl(output.verdict.size());
        send(socket, &verdict_len, sizeof(verdict_len), 0);
        send(socket, output.verdict.c_str(), output.verdict.size(), 0);
        
        int user_output_len = htonl(output.user_output.size());
        send(socket, &user_output_len, sizeof(user_output_len), 0);
        send(socket, output.user_output.c_str(), output.user_output.size(), 0);
    }
}