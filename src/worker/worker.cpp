#include "Worker.h"
#include "../utils/connection.h"
#include "../common/common.h"
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <nlohmann/json.hpp>
#include <sys/wait.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>
#include <bits/stdc++.h>

using json = nlohmann::json;
using namespace std;

Worker::Worker(string testcase_config_path, int listening_port) : early_termination(false) {
    init_task(testcase_config_path);

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

void Worker::start() {
        thread control_thread(&Worker::controlMessageListener, this);
        thread process_thread(&Worker::processTasksLoop, this);

        control_thread.join();
        process_thread.join();
    }

void Worker::init_task(string testcase_config_path) {
        ifstream file(testcase_config_path);
        
        if (!file.is_open()) {
            cerr << "Error: Cannot open testcase.json" << endl;
            exit(1);
        }

        json config;
        file >> config;
        file.close();

        taskData.clear();
        for (auto& task_json : config["tasks"]) {
            Task task;
            task.task_id = task_json["task_id"];
            task.memory_limit = task_json["memory_limit"];
            task.time_limit = task_json["time_limit"];

            for (auto& subtask_json : task_json["subtasks"]) {
                Subtask subtask;
                subtask.task_id = task_json["task_id"];
                subtask.subtask_id = subtask_json["subtask_id"];

                for (auto& testcase_json : subtask_json["testcases"]) {
                    Testcase testcase;
                    testcase.input_path = testcase_json["input_path"];
                    testcase.expected_output_path = testcase_json["expected_output_path"];
                    testcase.output_path = testcase_json["output_path"];
                    subtask.testcase.push_back(testcase);
                }

                task.subtask.push_back(subtask);
            }

            taskData.push_back(task);
        }
    }

void Worker::controlMessageListener() {
        while (true) {
            char buffer[1024];
            int bytes = recv(control_socket, buffer, 1024, 0);
            if (bytes<=0) break;

            cout<<"[Control Thread] Received early termination signal"<<endl;
            early_termination = true;
            break;
        }
    }

void Worker::test_receive_exe() {
    auto [task_message, executable_data] = receiveTaskMessage(data_socket);

    if (task_message.executable_size <= 0) {
        cerr << "Error: Invalid executable size" << endl;
        return;
    }

    // Create executables directory if it doesn't exist
    struct stat info;
    if (stat("executables", &info) != 0) {
        mkdir("executables", 0755);
    }

    // Generate unique executable filename
    string executable_path = "executables/exec_" + to_string(time(nullptr));

    // Save executable to file
    ofstream exec_file(executable_path, ios::binary);
    if (!exec_file) {
        cerr << "Error: Cannot create executable file" << endl;
        return;
    }
    exec_file.write(executable_data.data(), task_message.executable_size);
    exec_file.close();

    // Make executable
    chmod(executable_path.c_str(), 0755);
    cout << "Received executable from distributor" << endl;
}

void Worker::processTasksLoop() {
    while (true) {
        auto [task_message, executable_data] = receiveTaskMessage(data_socket);
        
        if (task_message.task_id < 0 || executable_data.empty()) {
            break;
        }

        int taskID = task_message.task_id;
        int subtaskID = task_message.subtask_id;
        int mod = task_message.mod;
        int r = task_message.r;
        int executable_size = task_message.executable_size;

        if (executable_size <= 0) {
            cerr << "Error: Invalid executable size" << endl;
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
            cerr << "Error: Cannot create executable file" << endl;
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

        for (int i=0; i<(int)taskData[taskID].subtask[subtaskID].testcase.size(); ++i) {
            
            if (i%mod!=r) continue;

            if (early_termination) {
                result.is_accepted=false;
                break;
            }
            
            Testcase testcase=taskData[taskID].subtask[subtaskID].testcase[i];
            Output output=execute_code(testcase, taskData[taskID].time_limit, taskData[taskID].memory_limit, executable_path);

            result.test_output.push_back(output);
            result.maximum_time=max(result.maximum_time, output.time_usage);
            result.maximum_memory=max(result.maximum_memory, output.memory_usage);

            if (output.verdict!="correct") {
                result.is_accepted=false;
                break;
            }
        }

        // Clean up executable file
        unlink(executable_path.c_str());

        sendResult(result, data_socket);
    }
}

Output Worker::execute_code(Testcase testcase, int time_limit, int memory_limit, string executable_path) {
    Output output;
    output.time_usage = 0;
    output.memory_usage = 0;
    output.verdict = "RE";
    output.user_output = "";

    // Ensure directory exists
    mkdir("executables", 0755);

    string temp_output = "executables/temp_output_" + 
                        to_string(getpid()) + "_" + 
                        to_string(time(nullptr)) + ".txt";
    
    pid_t pid = fork();
    if (pid == 0) {
        // CHILD: Redirect I/O
        int input_fd = open(testcase.input_path.c_str(), O_RDONLY);
        if (input_fd < 0) _exit(1);
        dup2(input_fd, STDIN_FILENO);
        close(input_fd);

        int output_fd = open(temp_output.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (output_fd < 0) _exit(1);
        dup2(output_fd, STDOUT_FILENO);
        dup2(output_fd, STDERR_FILENO);
        close(output_fd);

        // Set limits
        struct rlimit rl;
        rl.rlim_cur = rl.rlim_max = (time_limit / 1000) + 2;
        setrlimit(RLIMIT_CPU, &rl);
        
        rl.rlim_cur = rl.rlim_max = (long long)memory_limit * 1024 * 1024;
        setrlimit(RLIMIT_AS, &rl);

        // Disable core dumps
        rl.rlim_cur = rl.rlim_max = 0;
        setrlimit(RLIMIT_CORE, &rl);

        execl(executable_path.c_str(), executable_path.c_str(), (char*)NULL);
        _exit(1);
        
    } else if (pid > 0) {
        // PARENT: Monitor
        struct rusage usage;
        int status;
        long timeout_ms = time_limit * 3;
        
        struct timespec start_time, current_time;
        clock_gettime(CLOCK_MONOTONIC, &start_time);
        
        while (true) {
            pid_t result = wait4(pid, &status, WNOHANG, &usage);
            
            if (result == pid) {
                break;
            } else if (result < 0) {
                output.verdict = "RE";
                unlink(temp_output.c_str());
                return output;
            }
            
            // Check timeout
            clock_gettime(CLOCK_MONOTONIC, &current_time);
            long elapsed_ms = ((current_time.tv_sec - start_time.tv_sec) * 1000) +
                             ((current_time.tv_nsec - start_time.tv_nsec) / 1000000);
            
            if (elapsed_ms >= timeout_ms) {
                kill(pid, SIGKILL);
                waitpid(pid, &status, 0);
                output.verdict = "TLE";
                output.time_usage = timeout_ms;
                unlink(temp_output.c_str());
                return output;
            }
            
            usleep(5000);
        }
        
        // Calculate CPU time in milliseconds
        output.time_usage = (usage.ru_utime.tv_sec * 1000) + 
                           (usage.ru_utime.tv_usec / 1000);
        
        output.memory_usage = usage.ru_maxrss / 1024;

        // Check signals
        if (WIFSIGNALED(status)) {
            int sig = WTERMSIG(status);
            if (sig == SIGXCPU) {
                output.verdict = "TLE";
            } else if (sig == SIGKILL || sig == SIGSEGV || sig == SIGABRT) {
                output.verdict = "MLE";
            } else {
                output.verdict = "RE";
            }
            unlink(temp_output.c_str());
            return output;
        }
        
        // Check exit code
        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
            output.verdict = "RE";
            unlink(temp_output.c_str());
            return output;
        }

        // Check limits
        if (output.time_usage > time_limit) {
            output.verdict = "TLE";
            unlink(temp_output.c_str());
            return output;
        }

        if (output.memory_usage > memory_limit) {
            output.verdict = "MLE";
            unlink(temp_output.c_str());
            return output;
        }

        // Read and compare output
        ifstream user_file(temp_output);
        if (!user_file) {
            output.verdict = "RE";
            unlink(temp_output.c_str());
            return output;
        }
        
        stringstream user_ss;
        user_ss << user_file.rdbuf();
        output.user_output = user_ss.str();
        user_file.close();

        ifstream expected_file(testcase.expected_output_path);
        if (!expected_file) {
            output.verdict = "RE";
            unlink(temp_output.c_str());
            return output;
        }

        stringstream expected_ss;
        expected_ss << expected_file.rdbuf();
        string expected_output = expected_ss.str();
        expected_file.close();

        // Trim trailing whitespace
        auto trim = [](string& s) {
            while (!s.empty() && isspace(s.back())) s.pop_back();
            while (!s.empty() && isspace(s.front())) s.erase(0, 1);
        };
        
        string user_trimmed = output.user_output;
        string expected_trimmed = expected_output;
        trim(user_trimmed);
        trim(expected_trimmed);

        output.verdict = (user_trimmed == expected_trimmed) ? "correct" : "WA";
        unlink(temp_output.c_str());
        return output;
        
    } else {
        output.verdict = "RE";
        return output;
    }
}