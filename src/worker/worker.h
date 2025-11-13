#ifndef WORKER_H
#define WORKER_H

#include <bits/stdc++.h>
#include "../common/common.h"
using namespace std;

class Worker {
private:
    int data_socket;
    int control_socket;
    int max_cache_cnt;
    atomic<bool> early_termination, should_stop;
    vector<Task> taskData;
    vector<int> cache_submission_ids;

    // Private methods
    void controlMessageListener();
    void processTasksLoop();

public:
    Worker(int listening_port);
    void start();
    void init_task(bool receive_testcase_files = false);
};

#endif // WORKER_H