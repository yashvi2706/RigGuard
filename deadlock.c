#include "deadlock.h"
#include "resources.h"

// ─── Banker's Safety Algorithm ────────────────────────────────────────────────
int run_bankers(ProcessState procs[], int n, int available[], int safe_seq[]) {
    int work[NUM_RESOURCES];
    int finish[MAX_PROC] = {0};
    int seq_idx = 0;

    for (int j = 0; j < NUM_RESOURCES; j++)
        work[j] = available[j];

    printf(CYAN "\n[SYSTEM] 🔍 DEADLOCK CHECK    — Running Banker's Safety Algorithm...\n" RESET);
    printf(CYAN "[SYSTEM] 📋 AVAILABLE         — Resources: " RESET);
    for (int j = 0; j < NUM_RESOURCES; j++)
        printf("%s=%d ", RESOURCE_NAMES[j], work[j]);
    printf("\n");

    int progress = 1;
    while (progress) {
        progress = 0;
        for (int i = 0; i < n; i++) {
            if (finish[i]) continue;

            // Check if request <= work (can this process proceed?)
            int can_proceed = 1;
            for (int j = 0; j < NUM_RESOURCES; j++) {
                if (procs[i].request[j] > work[j]) {
                    can_proceed = 0;
                    break;
                }
            }

            if (can_proceed) {
                printf(GREEN "[SYSTEM] ✅ SAFE STEP         — %s can proceed\n" RESET, procs[i].name);
                // Simulate finishing: release their allocation
                for (int j = 0; j < NUM_RESOURCES; j++)
                    work[j] += procs[i].allocation[j];
                finish[i] = 1;
                safe_seq[seq_idx++] = i;
                progress = 1;
            }
        }
    }

    // Check if all finished
    for (int i = 0; i < n; i++) {
        if (!finish[i]) return 0; // UNSAFE
    }
    return 1; // SAFE
}

// ─── Cycle Detection (Resource Allocation Graph) ──────────────────────────────
int detect_deadlock_cycle(ProcessState procs[], int n, char *cycle_out) {
    // Build wait-for graph: proc[i] waits for proc[j] if
    // proc[i] requests a resource held by proc[j]
    int wait_for[MAX_PROC][MAX_PROC] = {0};

    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (i == j) continue;
            for (int k = 0; k < NUM_RESOURCES; k++) {
                // i requests resource k AND j holds resource k
                if (procs[i].request[k] > 0 && procs[j].allocation[k] > 0) {
                    wait_for[i][j] = 1;
                }
            }
        }
    }

    // DFS cycle detection
    int visited[MAX_PROC] = {0};
    int rec_stack[MAX_PROC] = {0};
    int path[MAX_PROC];
    int path_len = 0;

    // Simple cycle check using DFS
    for (int start = 0; start < n; start++) {
        if (visited[start]) continue;

        // DFS from 'start'
        int stack[MAX_PROC], sp = 0;
        int in_stack[MAX_PROC] = {0};
        stack[sp++] = start;

        while (sp > 0) {
            int node = stack[--sp];
            if (!visited[node]) {
                visited[node] = 1;
                in_stack[node] = 1;
                path[path_len++] = node;
            }

            for (int next = 0; next < n; next++) {
                if (!wait_for[node][next]) continue;
                if (!visited[next]) {
                    stack[sp++] = next;
                } else if (in_stack[next]) {
                    // Cycle found! Build cycle string
                    char tmp[MAX_MSG] = "";
                    int ci = 0;
                    while (ci < path_len && path[ci] != next) ci++;
                    for (int x = ci; x < path_len; x++) {
                        strcat(tmp, procs[path[x]].name);
                        if (x < path_len - 1) strcat(tmp, " → ");
                    }
                    strcat(tmp, " → ");
                    strcat(tmp, procs[next].name);
                    strncpy(cycle_out, tmp, MAX_MSG);
                    return 1; // Cycle found
                }
            }
        }

        // Reset in_stack for next DFS
        for (int i = 0; i < n; i++) in_stack[i] = 0;
        path_len = 0;
    }
    return 0; // No cycle
}

