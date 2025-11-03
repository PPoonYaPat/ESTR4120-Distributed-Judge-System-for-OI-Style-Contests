#ifndef DISTRIBUTOR_H
#define DISTRIBUTOR_H

#include <bits/stdc++.h>
#include "../common/common.h"
using namespace std;

struct SubmissionInfo {
    int user_id;
    int submission_id;
    int task_id;
    string executable_path;
};

struct TaskDetail {
    int task_id;
    int subtask_id;
    int mod;
    int r;
    string executable_path;
};

struct SubgroupTask {
    int user_id;
    int submission_id;
    TaskDetail task_detail;
};

struct MachineAddress; // Forward declaration from common.h

class Distributor {
private:
    int output_fd, cnt_worker;
    vector<MachineAddress> machine_addresses;
    mutex output_fd_mutex;

    vector<int> worker_data_sockets,worker_control_sockets;
    vector<thread> worker_threads;

    mutex submission_queue_mutex;
    condition_variable task_available_cv;
    queue<SubgroupTask> task_queue;
    
    atomic<bool> should_stop;
       
    vector<vector<vector<int>>> tasks_dependency_edges;
    vector<vector<int>> tasks_dependency_counts;

    mutex dependencies_mutex;
    map<int, map<int, int>> submission_dependencies;

    vector<Task> taskData;

    // Private methods
    void worker_communication_loop(int worker_data_socket, int worker_control_socket);

public:
    Distributor(int output_FD, vector<MachineAddress> machine_addresses);
    
    void start();
    void wait_for_completion();
    void shutdown();
    void add_submission(const SubmissionInfo& submission_info);
    void send_testcase(string file_config_path);
    void init_task(string testcase_config_path);
};

#endif // DISTRIBUTOR_H