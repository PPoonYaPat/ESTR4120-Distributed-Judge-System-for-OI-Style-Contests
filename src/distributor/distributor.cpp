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
    worker_status.resize(cnt_worker, make_pair(-1, -1));

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
// Initialize task
// ============================================================================

void Distributor::init_task(string testcase_config_path, bool send_testcase_files) {
    this->testcase_config_path = testcase_config_path;
    read_json(testcase_config_path, taskData);    
    init_dependencies(taskData, tasks_dependency_counts, tasks_dependency_edges);
    send_testcases(send_testcase_files);
}

void Distributor::init_task_auto_detect(string testcase_config_path, vector<string> testcase_dirs, bool send_testcase_files) {
    this->testcase_config_path = testcase_config_path;
    read_json_auto_detect(testcase_config_path, testcase_dirs, taskData);
    init_dependencies(taskData, tasks_dependency_counts, tasks_dependency_edges);
    send_testcases(send_testcase_files);
}

void Distributor::send_testcases(bool send_testcase_files) {
    for (int i=0; i<cnt_worker; ++i) {
        send_task_details(worker_data_sockets[i], taskData);
        if (send_testcase_files) {
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
}

// ============================================================================
// Start distributor
// ============================================================================

void Distributor::start() {
    for (int i=0; i<(int)worker_data_sockets.size(); ++i) {
        worker_threads.push_back(thread(&Distributor::worker_communication_loop, this, worker_data_sockets[i], worker_control_sockets[i], i));
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

void Distributor::wait_until_all_done() {
    while (true) {
        bool all_done = false;
        
        {
            lock_guard<mutex> queue_lock(submission_queue_mutex);
            lock_guard<mutex> status_lock(worker_status_mutex);
            
            if (task_queue.empty()) {
                all_done = true;
                for (const auto& status : worker_status) {
                    if (status.first != -1) {
                        all_done = false;
                        break;
                    }
                }
            }
        }
        
        if (all_done) {
            cout << "[Distributor] All tasks completed!" << endl;
            return;
        }
        
        this_thread::sleep_for(chrono::milliseconds(100));
    }
}

// ============================================================================
// Shutdown distributor
// ============================================================================

void Distributor::shutdown() {
    wait_until_all_done();
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
                    task_available_cv.notify_one();
                }
            }
            cout<<"[Distributor] Added submission "<<submission_info.submission_id<<" to task "<<task_id<<" subtask "<<i<<endl;
        }
    }
}

// ============================================================================
// Worker communication loop
// ============================================================================

void Distributor::worker_communication_loop(int worker_data_socket, int worker_control_socket, int worker_index) {
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

        {
            lock_guard<mutex> lock(worker_status_mutex);
            worker_status[worker_index] = make_pair(task.submission_id, task.task_detail.subtask_id);
        }

        cout << "[Distributor] Data socket:" << worker_data_socket << " Processing submission " << task.submission_id 
            << " for task " << task.task_detail.task_id 
            << " subtask " << task.task_detail.subtask_id << "\n";

        string executable_path = task.task_detail.executable_path;

        TaskMessage task_message = {
            task.submission_id,
            task.task_detail.task_id,
            task.task_detail.subtask_id,
            task.task_detail.mod,
            task.task_detail.r,
            0 // executable_size is not used in this version
        };
        
        sendTaskMessage(task_message, executable_path, worker_data_socket);

        Result result = receiveResult(worker_data_socket);
        cout << "[Distributor] Data socket:" << worker_data_socket << " Received result for submission " << task.submission_id 
            << " for task " << task.task_detail.task_id 
            << " subtask " << task.task_detail.subtask_id << "\n";

        {
            lock_guard<mutex> lock(output_fd_mutex);
            if (output_fd >= 0) {
                dprintf(output_fd, "submission_id: %d, task_id: %d, subtask_id: %d, is_accepted: %d\n", 
                        task.submission_id, result.task_id, result.subtask_id, result.is_accepted ? 1 : 0);

                int r=task.task_detail.r, mod=task.task_detail.mod;
                if (r==0) r=mod;

                for (size_t i = 0; i < result.test_output.size(); ++i) {
                    const auto& out = result.test_output[i];
                    dprintf(output_fd, "  Test #%zu: verdict=%s, time=%ld ms, memory=%ld MB\n",
                            i*mod+r, out.verdict.c_str(), out.time_usage, out.memory_usage);
                }

                fsync(output_fd);
            }
        }

        {
            lock_guard<mutex> lock(worker_status_mutex);
            worker_status[worker_index] = make_pair(-1, -1);
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
                                    mod,
                                    j,
                                    task.task_detail.executable_path
                                }
                            });
                            task_available_cv.notify_one();
                        }
                    }
                    cout<<"[Distributor] All dependencies of submission "<<task.submission_id<<" for task "<<task.task_detail.task_id<<" subtask "<<s<<" are satisfied"<<endl;
                    cout<<"[Distributor] Added submission "<<task.submission_id<<" to task "<<task.task_detail.task_id<<" subtask "<<s<<endl;
                }
            }

        } else {
            // Do early termination
            for (auto s : tasks_dependency_edges[task.task_detail.task_id][task.task_detail.subtask_id]) {
                bool output_to_console_yet=true;
                {
                    lock_guard<mutex> lock(dependencies_mutex);
                    if (submission_dependencies[task.submission_id][s] != -1) {
                        submission_dependencies[task.submission_id][s] = -1;
                        output_to_console_yet = false;
                    }
                }

                if (!output_to_console_yet) {
                    {
                        lock_guard<mutex> lock(output_fd_mutex);
                        if (output_fd >= 0) {
                            dprintf(output_fd, "submission_id: %d, task_id: %d, subtask_id: %d, is_accepted: %d\n", 
                                    task.submission_id, task.task_detail.task_id, task.task_detail.subtask_id, 0);

                            dprintf(output_fd, "  Test #1: verdict=skipped\n");
            
                            fsync(output_fd);
                        }
                    }
                }
            }

            // remove the same submission_id and subtask_id from the task_queue
            {
                lock_guard<mutex> lock(submission_queue_mutex);
                queue<SubgroupTask> filtered_queue;
                while (!task_queue.empty()) {
                    SubgroupTask current = move(task_queue.front());
                    task_queue.pop();
                    if (!(current.submission_id == task.submission_id && current.task_detail.subtask_id == task.task_detail.subtask_id)) {
                        filtered_queue.push(move(current));
                    }
                }
                task_queue = move(filtered_queue);
            }

            // remove the same submission_id and subtask_id from the worker and change worker_status
            {
                lock_guard<mutex> lock(worker_status_mutex);
                for (int i=0; i<cnt_worker; ++i) {
                    if (worker_status[i].first == task.submission_id && worker_status[i].second == task.task_detail.subtask_id) {
                        worker_status[i] = make_pair(-1, -1);
                        send_int(worker_control_sockets[i], 0);
                    }
                }
            }
        }
    }
}