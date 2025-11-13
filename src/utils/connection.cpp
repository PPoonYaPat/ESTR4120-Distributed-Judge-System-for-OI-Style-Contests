#include "connection.h"
#include "base_connection.h"
#include <fstream>
#include <iostream>
#include <cstdlib>

using namespace std;

// ============================================================================
// Task Message Protocol
// ============================================================================

void sendTaskMessage(const TaskMessage& task_message, string executable_path, int socket) {
    send_int(socket, task_message.submission_id);
    send_int(socket, task_message.task_id);
    send_int(socket, task_message.subtask_id);
    send_int(socket, task_message.mod);
    send_int(socket, task_message.r);

    if (recv_int(socket) == 0) return;

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
    }

    send_int(socket, executable_data.size());
    if (!executable_data.empty()) {
        send_all(socket, executable_data.data(), executable_data.size());
    }
}

pair<TaskMessage, vector<char>> receiveTaskMessage(int socket, vector<int>& cache_submission_ids) {
    TaskMessage msg;
    vector<char> executable_data;
    
    try {
        msg.submission_id = recv_int(socket);
        msg.task_id = recv_int(socket);
        msg.subtask_id = recv_int(socket);
        msg.mod = recv_int(socket);
        msg.r = recv_int(socket);

        auto it = find(cache_submission_ids.begin(), cache_submission_ids.end(), msg.submission_id);
        if (it != cache_submission_ids.end()) { // LRU: mark as most recently used
            cache_submission_ids.erase(it);
            cache_submission_ids.push_back(msg.submission_id);
            msg.executable_size = -1;
            send_int(socket, 0);
            return make_pair(msg, executable_data);
        }

        send_int(socket, 1);
        msg.executable_size = recv_int(socket);
        
        if (msg.executable_size > 0) {
            executable_data.resize(msg.executable_size);
            recv_all(socket, executable_data.data(), msg.executable_size);
        }
    } catch (const runtime_error& e) {
        cerr << "[receiveTaskMessage] Error: " << e.what() << endl;
        msg.task_id = -1;
        executable_data.clear();
    }
    
    return make_pair(msg, executable_data);
}

// ============================================================================
// Result Protocol
// ============================================================================

void sendResult(const Result& result, int socket) {
    cout << "[sendResult] Sending result: task_id=" << result.task_id << " subtask_id=" << result.subtask_id << " num_outputs=" << result.test_output.size() << endl;
    
    send_int(socket, result.task_id);
    send_int(socket, result.subtask_id);
    send_int(socket, result.maximum_time);
    send_int(socket, result.maximum_memory);
    send_int(socket, result.is_accepted ? 1 : 0);
    send_int(socket, result.test_output.size());
    
    // Send each Output
    cout << "[sendResult] Sending " << result.test_output.size() << " outputs" << endl;
    int idx=0;
    for (const auto& output : result.test_output) {
        send_int(socket, output.time_usage);
        send_int(socket, output.memory_usage);
        send_string(socket, output.verdict);
        send_string(socket, output.user_output);
        
        cout << "[sendResult] Sent output " << idx << ": verdict='" << output.verdict << "' (" << output.verdict.size() << " bytes)" << endl;
        idx++;
    }
    
    cout << "[sendResult] Complete result sent!" << endl;
    cout.flush();
}

Result receiveResult(int socket) {
    Result result;
    
    try {
        result.task_id = recv_int(socket);
        result.subtask_id = recv_int(socket);
        result.maximum_time = recv_int(socket);
        result.maximum_memory = recv_int(socket);
        result.is_accepted = (recv_int(socket) == 1);
        int count = recv_int(socket);
        
        cout << "[receiveResult] Receiving result: task_id=" << result.task_id << " subtask_id=" << result.subtask_id << " num_outputs=" << count << endl;
        cout.flush();
        
        // Receive each Output
        for (int i = 0; i < count; i++) {
            
            Output output;
            output.time_usage = recv_int(socket);
            output.memory_usage = recv_int(socket);
            output.verdict = recv_string(socket);
            output.user_output = recv_string(socket);
            
            cout << "[receiveResult] Received output " << i << ": verdict='" << output.verdict << "'" << endl;
            cout.flush();
            
            result.test_output.push_back(output);
        }
        
        cout << "[receiveResult] Successfully received complete result from task " << result.task_id << " subtask " << result.subtask_id << endl;
        cout.flush();
        
    } catch (const runtime_error& e) {
        cerr << "[receiveResult] Error: " << e.what() << endl;
        throw;
    }
    
    return result;
}

