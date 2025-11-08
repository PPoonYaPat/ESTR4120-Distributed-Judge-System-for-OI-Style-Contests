#ifndef WORKER_H
#define WORKER_H

#include <bits/stdc++.h>
#include "../common/common.h"
using namespace std;

class Worker {
private:
    int data_socket;
    int control_socket;
    atomic<bool> early_termination, should_stop;
    vector<Task> taskData;

    // Private methods
    void controlMessageListener();
    void processTasksLoop();

public:
    Worker(int listening_port);
    void start();
    void init_task(string testcase_config_path);
    void receive_testcase();
};

#endif // WORKER_H