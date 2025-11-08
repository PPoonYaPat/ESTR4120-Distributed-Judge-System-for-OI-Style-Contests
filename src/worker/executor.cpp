#include "../common/common.h"
#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <bits/stdc++.h>
using namespace std;

// ============================================================================
// Execute code
// ============================================================================

/*
struct Testcase {
    string input_path;
    string expected_output_path;
};
*/

bool outputs_match(const string& user_output, const string& expected_output) {
    auto trim = [](string s) {
        while (!s.empty() && isspace(s.back())) s.pop_back();
        while (!s.empty() && isspace(s.front())) s.erase(0, 1);
        return s;
    };
    
    return trim(user_output) == trim(expected_output);
}

bool read_file(const string& filepath, string& content) {
    ifstream file(filepath);
    if (!file) return false;
    
    stringstream buffer;
    buffer << file.rdbuf();
    content = buffer.str();
    file.close();
    return true;
}

string check_process_verdict(int status, const struct rusage& usage, int time_limit, int memory_limit, long& time_usage, long& memory_usage) {
    // Calculate resource usage
    time_usage = (usage.ru_utime.tv_sec * 1000) + (usage.ru_utime.tv_usec / 1000);
    memory_usage = usage.ru_maxrss / 1024;
    
    // Check signals
    if (WIFSIGNALED(status)) {
        int sig = WTERMSIG(status);
        if (sig == SIGXCPU) return "TLE";
        if (sig == SIGKILL || sig == SIGSEGV || sig == SIGABRT) return "MLE";
        return "RE";
    }
    
    // Check exit code
    if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
        return "RE";
    }
    
    // Check limits
    if (time_usage > time_limit) return "TLE";
    if (memory_usage > memory_limit) return "MLE";
    
    return "OK";  // No errors
}

Output execute_code(Testcase testcase, int time_limit, int memory_limit, string executable_path) {
    Output output;
    output.time_usage = 0;
    output.memory_usage = 0;
    output.verdict = "RE";
    output.user_output = "";
    
    mkdir("executables", 0755);
    string temp_output = "executables/temp_output_" + to_string(getpid()) + "_" + to_string(time(nullptr)) + ".txt";
    
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
            
            if (result == pid) break;
            if (result < 0) {
                unlink(temp_output.c_str());
                return output;
            }
            
            // Check timeout
            clock_gettime(CLOCK_MONOTONIC, &current_time);
            long elapsed_ms = ((current_time.tv_sec - start_time.tv_sec) * 1000) + ((current_time.tv_nsec - start_time.tv_nsec) / 1000000);
            
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
        
        // Check process verdict and get resource usage
        output.verdict = check_process_verdict(status, usage, time_limit, memory_limit, output.time_usage, output.memory_usage);
        
        if (output.verdict != "OK") {
            unlink(temp_output.c_str());
            return output;
        }

        // Read outputs and compare
        string expected_output;
        if (!read_file(temp_output, output.user_output) || !read_file(testcase.expected_output_path, expected_output)) {
            output.verdict = "RE";
            unlink(temp_output.c_str());
            return output;
        }

        output.verdict = outputs_match(output.user_output, expected_output) ? "correct" : "WA";
        unlink(temp_output.c_str());
        return output;
        
    } else {
        output.verdict = "RE";
        return output;
    }
}