// ============================================================================
// File Transfer Protocol
// ============================================================================

void send_file(int socket, const string& input_path, const string& output_path) {
    // Open the file to send
    ifstream input_file(input_path, ios::binary | ios::ate);
    if (!input_file) {
        cerr << "[send_file] Error: Cannot open file: " << input_path << endl;
        // Send empty path as error indicator
        send_string(socket, "");
        return;
    }
    
    // Get file size
    size_t file_size = input_file.tellg();
    input_file.seekg(0, ios::beg);
    
    // Read file data
    vector<char> file_data(file_size);
    input_file.read(file_data.data(), file_size);
    input_file.close();
    
    // Send output_path
    send_string(socket, output_path);
    
    // Send file data
    send_binary(socket, file_data);
    
    cerr << "[send_file] Sent " << file_size << " bytes from " << input_path << " -> " << output_path << endl;
}

void receive_file(int socket) {
    try {
        // Receive output_path
        string output_path = recv_string(socket);
        
        // Check for error indicator (empty path)
        if (output_path.empty()) {
            cerr << "[receive_file] Error: Sender failed to open file" << endl;
            return;
        }
        
        // Receive file data
        vector<char> file_data = recv_binary(socket);
        
        // Create directory if needed
        size_t last_slash = output_path.find_last_of('/');
        if (last_slash != string::npos) {
            string dir = output_path.substr(0, last_slash);
            int result = system(("mkdir -p " + dir).c_str());
            if (result != 0) {
                cerr << "[receive_file] Error: Failed to create directory: " << dir << endl;
                return;
            }
        }
        
        // Write file to disk
        ofstream output_file(output_path, ios::binary);
        if (!output_file) {
            cerr << "[receive_file] Error: Cannot create file: " << output_path << endl;
            return;
        }
        
        output_file.write(file_data.data(), file_data.size());
        output_file.close();
        
        cerr << "[receive_file] Received " << file_data.size() << " bytes -> " << output_path << endl;
             
    } catch (const runtime_error& e) {
        cerr << "[receive_file] Error: " << e.what() << endl;
    }
}

// ============================================================================
// Task Details Protocol
// ============================================================================

void send_task_details(int socket, vector<Task>& taskData) {
    send_int(socket, taskData.size());
    
    for (const auto& task : taskData) {
        send_int(socket, task.task_id);
        send_int(socket, task.memory_limit);
        send_int(socket, task.time_limit);
        send_int(socket, task.subtask.size());
        
        for (const auto& subtask : task.subtask) {
            send_int(socket, subtask.task_id);
            send_int(socket, subtask.subtask_id);
            send_int(socket, subtask.mod);
            
            send_int(socket, subtask.dependencies.size());
            for (int dep : subtask.dependencies) {
                send_int(socket, dep);
            }
            
            send_int(socket, subtask.testcase.size());
            for (const auto& tc : subtask.testcase) {
                send_string(socket, tc.input_path);
                send_string(socket, tc.expected_output_path);
            }
        }
    }
    
    cout << "[send_task_details] Successfully sent " << taskData.size() << " tasks" << endl;
}

void receive_task_details(int socket, vector<Task>& taskData) {
    try {
        int task_count = recv_int(socket);
        taskData.resize(task_count);
        
        for (int t = 0; t < task_count; t++) {
            Task& task = taskData[t];
            
            task.task_id = recv_int(socket);
            task.memory_limit = recv_int(socket);
            task.time_limit = recv_int(socket);
            
            int subtask_count = recv_int(socket);
            task.subtask.resize(subtask_count);
            
            for (int s = 0; s < subtask_count; s++) {
                Subtask& subtask = task.subtask[s];
                
                subtask.task_id = recv_int(socket);
                subtask.subtask_id = recv_int(socket);
                subtask.mod = recv_int(socket);
                
                int dep_count = recv_int(socket);
                subtask.dependencies.resize(dep_count);
                for (int d = 0; d < dep_count; d++) {
                    subtask.dependencies[d] = recv_int(socket);
                }
                
                int tc_count = recv_int(socket);
                subtask.testcase.resize(tc_count);
                for (int tc = 0; tc < tc_count; tc++) {
                    subtask.testcase[tc].input_path = recv_string(socket);
                    subtask.testcase[tc].expected_output_path = recv_string(socket);
                }
            }
        }
        
        cout << "[receive_task_details] Successfully received " << task_count << " tasks" << endl;
        
    } catch (const runtime_error& e) {
        cerr << "[receive_task_details] Error: " << e.what() << endl;
        throw;
    }
}