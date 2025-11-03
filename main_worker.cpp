#include <bits/stdc++.h>
#include "src/worker/worker.h"
using namespace std;

int main(int argc, char* argv[]) {
    assert(argc==2);
    int listening_port=atoi(argv[1]);

    Worker worker(listening_port);
    worker.receive_testcase();
}