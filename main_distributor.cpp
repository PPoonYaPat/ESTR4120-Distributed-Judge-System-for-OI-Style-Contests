#include <bits/stdc++.h>
#include "src/distributor/distributor.h"
#include <netinet/in.h>
#include <fcntl.h> 
#include <unistd.h>
#include <arpa/inet.h>
using namespace std;

int main() {
    // Step 1: List all worker machine addresses and ports
    vector<MachineAddress> machine_addresses = {
        {inet_addr("34.92.249.211"), 8500},
        {inet_addr("34.96.166.228"), 8500},
        // Add more workers as needed
    };
    
    // Step 2: Open log file descriptor for evaluation results
    int output_fd = open("log.txt", O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (output_fd == -1) {
        cerr << "Error: Failed to open log file" << endl;
        return 1;
    }
    
    // Step 3: Initialize distributor
    Distributor distributor(output_fd, machine_addresses);
    
    // Step 4: Initialize task configuration
    // Parameters:
    //   - "testcase_config.json": Configuration file path
    //   - {"testcase"}: Array of testcase folder paths (one per problem) e.g. {"testcase_problem1", "testcase_problem2", "testcase_problem3"}
    //   - false: Whether to send testcases to workers
    //       - Recommended to set to `true` on first run to transfer testcase files to all workers. 
    //       - Set to `false` for subsequent runs when testcases are already stored on worker nodes.
    distributor.init_task_auto_detect("testcase_config.json", {"testcase"}, false);

    // Step 5: Start distributor (establishes worker connections)
    distributor.start();
    
    // Step 6: Add submissions for evaluation
    // SubmissionInfo format: {user_id, submission_id, task_id, executable_path}
    distributor.add_submission(SubmissionInfo{12, 207, 0, "sol/sol"});
    distributor.add_submission(SubmissionInfo{11, 105, 0, "sol/re"});
    distributor.add_submission(SubmissionInfo{13, 305, 0, "sol/sol2"});
    distributor.add_submission(SubmissionInfo{14, 404, 0, "sol/tle"});
    // Add more submissions as needed
    
    // Step 7: Shutdown gracefully (waits for all evaluations to complete)
    distributor.shutdown();
}