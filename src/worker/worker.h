#ifndef WORKER_H
#define WORKER_H

#include <bits/stdc++.h>
using namespace std;

// Forward declarations
struct Testcase {
    string input_path;
    string expected_output_path;
    string output_path;
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

struct Output; // Defined in common.h

class Worker {
private:
    int data_socket;
    int control_socket;
    atomic<bool> early_termination;
    vector<Task> taskData;

    // Private methods
    void controlMessageListener();
    void processTasksLoop();
    Output execute_code(Testcase testcase, int time_limit, int memory_limit, string executable_path);

public:
    Worker(string testcase_config_path, int listening_port);
    void start();
    void init_task(string testcase_config_path);
    void test_receive_exe();
};

#endif // WORKER_H