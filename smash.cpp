#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include "Commands.h"
#include "signals.h"



int main(int argc, char* argv[]) {

    //TODO: setup sig alarm handler

    SmallShell& smash = SmallShell::getInstance();
    struct sigaction act;
    act.sa_handler = alarmHandler;
    act.sa_flags = SA_RESTART;
    if(sigaction(SIGALRM , &act, nullptr) == -1) {
        perror("smash error: failed to set SIGALRM handler");
    }
    if(signal(SIGINT , ctrlCHandler) == SIG_ERR) {
        perror("smash error: failed to set ctrl-C handler");
    }
    if(signal(SIGTSTP , ctrlZHandler) == SIG_ERR) {
        perror("smash error: failed to set ctrl-Z handler");
    }
    while(smash.IsRunning()) {
        smash.UpdatePid(0);
        std::cout << smash.line();
        std::string cmd_line;
        std::getline(std::cin, cmd_line);
        smash.executeCommand(cmd_line.c_str());
    }
    return 0;
}