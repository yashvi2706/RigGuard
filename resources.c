#include "resources.h"

// Semaphores start at 0.
// When a thread finds a resource BUSY, it calls sem_wait() → blocks.
// When a thread releases a resource, it calls sem_post() → wakes one waiter.
void init_semaphores() {
    for (int i = 0; i < NUM_RESOURCES; i++) {
        sem_init(&resource_sem[i], 0, 0); // 0 = no signals pending
    }
    printf(GREEN "[SYSTEM] ✅ SEMAPHORES INIT  — %d resource semaphores initialized (value=0)\n" RESET, NUM_RESOURCES);
}

void request_resource(const char *username, const char *role) {
    (void)role;
    Resource res[MAX_RESOURCES];
    int rcount = 0;

    print_separator();
    printf(CYAN "🔧 RESOURCE REQUEST\n" RESET);
    print_separator();
    printf(WHITE "Available Resources:\n" RESET);
    for (int i = 0; i < NUM_RESOURCES; i++)
        printf("  %d. %s\n", i+1, RESOURCE_NAMES[i]);
    printf(WHITE "Enter resource number (1-%d): " RESET, NUM_RESOURCES);

    int choice;
    scanf("%d", &choice);
    if (choice < 1 || choice > NUM_RESOURCES) {
        printf(RED "❌ Invalid choice.\n" RESET);
        return;
    }
    choice--;

    const char *res_name = RESOURCE_NAMES[choice];
    char detail[MAX_MSG];

    printf(YELLOW "\n[THREAD-%-10s] 📋 REQUESTING        — %s\n" RESET, username, res_name);
    log_incident(username, "REQUESTED", res_name);

    // ── Step 1: Check status under mutex ──────────────────────────────────
    printf(YELLOW "[THREAD-%-10s] 🔒 MUTEX LOCKED      — Entering critical section (resource table)\n" RESET, username);
    pthread_mutex_lock(&resource_mutex);

    load_resources(res, &rcount);

    int res_idx = -1;
    for (int i = 0; i < rcount; i++) {
        if (strcmp(res[i].resource, res_name) == 0) { res_idx = i; break; }
    }

    if (res_idx == -1) {
        printf(RED "[THREAD-%-10s] ❌ RESOURCE NOT FOUND — %s\n" RESET, username, res_name);
        pthread_mutex_unlock(&resource_mutex);
        return;
    }

    if (res[res_idx].status == 0) {
        // ── FREE: grant immediately ────────────────────────────────────────
        strncpy(res[res_idx].held_by, username, MAX_STR);
        res[res_idx].status = 1;
        save_resources(res, rcount);
        pthread_mutex_unlock(&resource_mutex);
        printf(YELLOW "[THREAD-%-10s] 🔓 MUTEX UNLOCKED    — Exiting critical section\n" RESET, username);
        printf(GREEN "[THREAD-%-10s] ✅ RESOURCE GRANTED  — %s allocated to you\n" RESET, username, res_name);
        snprintf(detail, sizeof(detail), "Granted %s", res_name);
        log_incident(username, "GRANTED", detail);

    } else {
        // ── BUSY: release mutex and sleep on semaphore ─────────────────────
        char held_by[MAX_STR];
        strncpy(held_by, res[res_idx].held_by, MAX_STR);

        printf(RED "[THREAD-%-10s] ⚠️  RESOURCE BUSY     — %s held by %s\n" RESET, username, res_name, held_by);
        printf(YELLOW "[THREAD-%-10s] 😴 THREAD SLEEPING   — Waiting for %s to be released\n" RESET, username, res_name);
        snprintf(detail, sizeof(detail), "Blocked on %s held by %s", res_name, held_by);
        log_incident(username, "BLOCKED", detail);

        pthread_mutex_unlock(&resource_mutex);
        printf(YELLOW "[THREAD-%-10s] 🔓 MUTEX UNLOCKED    — Released lock while sleeping\n" RESET, username);

        // BLOCK until release_resource() calls sem_post()
        printf(YELLOW "[THREAD-%-10s] ⏳ SEMAPHORE WAIT    — Blocking on %s semaphore...\n" RESET, username, res_name);
        sem_wait(&resource_sem[choice]);

        printf(GREEN "[THREAD-%-10s] 🌅 THREAD AWAKE      — Woken by semaphore signal!\n" RESET, username);
        printf(GREEN "[THREAD-%-10s] 🔄 RETRYING          — Re-attempting to acquire %s\n" RESET, username, res_name);

        // Re-acquire mutex and grab the now-free resource
        printf(YELLOW "[THREAD-%-10s] 🔒 MUTEX LOCKED      — Re-entering critical section\n" RESET, username);
        pthread_mutex_lock(&resource_mutex);

        load_resources(res, &rcount);
        res_idx = -1;
        for (int i = 0; i < rcount; i++) {
            if (strcmp(res[i].resource, res_name) == 0) { res_idx = i; break; }
        }

        if (res_idx != -1 && res[res_idx].status == 0) {
            strncpy(res[res_idx].held_by, username, MAX_STR);
            res[res_idx].status = 1;
            save_resources(res, rcount);
            printf(GREEN "[THREAD-%-10s] ✅ RESOURCE GRANTED  — %s allocated after wait!\n" RESET, username, res_name);
            snprintf(detail, sizeof(detail), "Granted %s after wait", res_name);
            log_incident(username, "GRANTED", detail);
        } else {
            printf(RED "[THREAD-%-10s] ⚠️  RACE CONDITION    — grabbed by another thread, re-queuing\n" RESET, username);
            log_incident(username, "RACE_REQUEUE", res_name);
        }

        pthread_mutex_unlock(&resource_mutex);
        printf(YELLOW "[THREAD-%-10s] 🔓 MUTEX UNLOCKED    — Exiting critical section\n" RESET, username);
    }

    print_separator();
}

