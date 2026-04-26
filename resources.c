#include "resources.h"
#include <fcntl.h>
#include <sys/file.h>
#include <errno.h>

// ─── File-based cross-process lock ───────────────────────────────────────────
static int lock_fd = -1;

static void cross_lock() {
    lock_fd = open("data/rigguard.lock", O_CREAT | O_RDWR, 0666);
    flock(lock_fd, LOCK_EX);
}

static void cross_unlock() {
    flock(lock_fd, LOCK_UN);
    close(lock_fd);
    lock_fd = -1;
}

// ─── Init named semaphores (shared across ALL processes) ─────────────────────
// NOTE: O_CREAT without O_EXCL means "open if exists, create if not"
// This way all processes share the SAME semaphore object in the kernel
void init_semaphores() {
    // Load current resource count from CSV
    Resource res[MAX_RESOURCES];
    load_resources(res, &num_resources);

    for (int i = 0; i < num_resources; i++) {
        char sem_name[MAX_STR];
        snprintf(sem_name, sizeof(sem_name), "/rigguard_sem_%d", i);
        resource_sem[i] = sem_open(sem_name, O_CREAT, 0666, 0);
        if (resource_sem[i] == SEM_FAILED) {
            perror("sem_open failed");
            exit(1);
        }
    }
    printf(GREEN "[SYSTEM] ✅ SEMAPHORES INIT  — %d named semaphores opened (shared across all processes)\n" RESET, num_resources);
}

// ─── Cleanup: only unlink when you are the LAST process (on Makefile reset) ──
void cleanup_semaphores() {
    for (int i = 0; i < num_resources; i++) {
        sem_close(resource_sem[i]);
    }
}

// ─── Called once on first boot to reset stale semaphores ─────────────────────
void reset_semaphores() {
    // Reset up to MAX_RESOURCES semaphores
    for (int i = 0; i < MAX_RESOURCES; i++) {
        char sem_name[MAX_STR];
        snprintf(sem_name, sizeof(sem_name), "/rigguard_sem_%d", i);
        sem_unlink(sem_name);
    }
    printf(GREEN "[SYSTEM] 🔄 SEMAPHORES RESET — Stale semaphores cleared\n" RESET);
}

// ─── Sync semaphores for any newly added resources ───────────────────────────
static void sync_semaphores(int new_count) {
    for (int i = num_resources; i < new_count; i++) {
        char sem_name[MAX_STR];
        snprintf(sem_name, sizeof(sem_name), "/rigguard_sem_%d", i);
        resource_sem[i] = sem_open(sem_name, O_CREAT, 0666, 0);
    }
    num_resources = new_count;
}

