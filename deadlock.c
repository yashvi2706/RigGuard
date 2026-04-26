#include "deadlock.h"

// ─── Banker's Safety Algorithm ────────────────────────────────────────────────
int run_bankers(ProcessState procs[], int n, int available[], int safe_seq[]) {
    int work[num_resources];
    int finish[MAX_PROC] = {0};
    int seq_idx = 0;

    for (int j = 0; j < num_resources; j++)
        work[j] = available[j];

    printf(CYAN "\n[SYSTEM] 🔍 DEADLOCK CHECK    — Running Banker's Safety Algorithm...\n" RESET);
    printf(CYAN "[SYSTEM] 📋 AVAILABLE         — Resources: " RESET);
    for (int j = 0; j < num_resources; j++)
        printf("res[%d]=%d ", j, work[j]);
    printf("\n");

    int progress = 1;
    while (progress) {
        progress = 0;
        for (int i = 0; i < n; i++) {
            if (finish[i]) continue;
            int can_proceed = 1;
            for (int j = 0; j < num_resources; j++) {
                if (procs[i].request[j] > work[j]) { can_proceed = 0; break; }
            }
            if (can_proceed) {
                printf(GREEN "[SYSTEM] ✅ SAFE STEP         — %s can proceed\n" RESET, procs[i].name);
                for (int j = 0; j < num_resources; j++)
                    work[j] += procs[i].allocation[j];
                finish[i] = 1;
                safe_seq[seq_idx++] = i;
                progress = 1;
            }
        }
    }

    for (int i = 0; i < n; i++)
        if (!finish[i]) return 0;
    return 1;
}

// ─── Cycle Detection using wait-for graph ────────────────────────────────────
int detect_deadlock_cycle(ProcessState procs[], int n, char *cycle_out) {
    // Build wait-for graph from request matrix
    // procs[i] waits for procs[j] if procs[i] requests something procs[j] holds
    int wait_for[MAX_PROC][MAX_PROC];
    memset(wait_for, 0, sizeof(wait_for));

    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (i == j) continue;
            for (int k = 0; k < num_resources; k++) {
                if (procs[i].request[k] > 0 && procs[j].allocation[k] > 0) {
                    wait_for[i][j] = 1;
                }
            }
        }
    }

    // Print wait-for graph
    printf(CYAN "\n[SYSTEM] 🔗 WAIT-FOR GRAPH    —\n" RESET);
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (wait_for[i][j])
                printf(YELLOW "         %s → waits for → %s\n" RESET, procs[i].name, procs[j].name);
        }
    }

    // DFS cycle detection
    int color[MAX_PROC] = {0}; // 0=white, 1=gray, 2=black
    int parent[MAX_PROC];
    memset(parent, -1, sizeof(parent));

    for (int start = 0; start < n; start++) {
        if (color[start] != 0) continue;

        // Iterative DFS
        int stack[MAX_PROC], sp = 0;
        int path[MAX_PROC], path_len = 0;
        stack[sp++] = start;

        while (sp > 0) {
            int node = stack[sp-1];

            if (color[node] == 0) {
                color[node] = 1; // gray = in progress
                path[path_len++] = node;
            }

            int pushed = 0;
            for (int next = 0; next < n; next++) {
                if (!wait_for[node][next]) continue;
                if (color[next] == 0) {
                    parent[next] = node;
                    stack[sp++] = next;
                    pushed = 1;
                    break;
                } else if (color[next] == 1) {
                    // Back edge found — cycle!
                    char tmp[MAX_MSG] = "";
                    // Build cycle string from next to current node
                    int ci = 0;
                    while (ci < path_len && path[ci] != next) ci++;
                    for (int x = ci; x < path_len; x++) {
                        strcat(tmp, procs[path[x]].name);
                        strcat(tmp, " → ");
                    }
                    strcat(tmp, procs[next].name);
                    strncpy(cycle_out, tmp, MAX_MSG);
                    return 1;
                }
            }

            if (!pushed) {
                color[node] = 2; // black = done
                sp--;
                if (path_len > 0) path_len--;
            }
        }
    }
    return 0;
}

