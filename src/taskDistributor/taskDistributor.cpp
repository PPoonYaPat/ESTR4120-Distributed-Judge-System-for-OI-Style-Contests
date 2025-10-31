#include <bits/stdc++.h>
#include "../common/common.h"
#include "../utils/connection.h"
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
    int mod,r;
    string executable_path;
};

struct SubgroupTask {
    int user_id;
    int submission_id;
    TaskDetail task_detail;
};

class TaskDistributor {

private:
    int output_fd;
    mutex output_fd_mutex;

    vector<int> worker_sockets;
    vector<thread> worker_threads;

    mutex submission_queue_mutex;
    condition_variable task_available_cv;
    queue<SubgroupTask> task_queue;
    
    atomic<bool> should_stop{false};
       
    vector<vector<vector<int>>> tasks_dependency_edges; // tasks_dependencies[task_id][subtask_id][dependency_task_id]
    vector<vector<int>> tasks_dependency_counts;

    mutex dependencies_mutex;
    map<int,map<int,int>> submission_dependencies; // submission_dependencies[submission_id][subtask_id]

public:

    TaskDistributor(int output_FD, vector<MachineAddress> machine_addresses) {
        output_fd=output_FD;
        for (auto machine_address : machine_addresses) {
            int worker_socket=connect_to(machine_address.address, machine_address.port);
            if (worker_socket==-1) continue;
            worker_sockets.push_back(worker_socket);
        }
    }

    void init_tasks_dependencies() {

    }

    void start() {
        for (int worker_socket : worker_sockets) {
            worker_threads.push_back(thread(&TaskDistributor::worker_communication_loop, this, worker_socket));
        }
    }
    
    void wait_for_completion() {
        for (auto& t : worker_threads) {
            if (t.joinable()) {
                t.join();
            }
        }
    }
    
    void shutdown() {
        should_stop = true;
        task_available_cv.notify_all();
        wait_for_completion();
    }

    void add_submission(const SubmissionInfo& submission_info) {
        int task_id=submission_info.task_id;

        for (int i=0; i<(int)tasks_dependency_counts[task_id].size(); ++i) {
            int deg_count=tasks_dependency_counts[task_id][i];
            
            {
                lock_guard<mutex> lock(dependencies_mutex);
                submission_dependencies[submission_info.submission_id][i]=deg_count;
            }
            
            if (deg_count==0) {
                {
                    lock_guard<mutex> lock(submission_queue_mutex);
                    task_queue.push({
                        submission_info.user_id,
                        submission_info.submission_id,
                        TaskDetail{
                            task_id,
                            i,
                            1,
                            0,
                            submission_info.executable_path
                        }
                    });
                }
                task_available_cv.notify_one();
            }
        }
    }

    void worker_communication_loop(int worker_socket) {
        while (!should_stop) {

            bool has_task=false;
            SubgroupTask task;

            {
                unique_lock<mutex> lock(submission_queue_mutex);
                
                task_available_cv.wait(lock, [this] {
                    return !task_queue.empty() || should_stop;
                });
                
                if (should_stop && task_queue.empty()) break;
                
                if (!task_queue.empty()) {
                    task=task_queue.front();
                    task_queue.pop();
                    has_task=true;
                }
            }

            if (!has_task) continue;

            string executable_path=task.task_detail.executable_path;

            vector<char> executable_data;
            ifstream exe_file(executable_path, ios::binary | ios::ate);
            if (exe_file) {
                size_t executable_size = exe_file.tellg();
                exe_file.seekg(0, ios::beg);
                executable_data.resize(executable_size);
                exe_file.read(executable_data.data(), executable_size);
                exe_file.close();
            } else {
                cerr << "Error: Cannot open executable file: " << executable_path << endl;
                continue;
            }

            TaskMessage task_message={
                task.task_detail.task_id,
                task.task_detail.subtask_id,
                task.task_detail.mod,
                task.task_detail.r,
                (int)executable_data.size()
            };

            sendTaskMessage(task_message, executable_data, worker_socket);

            Result result=receiveResult(worker_socket);

            {
                lock_guard<mutex> lock(output_fd_mutex);
                if (output_fd >= 0) {
                    dprintf(output_fd, "submission_id: %d, task_id: %d, subtask_id: %d, is_accepted: %d\n", 
                            task.submission_id, result.task_id, result.subtask_id, result.is_accepted ? 1 : 0);

                    for (size_t i = 0; i < result.test_output.size(); ++i) {
                        const auto& out = result.test_output[i];
                        dprintf(output_fd, "  Test #%zu: verdict=%s, time=%d ms, memory=%d MB\n",
                                i+1, out.verdict.c_str(), out.time_usage, out.memory_usage);
                    }

                    fsync(output_fd);
                }
            }

            if (result.is_accepted) {
                
                for (auto s : tasks_dependency_edges[task.task_detail.task_id][task.task_detail.subtask_id]) {
                    int remaining;
                    {
                        lock_guard<mutex> lock(dependencies_mutex);
                        --submission_dependencies[task.submission_id][s];
                        remaining = submission_dependencies[task.submission_id][s];
                    }

                    if (remaining==0) {
                        {
                            lock_guard<mutex> lock(submission_queue_mutex);
                            task_queue.push({
                                task.user_id,
                                task.submission_id,
                                TaskDetail{
                                    task.task_detail.task_id,
                                    s,
                                    task.task_detail.mod,
                                    task.task_detail.r,
                                    task.task_detail.executable_path
                                }
                            });
                        }
                        task_available_cv.notify_one();
                    }
                }

            } else {
                //do early termination
            }
        }
    }
};