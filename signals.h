#ifndef SIGNALS_H
#define SIGNALS_H

#include "utils.h"
#include <signal.h>

// SIGUSR1 = Emergency alert signal
// SIGUSR2 = Deadlock detected signal

void setup_signal_handlers();
void signal_all_crew(int signum, const char *message);
void write_signal_pid(const char *username);
void cleanup_signal_pid(const char *username);

#endif