// ─── Recovery Suggestion ──────────────────────────────────────────────────────
void suggest_recovery(ProcessState procs[], int n, const char *cycle) {
    printf(RED "\n[SYSTEM] 🔴 DEADLOCK DETECTED — Cycle: %s\n" RESET, cycle);
    printf(RED "[SYSTEM] 🚨 EMERGENCY ALERT   — Broadcasting to all crew!\n" RESET);

    // Find process with most held resources as rollback candidate (skip commander)
    int max_held = -1, rollback_idx = 0;
    for (int i = 0; i < n; i++) {
        int held = 0;
        for (int j = 0; j < num_resources; j++)
            held += procs[i].allocation[j];
        if (held > max_held && strcmp(procs[i].name, "commander") != 0) {
            max_held = held;
            rollback_idx = i;
        }
    }

    printf(YELLOW "[SYSTEM] 🛠️  RECOVERY        — Rolling back: %s\n" RESET, procs[rollback_idx].name);
    printf(YELLOW "[SYSTEM] 📢 SEMAPHORE POST   — Signaling ALL blocked threads to wake up\n" RESET);

    // Free ALL resources held by the rolled-back process
    // AND signal ALL semaphores that have waiting processes
    for (int j = 0; j < num_resources; j++) {
        if (procs[rollback_idx].allocation[j] > 0) {
            printf(GREEN "[SYSTEM] 🔓 RESOURCE FREED   — resource[%d] released by %s\n" RESET,
                   j, procs[rollback_idx].name);
            sem_post(resource_sem[j]);
        }
        if (procs[rollback_idx].request[j] > 0) {
            printf(GREEN "[SYSTEM] 📢 UNBLOCKING       — Signaling resource[%d] semaphore\n" RESET, j);
            sem_post(resource_sem[j]);
        }
    }

    // Signal ALL semaphores that have waiters to break full cycle
    printf(YELLOW "[SYSTEM] 🔓 BREAKING CYCLE   — Posting all semaphores to unblock deadlocked threads\n" RESET);
    for (int j = 0; j < num_resources; j++) {
        int waiting = 0;
        for (int i = 0; i < n; i++) {
            if (procs[i].request[j] > 0) { waiting = 1; break; }
        }
        if (waiting) {
            sem_post(resource_sem[j]);
            printf(GREEN "[SYSTEM] 📢 SEMAPHORE POST   — resource[%d] semaphore signaled\n" RESET, j);
        }
    }

    char detail[MAX_MSG];
    snprintf(detail, sizeof(detail), "Cycle=%s Rollback=%s", cycle, procs[rollback_idx].name);
    log_incident("SYSTEM", "DEADLOCK_RECOVERY", detail);
}

// ─── Main deadlock check entry point ─────────────────────────────────────────
void run_deadlock_check(const char *current_user) {
    Resource res[MAX_RESOURCES];
    int rcount = 0;
    load_resources(res, &rcount);

    User users[MAX_USERS];
    int ucount = 0;
    load_users(users, &ucount);

    // Build process states
    ProcessState procs[MAX_PROC];
    int n = 0;
    int available[MAX_RESOURCES];
    memset(available, 0, sizeof(available));

    for (int i = 0; i < ucount && n < MAX_PROC; i++) {
        strncpy(procs[n].name, users[i].username, MAX_STR);
        memset(procs[n].allocation, 0, sizeof(procs[n].allocation));
        memset(procs[n].request, 0, sizeof(procs[n].request));
        n++;
    }

    // Fill allocation and request from resources.csv
    // allocation[i][j] = process i holds resource j
    // request[i][j]    = process i is WAITING for resource j
    for (int r = 0; r < rcount; r++) {
        // Find resource index
        int res_j = -1;
        for (int j = 0; j < num_resources; j++) {
            if (strcmp(res[j].resource, res[r].resource) == 0) { res_j = j; break; }
        }
        if (res_j < 0) continue;

        if (strcmp(res[r].held_by, "none") == 0) {
            available[res_j] = 1;
        } else {
            // Find process holding this resource
            for (int i = 0; i < n; i++) {
                if (strcmp(procs[i].name, res[r].held_by) == 0) {
                    procs[i].allocation[res_j] = 1;
                    break;
                }
            }
        }

        // *** KEY: check waited_by — who is blocked on this resource ***
        if (strcmp(res[r].waited_by, "none") != 0) {
            for (int i = 0; i < n; i++) {
                if (strcmp(procs[i].name, res[r].waited_by) == 0) {
                    procs[i].request[res_j] = 1; // process i is waiting for resource j
                    printf(YELLOW "[SYSTEM] 📋 WAIT DETECTED     — %s is waiting for %s (held by %s)\n" RESET,
                           res[r].waited_by, res[r].resource, res[r].held_by);
                    break;
                }
            }
        }
    }

    // Print allocation table
    print_separator();
    printf(CYAN "📊 RESOURCE ALLOCATION TABLE\n" RESET);
    print_separator();
    printf(WHITE "%-20s", "Resource");
    for (int i = 0; i < n; i++) printf("%-12s", procs[i].name);
    printf("\n");
    print_separator();
    for (int j = 0; j < num_resources; j++) {
        printf(YELLOW "%-20s" RESET, res[j].resource);
        for (int i = 0; i < n; i++) {
            if (procs[i].allocation[j])
                printf(RED "%-12s" RESET, "HELD");
            else if (procs[i].request[j])
                printf(YELLOW "%-12s" RESET, "WAITING");
            else
                printf("%-12s", "free");
        }
        printf("\n");
    }
    print_separator();

    // First check for cycles (actual deadlock)
    char cycle[MAX_MSG] = "";
    int has_cycle = detect_deadlock_cycle(procs, n, cycle);

    if (has_cycle) {
        suggest_recovery(procs, n, cycle);
    } else {
        // Run Banker's for safe state check
        int safe_seq[MAX_PROC];
        int is_safe = run_bankers(procs, n, available, safe_seq);

        if (is_safe) {
            printf(GREEN "\n[SYSTEM] ✅ SAFE STATE       — No deadlock. Safe sequence: " RESET);
            for (int i = 0; i < n; i++)
                printf("%s%s", procs[safe_seq[i]].name, i < n-1 ? " → " : "\n");
            printf(GREEN "[SYSTEM] 🛢️  RIG STABLE       — All systems operational!\n" RESET);
            log_incident(current_user, "DEADLOCK_CHECK", "SAFE - No deadlock");
        } else {
            printf(RED "\n[SYSTEM] ⚠️  UNSAFE STATE    — Potential deadlock forming!\n" RESET);
            log_incident(current_user, "DEADLOCK_CHECK", "UNSAFE STATE");
        }
    }
}