// ─── Recovery Suggestion ──────────────────────────────────────────────────────
void suggest_recovery(ProcessState procs[], int n, const char *cycle) {
    printf(RED "\n[SYSTEM] 🔴 DEADLOCK DETECTED — Cycle: %s\n" RESET, cycle);
    printf(RED "[SYSTEM] 🚨 EMERGENCY ALERT   — Broadcasting to all crew!\n" RESET);

    // Find process holding the most resources (rollback candidate)
    int max_held = -1, rollback_idx = 0;
    for (int i = 0; i < n; i++) {
        int held = 0;
        for (int j = 0; j < NUM_RESOURCES; j++)
            held += procs[i].allocation[j];
        // Prefer rolling back non-commander (index > 0 = lower priority)
        if (held > max_held && i > 0) {
            max_held = held;
            rollback_idx = i;
        }
    }

    printf(YELLOW "[SYSTEM] 🛠️  RECOVERY        — Rolling back: %s (lowest priority)\n" RESET,
           procs[rollback_idx].name);
    printf(YELLOW "[SYSTEM] 📢 SEMAPHORE POST   — Signaling blocked threads to wake up\n" RESET);

    // Signal semaphores for resources held by rolled-back process
    for (int j = 0; j < NUM_RESOURCES; j++) {
        if (procs[rollback_idx].allocation[j] > 0) {
            printf(GREEN "[SYSTEM] 🔓 RESOURCE FREED   — %s released by %s\n" RESET,
                   RESOURCE_NAMES[j], procs[rollback_idx].name);
            sem_post(&resource_sem[j]);
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

    // Load all users to build process states
    User users[MAX_USERS];
    int ucount = 0;
    load_users(users, &ucount);

    // Build process states from resources
    ProcessState procs[MAX_PROC];
    int n = 0;
    int available[NUM_RESOURCES] = {0};

    // Initialize procs for each user
    for (int i = 0; i < ucount && n < MAX_PROC; i++) {
        strncpy(procs[n].name, users[i].username, MAX_STR);
        memset(procs[n].allocation, 0, sizeof(procs[n].allocation));
        memset(procs[n].request, 0, sizeof(procs[n].request));
        n++;
    }

    // Fill allocation from resources.csv
    for (int r = 0; r < rcount; r++) {
        if (strcmp(res[r].held_by, "none") == 0) {
            // Find resource index
            for (int j = 0; j < NUM_RESOURCES; j++) {
                if (strcmp(RESOURCE_NAMES[j], res[r].resource) == 0) {
                    available[j] = 1;
                    break;
                }
            }
            continue;
        }
        // Find user index
        for (int i = 0; i < n; i++) {
            if (strcmp(procs[i].name, res[r].held_by) == 0) {
                for (int j = 0; j < NUM_RESOURCES; j++) {
                    if (strcmp(RESOURCE_NAMES[j], res[r].resource) == 0) {
                        procs[i].allocation[j] = 1;
                        break;
                    }
                }
                break;
            }
        }
    }

    // Print current allocation table
    print_separator();
    printf(CYAN "📊 RESOURCE ALLOCATION TABLE\n" RESET);
    print_separator();
    printf(WHITE "%-20s", "Resource");
    for (int i = 0; i < n; i++) printf("%-15s", procs[i].name);
    printf("\n");
    print_separator();
    for (int j = 0; j < NUM_RESOURCES; j++) {
        printf(YELLOW "%-20s" RESET, RESOURCE_NAMES[j]);
        for (int i = 0; i < n; i++) {
            if (procs[i].allocation[j])
                printf(GREEN "%-15s" RESET, "HELD");
            else
                printf("%-15s", "free");
        }
        printf("\n");
    }
    print_separator();

    // Run Banker's
    int safe_seq[MAX_PROC];
    int is_safe = run_bankers(procs, n, available, safe_seq);

    if (is_safe) {
        printf(GREEN "\n[SYSTEM] ✅ SAFE STATE       — No deadlock. Safe sequence: " RESET);
        for (int i = 0; i < n; i++)
            printf("%s%s", procs[safe_seq[i]].name, i < n-1 ? " → " : "\n");
        printf(GREEN "[SYSTEM] 🛢️  RIG STABLE       — All systems operational!\n" RESET);
        log_incident(current_user, "DEADLOCK_CHECK", "SAFE - No deadlock");
    } else {
        // Try cycle detection
        char cycle[MAX_MSG] = "";
        int has_cycle = detect_deadlock_cycle(procs, n, cycle);
        if (has_cycle) {
            suggest_recovery(procs, n, cycle);
        } else {
            printf(RED "\n[SYSTEM] ⚠️  UNSAFE STATE    — System in unsafe state but no cycle yet.\n" RESET);
            log_incident(current_user, "DEADLOCK_CHECK", "UNSAFE STATE");
        }
    }
}