void request_resource(const char *username, const char *role) {
    (void)role;
    Resource res[MAX_RESOURCES];
    int rcount = 0;

    print_separator();
    printf(CYAN "🔧 RESOURCE REQUEST\n" RESET);
    print_separator();
    printf(WHITE "Available Resources:\n" RESET);
    // Load current resources dynamically (picks up newly added equipment too)
    Resource cur_res[MAX_RESOURCES];
    int cur_count = 0;
    load_resources(cur_res, &cur_count);

    // Sync semaphores for any newly added equipment
    if (cur_count > num_resources) {
        printf(GREEN "[SYSTEM] 🔗 NEW EQUIPMENT     — %d new resource(s) detected, syncing semaphores\n" RESET, cur_count - num_resources);
        sync_semaphores(cur_count);
    }
    num_resources = cur_count;

    for (int i = 0; i < cur_count; i++)
        printf("  %d. %s\n", i+1, cur_res[i].resource);
    printf(WHITE "Enter resource number (1-%d): " RESET, num_resources);

    int choice;
    scanf("%d", &choice);
    if (choice < 1 || choice > num_resources) {
        printf(RED "❌ Invalid choice.\n" RESET);
        return;
    }
    choice--;

    const char *res_name = cur_res[choice].resource;
    char detail[MAX_MSG];

    printf(YELLOW "\n[THREAD-%-10s] 📋 REQUESTING        — %s\n" RESET, username, res_name);
    log_incident(username, "REQUESTED", res_name);

    // ── Step 1: Check status under cross-process file lock ────────────────
    printf(YELLOW "[THREAD-%-10s] 🔒 MUTEX LOCKED      — Entering critical section (resource table)\n" RESET, username);
    cross_lock();

    load_resources(res, &rcount);

    int res_idx = -1;
    for (int i = 0; i < rcount; i++) {
        if (strcmp(res[i].resource, res_name) == 0) { res_idx = i; break; }
    }

    if (res_idx == -1) {
        printf(RED "[THREAD-%-10s] ❌ RESOURCE NOT FOUND — %s\n" RESET, username, res_name);
        cross_unlock();
        return;
    }

    // Fix: prevent self-deadlock — user already holds this resource
    if (res[res_idx].status == 1 && strcmp(res[res_idx].held_by, username) == 0) {
        cross_unlock();
        printf(YELLOW "[THREAD-%-10s] 🔓 MUTEX UNLOCKED    — Exiting critical section\n" RESET, username);
        printf(YELLOW "[THREAD-%-10s] ⚠️  ALREADY HELD     — You already hold %s!\n" RESET, username, res_name);
        return;
    }

    if (res[res_idx].status == 0) {
        // ── FREE: grant immediately ────────────────────────────────────────
        strncpy(res[res_idx].held_by, username, MAX_STR);
        strncpy(res[res_idx].waited_by, "none", MAX_STR);
        res[res_idx].status = 1;
        save_resources(res, rcount);
        cross_unlock();
        printf(YELLOW "[THREAD-%-10s] 🔓 MUTEX UNLOCKED    — Exiting critical section\n" RESET, username);
        printf(GREEN "[THREAD-%-10s] ✅ RESOURCE GRANTED  — %s allocated to you\n" RESET, username, res_name);
        snprintf(detail, sizeof(detail), "Granted %s", res_name);
        log_incident(username, "GRANTED", detail);

    } else {
        // ── BUSY: record who is waiting, release lock, sleep on semaphore ──
        char held_by[MAX_STR];
        strncpy(held_by, res[res_idx].held_by, MAX_STR);

        // *** KEY FIX: write waited_by to CSV so deadlock detector can see it ***
        strncpy(res[res_idx].waited_by, username, MAX_STR);
        save_resources(res, rcount);

        cross_unlock();
        printf(YELLOW "[THREAD-%-10s] 🔓 MUTEX UNLOCKED    — Released lock while sleeping\n" RESET, username);

        printf(RED "[THREAD-%-10s] ⚠️  RESOURCE BUSY     — %s held by %s\n" RESET, username, res_name, held_by);
        printf(YELLOW "[THREAD-%-10s] 😴 THREAD SLEEPING   — Waiting for %s to be released\n" RESET, username, res_name);
        printf(YELLOW "[THREAD-%-10s] 📝 WAIT RECORDED     — Written to resources.csv for deadlock detection\n" RESET, username);
        snprintf(detail, sizeof(detail), "Blocked on %s held by %s", res_name, held_by);
        log_incident(username, "BLOCKED", detail);

        printf(YELLOW "[THREAD-%-10s] ⏳ SEMAPHORE WAIT    — Blocking on %s semaphore...\n" RESET, username, res_name);
        fflush(stdout);

        sem_wait(resource_sem[choice]); // ← TRULY BLOCKS, woken by sem_post in release

        printf(GREEN "[THREAD-%-10s] 🌅 THREAD AWAKE      — Woken by semaphore signal!\n" RESET, username);
        printf(GREEN "[THREAD-%-10s] 🔄 RETRYING          — Re-attempting to acquire %s\n" RESET, username, res_name);

        printf(YELLOW "[THREAD-%-10s] 🔒 MUTEX LOCKED      — Re-entering critical section\n" RESET, username);
        cross_lock();

        load_resources(res, &rcount);
        res_idx = -1;
        for (int i = 0; i < rcount; i++) {
            if (strcmp(res[i].resource, res_name) == 0) { res_idx = i; break; }
        }

        if (res_idx != -1 && res[res_idx].status == 0) {
            strncpy(res[res_idx].held_by, username, MAX_STR);
            strncpy(res[res_idx].waited_by, "none", MAX_STR);
            res[res_idx].status = 1;
            save_resources(res, rcount);
            cross_unlock();
            printf(YELLOW "[THREAD-%-10s] 🔓 MUTEX UNLOCKED    — Exiting critical section\n" RESET, username);
            printf(GREEN "[THREAD-%-10s] ✅ RESOURCE GRANTED  — %s allocated after wait!\n" RESET, username, res_name);
            snprintf(detail, sizeof(detail), "Granted %s after wait", res_name);
            log_incident(username, "GRANTED", detail);
        } else {
            cross_unlock();
            printf(YELLOW "[THREAD-%-10s] 🔓 MUTEX UNLOCKED    — Exiting critical section\n" RESET, username);
            printf(RED "[THREAD-%-10s] ⚠️  RACE CONDITION    — Resource grabbed by another, try again\n" RESET, username);
            log_incident(username, "RACE_REQUEUE", res_name);
        }
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
    cross_lock();

    load_resources(res, &rcount);
    int found = 0;
    int sem_idx = -1;

    for (int i = 0; i < rcount; i++) {
        if (strcmp(res[i].resource, res_name) == 0 &&
            strcmp(res[i].held_by, username) == 0) {
            strncpy(res[i].held_by, "none", MAX_STR);
            strncpy(res[i].waited_by, "none", MAX_STR);
            res[i].status = 0;
            save_resources(res, rcount);
            found = 1;
            for (int j = 0; j < rcount; j++) {
                if (strcmp(res[j].resource, res_name) == 0) { sem_idx = j; break; }
            }
            break;
        }
    }

    cross_unlock();
    printf(YELLOW "[THREAD-%-10s] 🔓 MUTEX UNLOCKED    — Exiting critical section\n" RESET, username);

    if (found) {
        printf(GREEN "[THREAD-%-10s] ✅ RESOURCE RELEASED — %s is now free\n" RESET, username, res_name);
        log_incident(username, "RELEASED", res_name);
        if (sem_idx >= 0) {
            printf(GREEN "[THREAD-%-10s] 📢 SEMAPHORE POST    — Signaling waiting processes for %s\n" RESET, username, res_name);
            fflush(stdout);
            sem_post(resource_sem[sem_idx]); // ← wakes blocked process
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
