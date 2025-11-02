#include <bits/stdc++.h>
#include "src/worker/worker.h"
using namespace std;

int main(int argc, char* argv[]) {
    assert(argc==2);
    int listening_port=atoi(argv[1]);

    Worker worker("../example_config.json", listening_port);
    worker.test_receive_exe();
}