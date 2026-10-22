#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include "Commands.h"
#include "signals.h"

int main(int argc, char *argv[]) {
    if (signal(SIGINT, ctrlCHandler) == SIG_ERR) {
        perror("smash error: failed to set ctrl-C handler");
    }


    SmallShell &smash = SmallShell::getInstance();
    smash.createCommandVector();
    while (true) {
        std::cout << SmallShell::getInstance().getPrompt() << "> ";

        std::string cmd_line;
        if (!std::getline(std::cin, cmd_line)) {
            if (std::cin.eof()) {
                break; // Ctrl+D
            }
            std::cin.clear();
            std::cout << std::endl;
            continue;
        }
        smash.executeCommand(cmd_line.c_str());
    }
    return 0;
}