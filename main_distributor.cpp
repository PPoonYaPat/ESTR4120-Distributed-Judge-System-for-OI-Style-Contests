#include <bits/stdc++.h>
#include "src/distributor/distributor.h"
#include <netinet/in.h>
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
        //{htonl(INADDR_LOOPBACK), 8888},
    };

    Distributor distributor(STDOUT_FILENO, machine_addresses);
    // distributor.send_testcase("test1.json");
    distributor.init_task("test1.json");
    distributor.start();

    sleep(5);

    distributor.add_submission(SubmissionInfo{1, 1, 0, "../temp"});

    sleep(5);
    distributor.shutdown();

}