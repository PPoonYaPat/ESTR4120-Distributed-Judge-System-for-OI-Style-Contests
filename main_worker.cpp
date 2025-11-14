#include <bits/stdc++.h>
#include "src/worker/worker.h"
using namespace std;

int main(int argc, char* argv[]) {
    assert(argc==2 || argc==3);
    int listening_port=atoi(argv[1]);

    bool download_testcases = false;
    if (argc==3 && string(argv[2])=="-tc") download_testcases = true;

    Worker worker(listening_port);
    worker.init_task(download_testcases);
    worker.start();
}