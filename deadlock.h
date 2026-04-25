#ifndef DEADLOCK_H
#define DEADLOCK_H

#include "utils.h"
#include "filestore.h"

// Allocation matrix: allocation[i][j] = amount of resource j held by process i
// For RigGuard: each user holds 0 or 1 of each resource

#define MAX_PROC 10

typedef struct {
    char name[MAX_STR];
    int  allocation[NUM_RESOURCES];  // what they currently hold
    int  request[NUM_RESOURCES];     // what they are waiting for
} ProcessState;

int  run_bankers(ProcessState procs[], int n, int available[], int safe_seq[]);
int  detect_deadlock_cycle(ProcessState procs[], int n, char *cycle_out);
void suggest_recovery(ProcessState procs[], int n, const char *cycle);
void run_deadlock_check(const char *current_user);

#endif
