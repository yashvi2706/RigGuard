// ─────────────────────────────────────────────────────────────────────────────
//  RigGuard Signal Module
//  SIGUSR1 → Emergency alert (sent by commander broadcast)
//  SIGUSR2 → Deadlock detected (sent by monitor thread)
//
//  Each process writes its PID to data/pids/<username>.pid on login
//  Signaling reads all PIDs and sends signal to every active crew process
// ─────────────────────────────────────────────────────────────────────────────
#include "signals.h"
#include <sys/stat.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#define PID_DIR "data/pids"

// ─── Signal message storage (set before kill, read in handler) ────────────────
static char signal_msg[MAX_MSG] = "";
static char signal_sender[MAX_STR] = "";

// ─── SIGUSR1 Handler — Emergency Alert ───────────────────────────────────────
static void handle_sigusr1(int sig) {
    (void)sig;
    // Write to stdout safely (async-signal-safe)
    const char *line  = "\n" RED;
    const char *hdr   = "[SIGNAL-SIGUSR1 ] 🚨 EMERGENCY SIGNAL  — Received from crew!\n";
    const char *reset = RESET;
    write(STDOUT_FILENO, line,  strlen(line));
    write(STDOUT_FILENO, hdr,   strlen(hdr));
    write(STDOUT_FILENO, reset, strlen(reset));
    // Note: can't use printf safely in signal handler
}

// ─── SIGUSR2 Handler — Deadlock Alert ────────────────────────────────────────
static void handle_sigusr2(int sig) {
    (void)sig;
    const char *line = "\n" RED;
    const char *hdr  = "[SIGNAL-SIGUSR2 ] ⚠️  DEADLOCK SIGNAL   — Monitor detected deadlock!\n";
    const char *reset = RESET;
    write(STDOUT_FILENO, line,  strlen(line));
    write(STDOUT_FILENO, hdr,   strlen(hdr));
    write(STDOUT_FILENO, reset, strlen(reset));
}

// ─── SIGINT Handler — Clean shutdown ─────────────────────────────────────────
static void handle_sigint(int sig) {
    (void)sig;
    const char *msg = "\n[SIGNAL-SIGINT  ] 🛑 INTERRUPTED       — Cleaning up and exiting...\n";
    write(STDOUT_FILENO, msg, strlen(msg));
    // Remove PID file — we'll call cleanup on exit normally
    _exit(0);
}

// ─── Setup all signal handlers ────────────────────────────────────────────────
void setup_signal_handlers() {
    struct sigaction sa1, sa2, sa_int;
    memset(&sa1,    0, sizeof(sa1));
    memset(&sa2,    0, sizeof(sa2));
    memset(&sa_int, 0, sizeof(sa_int));

    sa1.sa_handler    = handle_sigusr1;
    sa2.sa_handler    = handle_sigusr2;
    sa_int.sa_handler = handle_sigint;

    sigemptyset(&sa1.sa_mask);
    sigemptyset(&sa2.sa_mask);
    sigemptyset(&sa_int.sa_mask);

    sigaction(SIGUSR1, &sa1,    NULL);
    sigaction(SIGUSR2, &sa2,    NULL);
    sigaction(SIGINT,  &sa_int, NULL);

    printf(GREEN "[SIGNAL] ✅ HANDLERS SET     — SIGUSR1=Emergency, SIGUSR2=Deadlock, SIGINT=Shutdown\n" RESET);
}

// ─── Write this process PID to data/pids/<username>.pid ──────────────────────
void write_signal_pid(const char *username) {
    mkdir(PID_DIR, 0755);
    char path[MAX_STR];
    snprintf(path, sizeof(path), "%s/%s.pid", PID_DIR, username);
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "%d\n", getpid());
        fclose(f);
        printf(GREEN "[SIGNAL] 📝 PID REGISTERED  — PID %d written to %s\n" RESET, getpid(), path);
    }
}

// ─── Remove PID file on logout ────────────────────────────────────────────────
void cleanup_signal_pid(const char *username) {
    char path[MAX_STR];
    snprintf(path, sizeof(path), "%s/%s.pid", PID_DIR, username);
    remove(path);
    printf(YELLOW "[SIGNAL] 🗑️  PID REMOVED     — %s unregistered\n" RESET, username);
}

// ─── Send signal to ALL active crew processes ─────────────────────────────────
void signal_all_crew(int signum, const char *message) {
    DIR *dir = opendir(PID_DIR);
    if (!dir) {
        printf(YELLOW "[SIGNAL] ⚠️  No PID dir found — no other processes to signal\n" RESET);
        return;
    }

    const char *sig_name = (signum == SIGUSR1) ? "SIGUSR1 (Emergency)" : "SIGUSR2 (Deadlock)";
    printf(YELLOW "[SIGNAL] 📢 BROADCASTING     — Sending %s to all crew processes\n" RESET, sig_name);
    if (message && strlen(message) > 0)
        printf(YELLOW "[SIGNAL] 📋 MESSAGE          — %s\n" RESET, message);

    pid_t my_pid = getpid();
    int   count  = 0;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        // Only process .pid files
        char *dot = strrchr(entry->d_name, '.');
        if (!dot || strcmp(dot, ".pid") != 0) continue;

        char path[MAX_STR];
        snprintf(path, sizeof(path), "%s/%s", PID_DIR, entry->d_name);

        FILE *f = fopen(path, "r");
        if (!f) continue;

        pid_t target_pid;
        fscanf(f, "%d", &target_pid);
        fclose(f);

        if (target_pid == my_pid) continue; // don't signal yourself

        // Check process is still alive
        if (kill(target_pid, 0) == 0) {
            // Process exists — send signal
            if (kill(target_pid, signum) == 0) {
                // Extract username from filename (remove .pid)
                char uname[MAX_STR];
                strncpy(uname, entry->d_name, sizeof(uname));
                char *ext = strrchr(uname, '.');
                if (ext) *ext = '\0';

                printf(GREEN "[SIGNAL] ✅ SIGNAL SENT      — %s (PID %d) received %s\n" RESET,
                       uname, target_pid, sig_name);
                count++;
            }
        } else {
            // Stale PID file — remove it
            remove(path);
        }
    }
    closedir(dir);

    if (count == 0)
        printf(YELLOW "[SIGNAL] ℹ️  NO OTHER CREW    — No other active processes found\n" RESET);
    else
        printf(GREEN "[SIGNAL] 📊 TOTAL SIGNALED   — %d crew process(es) notified\n" RESET, count);
}
