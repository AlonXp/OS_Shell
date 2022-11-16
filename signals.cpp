#include <iostream>
#include <signal.h>
#include "signals.h"
#include "Commands.h"

using namespace std;
void ctrlZHandler(int sig_num) {
    SmallShell &shl = SmallShell::getInstance();
    std::cout << "smash: got ctrl-Z";
    std::cout << "" << std::endl;
    int i = shl.GetPid();
    if (i > 0) {
        int res;
        DO_SYS(res = kill(i, SIGSTOP));
        std::cout << "smash: process " << i << " was stopped" << std::endl;
    }
}
void ctrlCHandler(int sig_num) {
        SmallShell& shl = SmallShell::getInstance();
        std::cout << "smash: got ctrl-C";
        std::cout << "" << std::endl ;
        int i = shl.GetPid();
        if(i > 0) {
            std::cout << "smash: process " << i <<  " was killed" << std::endl ;
            int res;
            DO_SYS(res = kill(i, SIGKILL));
        }
}

void alarmHandler(int sig_num) {
    std::cout << "smash: got an alarm" << std::endl;
    SmallShell& shl = SmallShell::getInstance();
    std::shared_ptr<JobsList> temp = shl.getTimeOutList();
    temp->alarmFirst(shl.getJobsList());

}

