#include <bits/stdc++.h>
#include "src/distributor/distributor.h"
#include <netinet/in.h>
#include <unistd.h>
using namespace std;

int main() {
    vector<MachineAddress> machine_addresses = {
        {htonl(INADDR_LOOPBACK), 9999},
        {htonl(INADDR_LOOPBACK), 8888},
    };

    Distributor distributor(STDOUT_FILENO, machine_addresses);
    distributor.test_send_exe("temp");
    
}