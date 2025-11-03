#ifndef WORKER_H
#define WORKER_H

#include <bits/stdc++.h>
#include "../common/common.h"
using namespace std;

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
    Worker(int listening_port);
    void start();
    void init_task(string testcase_config_path);
    void receive_testcase();
};

#endif // WORKER_H