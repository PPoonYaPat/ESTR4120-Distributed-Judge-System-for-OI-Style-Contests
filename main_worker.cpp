#include <bits/stdc++.h>
#include "src/worker/worker.h"
using namespace std;

int main(int argc, char* argv[]) {
    // Step 1: Initialize worker with listening port
    int listening_port = atoi(argv[1]);
    
    // Step 2: Set testcase download flag
    // - Use true on first run to download all testcases to worker
    // - Use false for subsequent runs if testcases are already cached
    bool download_testcases = (argc==3 && string(argv[2])=="-tc");
    
    Worker worker(listening_port);
    
    // Step 3: Initialize task configuration
    // This prepares the worker to receive and execute tasks
    worker.init_task(download_testcases);
    
    // Step 4: Start worker (begins listening for tasks)
    worker.start();
}