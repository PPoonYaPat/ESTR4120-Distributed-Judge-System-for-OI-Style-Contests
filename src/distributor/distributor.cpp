#include "Distributor.h"
#include "../common/common.h"
#include "../utils/connection.h"
#include <iostream>
#include <fstream>
#include <unistd.h>

using namespace std;

Distributor::Distributor(int output_FD, vector<MachineAddress> machine_addresses) : should_stop(false) {
    output_fd = output_FD;
    for (auto machine_address : machine_addresses) {
        int worker_data_socket=connect_to(machine_address.address, machine_address.listening_port);
        if (worker_data_socket == -1) {
            cerr << "[Distributor] Failed to connect to worker data socket at "<<ip_to_string(machine_address.address)<<":"<<machine_address.listening_port<<endl;
            continue;
        }
        worker_data_sockets.push_back(worker_data_socket);
        cout<<"Connected to worker data socket at "<<ip_to_string(machine_address.address)<<":"<<machine_address.listening_port<<endl;

        int worker_control_socket=connect_to(machine_address.address, machine_address.listening_port);
        if (worker_control_socket == -1) {
            cerr << "[Distributor] Failed to connect to worker control socket at "<<ip_to_string(machine_address.address)<<":"<<machine_address.listening_port<<endl;
            continue;
        }
        worker_control_sockets.push_back(worker_control_socket);
        cout<<"Connected to worker control socket at "<<ip_to_string(machine_address.address)<<":"<<machine_address.listening_port<<endl;
    }
}

void Distributor::init_tasks_dependencies() {
    // Implementation here
}

void Distributor::start() {
    for (int i=0; i<(int)worker_data_sockets.size(); ++i) {
        worker_threads.push_back(thread(&Distributor::worker_communication_loop, this, worker_data_sockets[i], worker_control_sockets[i]));
    }
}

void Distributor::wait_for_completion() {
    for (auto& t : worker_threads) {
        if (t.joinable()) {
            t.join();
        }
    }
}

void Distributor::shutdown() {
    should_stop = true;
    task_available_cv.notify_all();
    wait_for_completion();
}

void Distributor::add_submission(const SubmissionInfo& submission_info) {
    int task_id = submission_info.task_id;

    for (int i = 0; i < (int)tasks_dependency_counts[task_id].size(); ++i) {
        int deg_count = tasks_dependency_counts[task_id][i];
        
        {
            lock_guard<mutex> lock(dependencies_mutex);
            submission_dependencies[submission_info.submission_id][i] = deg_count;
        }
        
        if (deg_count == 0) {
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

void Distributor::test_send_exe(string executable_path) {
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
        return;
    }

    TaskMessage task_message = {1,1,1,1,(int)executable_data.size()};
    sendTaskMessage(task_message, executable_data, worker_data_sockets[0]);
    cout << "Sent executable to worker" << endl;
}

void Distributor::worker_communication_loop(int worker_data_socket, int worker_control_socket) {
    assert(worker_data_socket >= 0 && worker_control_socket >= 0);
    while (!should_stop) {

        bool has_task = false;
        SubgroupTask task;

        {
            unique_lock<mutex> lock(submission_queue_mutex);
            
            task_available_cv.wait(lock, [this] {
                return !task_queue.empty() || should_stop;
            });
            
            if (should_stop && task_queue.empty()) break;
            
            if (!task_queue.empty()) {
                task = task_queue.front();
                task_queue.pop();
                has_task = true;
            }
        }

        if (!has_task) continue;

        string executable_path = task.task_detail.executable_path;

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

        TaskMessage task_message = {
            task.task_detail.task_id,
            task.task_detail.subtask_id,
            task.task_detail.mod,
            task.task_detail.r,
            (int)executable_data.size()
        };

        sendTaskMessage(task_message, executable_data, worker_data_socket);

        Result result = receiveResult(worker_data_socket);

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

                if (remaining == 0) {
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
            // Do early termination
        }
    }
}