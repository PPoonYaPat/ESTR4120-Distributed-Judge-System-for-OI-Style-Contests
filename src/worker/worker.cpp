#include "Worker.h"
#include "../utils/connection.h"
#include "executor.h"
#include "../common/common.h"
#include "../utils/base_connection.h"
#include "../common/jsonProcess.h"
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>
#include <bits/stdc++.h>

using namespace std;

// ============================================================================
// Worker constructor
// ============================================================================

Worker::Worker(int listening_port) : early_termination(false), should_stop(false) {

    int listening_socket = listen_on_port(listening_port);
    if (listening_socket == -1) {
        cerr << "Error: Failed to listen on port " << listening_port << endl;
        exit(1);
    }
    cout << "[Worker] Listening for connections on port " << listening_port << endl;

    data_socket = accept_connection(listening_socket);
    if (data_socket == -1) {
        cerr << "Error: Failed to accept data connection" << endl;
        exit(1);
    }
    cout << "[Worker] Accepted data connection" << endl;
    cout.flush();

    control_socket = accept_connection(listening_socket);
    if (control_socket == -1) {
        cerr << "Error: Failed to accept control connection" << endl;
        exit(1);
    }
    cout << "[Worker] Accepted control connection" << endl;
    cout.flush();

    close(listening_socket);
}

// ============================================================================
// Start worker
// ============================================================================

void Worker::start() {
    thread control_thread(&Worker::controlMessageListener, this);
    thread process_thread(&Worker::processTasksLoop, this);

    control_thread.join();
    process_thread.join();
}

// ============================================================================
// Receive testcase
// ============================================================================

void Worker::receive_testcase() {
    receive_file(data_socket);
    init_task("testcase_config.json");

    for (auto task : taskData) {
        for (auto subtask : task.subtask) {
            for (auto testcase : subtask.testcase) {
                receive_file(data_socket);
                receive_file(data_socket);
                cout<<"Received testcase "<<testcase.input_path<<" and "<<testcase.expected_output_path<<" from distributor"<<endl;
            }
        }
    }
}

// ============================================================================
// Initialize task
// ============================================================================

void Worker::init_task(string testcase_config_path) {
    read_json(testcase_config_path, taskData);
}

// ============================================================================
// Control message listener
// ============================================================================

void Worker::controlMessageListener() {
    while (!should_stop) {
        if (recv_int(control_socket) == 1) {
            cout<<"[Control Thread] Received shutdown signal"<<endl;
            should_stop = true;

        } else {
            cout<<"[Control Thread] Received early termination signal"<<endl;
            early_termination = true;
        }
    }
}

// ============================================================================
// Process tasks loop
// ============================================================================

void Worker::processTasksLoop() {
    while (!should_stop) {
        auto [task_message, executable_data] = receiveTaskMessage(data_socket);
        cout<<"[Worker] Received task message from distributor"<<endl;
        
        if (task_message.task_id < 0 || executable_data.empty()) {
            break;
        }

        int taskID = task_message.task_id;
        int subtaskID = task_message.subtask_id;
        int mod = task_message.mod;
        int r = task_message.r;
        int executable_size = task_message.executable_size;

        if (executable_size <= 0) {
            cout << "[Worker] Error: Invalid executable size" << endl;
            continue;
        }

        // Create executables directory if it doesn't exist
        struct stat info;
        if (stat("executables", &info) != 0) {
            mkdir("executables", 0755);
        }

        // Generate unique executable filename
        string executable_path = "executables/exec_" + to_string(taskID) + "_" + to_string(subtaskID) + "_" + to_string(time(nullptr));

        // Save executable to file
        ofstream exec_file(executable_path, ios::binary);
        if (!exec_file) {
            cout << "[Worker] Error: Cannot create executable file" << endl;
            continue;
        }
        exec_file.write(executable_data.data(), executable_size);
        exec_file.close();

        // Make executable
        chmod(executable_path.c_str(), 0755);

        Result result;
        result.task_id = taskID;
        result.subtask_id = subtaskID;
        result.is_accepted = true;
        result.maximum_time = 0;
        result.maximum_memory = 0;
        result.test_output.clear();
        early_termination = false;

        cout<<"[Worker] Task "<<taskID<<" subtask "<<subtaskID<<" has "<<taskData[taskID].subtask[subtaskID].testcase.size()<<" testcases"<<endl;

        for (int i=0; i<(int)taskData[taskID].subtask[subtaskID].testcase.size(); ++i) {
            cout<<"[Worker] Processing testcase "<<i<<" for task "<<taskID<<" subtask "<<subtaskID<<endl;
            
            if (i%mod!=r) continue;

            if (early_termination) {
                result.is_accepted=false;
                break;
            }
            
            Testcase testcase=taskData[taskID].subtask[subtaskID].testcase[i];
            Output output=execute_code(testcase, taskData[taskID].time_limit, taskData[taskID].memory_limit, executable_path);

            result.test_output.push_back(output);
            result.maximum_time=max((long)result.maximum_time, output.time_usage);
            result.maximum_memory=max((long)result.maximum_memory, output.memory_usage);

            if (output.verdict!="correct") {
                result.is_accepted=false;
                break;
            }
        }

        // Clean up executable file
        unlink(executable_path.c_str());

        sendResult(result, data_socket);
        cout<<"[Worker] Sent result to distributor"<<endl;
    }
}