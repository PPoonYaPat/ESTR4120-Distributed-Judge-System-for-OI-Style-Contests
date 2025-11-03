#ifndef COMMON_H
#define COMMON_H

#include <bits/stdc++.h>
#include <netinet/in.h>
using namespace std;

struct Output {
    int time_usage;
    int memory_usage;
    string verdict; // correct, WA, TLE, MLE, RE, ...
    string user_output;
};

struct Result {
    int task_id;
    int subtask_id;
    int maximum_time;
    int maximum_memory;
    bool is_accepted;
    vector<Output> test_output;
};

struct TaskMessage {
    int task_id;
    int subtask_id;
    int mod, r;
    int executable_size; // size of the executable file in bytes
    // every subtask_id that %mod==r will be assigned to this worker
    // After this struct, executable_size bytes of executable binary data will be sent
};

struct Testcase {
    string input_path;
    string expected_output_path;
};

struct Subtask {
    int task_id;
    int subtask_id;
    vector<Testcase> testcase;
};

struct Task {
    int task_id;
    int memory_limit;
    int time_limit;
    vector<Subtask> subtask;
};

struct MachineAddress {
    in_addr_t address;
    uint16_t listening_port;
};

#endif // COMMON_H