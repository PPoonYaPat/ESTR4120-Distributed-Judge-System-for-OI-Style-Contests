#include <bits/stdc++.h>
#include "src/distributor/distributor.h"
#include <netinet/in.h>
#include <fcntl.h> 
#include <unistd.h>
using namespace std;

/*
struct SubmissionInfo {
    int user_id;
    int submission_id;
    int task_id;
    string executable_path;
};
*/

int main() {
    vector<MachineAddress> machine_addresses = {
        {htonl(INADDR_LOOPBACK), 9999},
        {htonl(INADDR_LOOPBACK), 8888},
        {htonl(INADDR_LOOPBACK), 7777},
    };

    int output_fd = open("output.txt", O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (output_fd == -1) {
        cerr << "Error: Failed to open output file" << endl;
        exit(1);
    }

    Distributor distributor(output_fd, machine_addresses);
    distributor.init_task_auto_detect("testcase_config2.json", {"testcase"}, false);
    distributor.start();

    //distributor.add_submission(SubmissionInfo{11, 105, 0, "sol/bruteforce"});
    distributor.add_submission(SubmissionInfo{11, 207, 0, "sol/sol"});

    distributor.shutdown();

}