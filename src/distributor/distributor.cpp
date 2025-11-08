#include "Distributor.h"
#include "../common/common.h"
#include "../utils/connection.h"
#include "../utils/base_connection.h"
#include <unistd.h>
#include "../common/jsonProcess.h"
#include <bits/stdc++.h>

using namespace std;

// ============================================================================
// Distributor constructor
// ============================================================================

Distributor::Distributor(int output_FD, vector<MachineAddress> machine_addresses) : should_stop(false) {
    output_fd = output_FD;
    cnt_worker = (int)machine_addresses.size();
    this->machine_addresses = machine_addresses;

    for (auto machine_address : machine_addresses) {

        // Connect to worker data socket
        int worker_data_socket=connect_to(machine_address.address, machine_address.listening_port);
        if (worker_data_socket == -1) {
            cerr << "[Distributor] Failed to connect to worker data socket at "<<ip_to_string(machine_address.address)<<":"<<machine_address.listening_port<<endl;
            continue;
        }
        worker_data_sockets.push_back(worker_data_socket);
        cout<<"Connected to worker data socket at "<<ip_to_string(machine_address.address)<<":"<<machine_address.listening_port<<endl;

        // Connect to worker control socket
        int worker_control_socket=connect_to(machine_address.address, machine_address.listening_port);
        if (worker_control_socket == -1) {
            cerr << "[Distributor] Failed to connect to worker control socket at "<<ip_to_string(machine_address.address)<<":"<<machine_address.listening_port<<endl;
            continue;
        }
        worker_control_sockets.push_back(worker_control_socket);
        cout<<"Connected to worker control socket at "<<ip_to_string(machine_address.address)<<":"<<machine_address.listening_port<<endl;
    }
}

// ============================================================================
// Send testcase to workers
// ============================================================================

void Distributor::send_testcase(string file_config_path) {
    init_task(file_config_path);

    for (int i=0; i<cnt_worker; ++i) {
        send_file(worker_data_sockets[i], file_config_path, "testcase_config.json");
        for (auto task : taskData) {
            for (auto subtask : task.subtask) {
                for (auto testcase : subtask.testcase) {
                    send_file(worker_data_sockets[i], testcase.input_path, testcase.input_path);
                    send_file(worker_data_sockets[i], testcase.expected_output_path, testcase.expected_output_path);
                    cout<<"Sent testcase "<<testcase.input_path<<" and "<<testcase.expected_output_path<<" to worker "
                    <<ip_to_string(machine_addresses[i].address)<<":"<<machine_addresses[i].listening_port<<"\n";
                }
            }
        }
    }
}

// ============================================================================
// Initialize task
// ============================================================================

void Distributor::init_task(string testcase_config_path) {
    read_json(testcase_config_path, taskData);    
    init_dependencies(taskData, tasks_dependency_counts, tasks_dependency_edges);
}

// ============================================================================
// Start distributor
// ============================================================================

void Distributor::start() {
    for (int i=0; i<(int)worker_data_sockets.size(); ++i) {
        worker_threads.push_back(thread(&Distributor::worker_communication_loop, this, worker_data_sockets[i], worker_control_sockets[i]));
    }
}

// ============================================================================
// Wait for completion
// ============================================================================

void Distributor::wait_for_completion() {
    for (auto& t : worker_threads) {
        if (t.joinable()) {
            t.join();
        }
    }
}

// ============================================================================
// Shutdown distributor
// ============================================================================

void Distributor::shutdown() {
    should_stop = true;
    task_available_cv.notify_all();
    wait_for_completion();
}

// ============================================================================
// Add submission
// ============================================================================

void Distributor::add_submission(const SubmissionInfo& submission_info) {
    int task_id = submission_info.task_id;
    cout<<"[Distributor] Adding submission "<<submission_info.submission_id<<" for task "<<task_id<<endl;

    for (int i = 0; i < (int)taskData[task_id].subtask.size(); ++i) {
        int deg_count = tasks_dependency_counts[task_id][i];
        int mod = taskData[task_id].subtask[i].mod;

        {
            lock_guard<mutex> lock(dependencies_mutex);
            submission_dependencies[submission_info.submission_id][i] = deg_count;
        }
        
        if (deg_count == 0) {
            {
                lock_guard<mutex> lock(submission_queue_mutex);
                for (int j=0; j<mod; ++j) {
                    task_queue.push({
                        submission_info.user_id,
                        submission_info.submission_id,
                        TaskDetail{ task_id, i, mod, j, submission_info.executable_path }
                    });
                }
            }
            cout<<"[Distributor] Added submission "<<submission_info.submission_id<<" to task "<<task_id<<" subtask "<<i<<endl;
            task_available_cv.notify_one();
        }
    }
}

// ============================================================================
// Worker communication loop
// ============================================================================

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
                task = move(task_queue.front());
                task_queue.pop();
                has_task = true;
            }
        }

        if (!has_task) {
            continue;
        }

        cout << "[Distributor] Data socket:" << worker_data_socket << " Processing submission " << task.submission_id 
            << " for task " << task.task_detail.task_id 
            << " subtask " << task.task_detail.subtask_id << "\n";

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
        cout << "[Distributor] Data socket:" << worker_data_socket << " Received result for submission " << task.submission_id 
            << " for task " << task.task_detail.task_id 
            << " subtask " << task.task_detail.subtask_id << "\n";

        {
            lock_guard<mutex> lock(output_fd_mutex);
            if (output_fd >= 0) {
                dprintf(output_fd, "submission_id: %d, task_id: %d, subtask_id: %d, is_accepted: %d\n", 
                        task.submission_id, result.task_id, result.subtask_id, result.is_accepted ? 1 : 0);

                for (size_t i = 0; i < result.test_output.size(); ++i) {
                    const auto& out = result.test_output[i];
                    dprintf(output_fd, "  Test #%zu: verdict=%s, time=%ld ms, memory=%ld MB\n",
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

                    int mod = taskData[task.task_detail.task_id].subtask[s].mod;

                    {
                        lock_guard<mutex> lock(submission_queue_mutex);
                        for (int j=0; j<mod; ++j) {
                            task_queue.push({
                                task.user_id,
                                task.submission_id,
                                TaskDetail{
                                    task.task_detail.task_id,
                                    s,
                                    task.task_detail.mod,
                                    j,
                                    task.task_detail.executable_path
                                }
                            });
                        }
                    }
                    cout<<"[Distributor] All dependencies of submission "<<task.submission_id<<" for task "<<task.task_detail.task_id<<" subtask "<<s<<" are satisfied"<<endl;
                    cout<<"[Distributor] Added submission "<<task.submission_id<<" to task "<<task.task_detail.task_id<<" subtask "<<s<<endl;
                    task_available_cv.notify_one();
                }
            }

        } else {
            // Do early termination
        }
    }
}