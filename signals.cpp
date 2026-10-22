#include <iostream>
#include <signal.h>
#include "signals.h"
#include "Commands.h"

using namespace std;

void ctrlCHandler(int sig_num) {
    const char msg[] = "smash: got ctrl-C\n";
    write(STDOUT_FILENO, msg, sizeof(msg) - 1);
    SmallShell::getInstance().smashControlCSigHandler();
}