void release_resource(const char *username) {
    Resource res[MAX_RESOURCES];
    int rcount = 0;

    print_separator();
    printf(CYAN "🔓 RELEASE RESOURCE\n" RESET);
    print_separator();

    load_resources(res, &rcount);
    int holds_any = 0;
    printf(WHITE "Resources you currently hold:\n" RESET);
    for (int i = 0; i < rcount; i++) {
        if (strcmp(res[i].held_by, username) == 0) {
            printf("  %d. %s\n", i+1, res[i].resource);
            holds_any = 1;
        }
    }

    if (!holds_any) {
        printf(YELLOW "You are not holding any resources.\n" RESET);
        return;
    }

    printf(WHITE "Enter resource name to release: " RESET);
    char res_name[MAX_STR];
    scanf("%s", res_name);

    printf(YELLOW "\n[THREAD-%-10s] 🔒 MUTEX LOCKED      — Entering critical section\n" RESET, username);
    pthread_mutex_lock(&resource_mutex);

    load_resources(res, &rcount);
    int found = 0;
    int sem_idx = -1;

    for (int i = 0; i < rcount; i++) {
        if (strcmp(res[i].resource, res_name) == 0 &&
            strcmp(res[i].held_by, username) == 0) {
            strncpy(res[i].held_by, "none", MAX_STR);
            res[i].status = 0;
            save_resources(res, rcount);
            found = 1;
            for (int j = 0; j < NUM_RESOURCES; j++) {
                if (strcmp(RESOURCE_NAMES[j], res_name) == 0) { sem_idx = j; break; }
            }
            break;
        }
    }

    pthread_mutex_unlock(&resource_mutex);
    printf(YELLOW "[THREAD-%-10s] 🔓 MUTEX UNLOCKED    — Exiting critical section\n" RESET, username);

    if (found) {
        printf(GREEN "[THREAD-%-10s] ✅ RESOURCE RELEASED — %s is now free\n" RESET, username, res_name);
        log_incident(username, "RELEASED", res_name);
        if (sem_idx >= 0) {
            // Wake up any thread sleeping on this semaphore
            printf(GREEN "[THREAD-%-10s] 📢 SEMAPHORE POST    — Signaling waiting threads for %s\n" RESET, username, res_name);
            sem_post(&resource_sem[sem_idx]);
        }
    } else {
        printf(RED "[THREAD-%-10s] ❌ RELEASE FAILED    — You don't hold %s\n" RESET, username, res_name);
    }

    print_separator();
}

void view_allocation_table() {
    Resource res[MAX_RESOURCES];
    int rcount = 0;
    load_resources(res, &rcount);

    print_separator();
    printf(CYAN "📊 EQUIPMENT ALLOCATION TABLE\n" RESET);
    print_separator();
    printf(WHITE "%-25s %-20s %s\n" RESET, "Resource", "Held By", "Status");
    print_separator();
    for (int i = 0; i < rcount; i++) {
        if (res[i].status == 1)
            printf(RED "%-25s %-20s %s\n" RESET, res[i].resource, res[i].held_by, "🔴 ALLOCATED");
        else
            printf(GREEN "%-25s %-20s %s\n" RESET, res[i].resource, "none", "🟢 FREE");
    }
    print_separator();
}
