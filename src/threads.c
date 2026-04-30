// ─────────────────────────────────────────────────────────────────────────────
//  RigGuard Threading Module
//  Thread 1: Listener  — receives real-time messages from server via socket
//  Thread 2: Monitor   — background deadlock checker every 10 seconds
// ─────────────────────────────────────────────────────────────────────────────
#include "threads.h"
#include "deadlock.h"
#include "filestore.h"
#include "signals.h"

// ─── Globals ──────────────────────────────────────────────────────────────────
int  client_sock  = -1;
char g_username[MAX_STR] = "";
char g_role[MAX_STR]     = "";

static pthread_t listener_tid;
static pthread_t monitor_tid;
static volatile int threads_running = 0;

// ─── Thread 1: Socket Listener ────────────────────────────────────────────────
// Runs in background, prints incoming messages from server instantly
static void *listener_thread(void *arg) {
    (void)arg;
    Packet p;
    fflush(stdout);

    while (threads_running && client_sock >= 0) {
        ssize_t n = recv(client_sock, &p, sizeof(p), 0);
        if (n <= 0) {
            if (threads_running)
                printf(RED "\n[LISTENER-THREAD] 🔴 DISCONNECTED   — Lost connection to server\n" RESET);
            break;
        }

        // Print received message based on type
        printf("\n"); // newline so it doesn't overlap with menu prompt
        if (strcmp(p.type, MSG_BROADCAST) == 0 || strcmp(p.type, MSG_ALERT) == 0) {
            printf(RED "[LISTENER-THREAD] 🚨 INCOMING ALERT  — [%s] %s: %s\n" RESET,
                   p.timestamp, p.sender, p.body);
        } else if (strcmp(p.type, MSG_JOIN) == 0 || strcmp(p.type, MSG_LEAVE) == 0) {
            // Silently ignore join/leave notifications
            continue;
        } else {
            // Normal chat
            if (strcmp(p.receiver, "ALL") == 0)
                printf(MAGENTA "[LISTENER-THREAD] 📡 BROADCAST       — [%s] %s → ALL: %s\n" RESET,
                       p.timestamp, p.sender, p.body);
            else
                printf(CYAN "[LISTENER-THREAD] 💬 DIRECT MSG      — [%s] %s → %s: %s\n" RESET,
                       p.timestamp, p.sender, p.receiver, p.body);
        }
        fflush(stdout);

        // Don't save to CSV here — sender already saved it via append_message()
        // Saving here causes duplicates in message board
    }

    printf(YELLOW "[LISTENER-THREAD] 🔴 STOPPED\n" RESET);
    return NULL;
}

// ─── Thread 2: Background Deadlock Monitor ────────────────────────────────────
// Checks for deadlock every 10 seconds, alerts via socket if found
static void *monitor_thread(void *arg) {
    (void)arg;
    fflush(stdout);

    int cycle = 0;
    char last_reported_cycle[MAX_MSG] = ""; // track last deadlock so we don't spam
    while (threads_running) {
        sleep(5);
        if (!threads_running) break;

        // Only run deadlock check if someone is actually waiting for a resource
        Resource res[MAX_RESOURCES];
        int rcount = 0;
        load_resources_silent(res, &rcount);

        int anyone_waiting = 0;
        for (int i = 0; i < rcount; i++) {
            if (strcmp(res[i].waited_by, "none") != 0) {
                anyone_waiting = 1;
                break;
            }
        }
        if (!anyone_waiting) {
            // Reset last reported cycle when no one is waiting
            memset(last_reported_cycle, 0, sizeof(last_reported_cycle));
            continue;
        }

        cycle++;
        // Silent scan — no print until deadlock actually found

        User users[MAX_USERS];
        int ucount = 0;
        load_users(users, &ucount);

        // Build process states
        typedef struct { char name[MAX_STR]; int alloc[MAX_RESOURCES]; int req[MAX_RESOURCES]; } PS;
        PS procs[MAX_USERS];
        int n = 0;

        for (int i = 0; i < ucount && n < MAX_USERS; i++) {
            strncpy(procs[n].name, users[i].username, MAX_STR);
            memset(procs[n].alloc, 0, sizeof(procs[n].alloc));
            memset(procs[n].req,   0, sizeof(procs[n].req));
            n++;
        }

        for (int r = 0; r < rcount; r++) {
            int res_j = r;
            if (strcmp(res[r].held_by, "none") != 0) {
                for (int i = 0; i < n; i++) {
                    if (strcmp(procs[i].name, res[r].held_by) == 0) {
                        procs[i].alloc[res_j] = 1; break;
                    }
                }
            }
            if (strcmp(res[r].waited_by, "none") != 0) {
                for (int i = 0; i < n; i++) {
                    if (strcmp(procs[i].name, res[r].waited_by) == 0) {
                        procs[i].req[res_j] = 1; break;
                    }
                }
            }
        }

        // Simple cycle check
        int wait_for[MAX_USERS][MAX_USERS];
        memset(wait_for, 0, sizeof(wait_for));
        for (int i = 0; i < n; i++)
            for (int j = 0; j < n; j++)
                if (i != j)
                    for (int k = 0; k < rcount; k++)
                        if (procs[i].req[k] && procs[j].alloc[k])
                            wait_for[i][j] = 1;

        // DFS cycle detection
        int visited[MAX_USERS] = {0};
        int deadlock_found = 0;
        char cycle_desc[MAX_MSG] = "";

        for (int s = 0; s < n && !deadlock_found; s++) {
            if (visited[s]) continue;
            int stack[MAX_USERS], sp = 0;
            int path[MAX_USERS], plen = 0;
            int in_stack[MAX_USERS] = {0};
            stack[sp++] = s;

            while (sp > 0 && !deadlock_found) {
                int node = stack[sp-1];
                if (!visited[node]) {
                    visited[node] = 1;
                    in_stack[node] = 1;
                    path[plen++] = node;
                }
                int pushed = 0;
                for (int next = 0; next < n; next++) {
                    if (!wait_for[node][next]) continue;
                    if (!visited[next]) {
                        stack[sp++] = next;
                        pushed = 1; break;
                    } else if (in_stack[next]) {
                        // Cycle found
                        deadlock_found = 1;
                        int ci = 0;
                        while (ci < plen && path[ci] != next) ci++;
                        for (int x = ci; x < plen; x++) {
                            strcat(cycle_desc, procs[path[x]].name);
                            if (x < plen-1) strcat(cycle_desc, "→");
                        }
                        strcat(cycle_desc, "→");
                        strcat(cycle_desc, procs[next].name);
                        break;
                    }
                }
                if (!pushed && !deadlock_found) {
                    in_stack[node] = 0;
                    sp--;
                    if (plen > 0) plen--;
                }
            }
        }

        if (deadlock_found) {
            // Only alert if this is a NEW deadlock (not same one already reported)
            if (strcmp(cycle_desc, last_reported_cycle) != 0) {
                strncpy(last_reported_cycle, cycle_desc, MAX_MSG);

                printf(RED "[MONITOR-THREAD ] 🔴 DEADLOCK FOUND — Cycle: %s\n" RESET, cycle_desc);
                printf(RED "[MONITOR-THREAD ] 📢 ALERTING       — Notifying Rig Commander only\n" RESET);
                fflush(stdout);

                // Send alert to COMMANDER ONLY via socket (not all crew)
                if (client_sock >= 0) {
                    char alert_body[MAX_MSG * 2];
                    snprintf(alert_body, sizeof(alert_body),
                             "⚠️ DEADLOCK DETECTED: %s — Run Option 6 to resolve!", cycle_desc);
                    send_packet(client_sock, MSG_ALERT, g_username, "commander", alert_body);
                }

                // Send SIGUSR2 only to commander process
                printf(RED "[MONITOR-THREAD ] 📢 SIGUSR2 SENDING — Deadlock signal to commander\n" RESET);
                // Find commander PID and signal only that
                char pid_path[MAX_STR];
                snprintf(pid_path, sizeof(pid_path), "data/pids/commander.pid");
                FILE *pf = fopen(pid_path, "r");
                if (pf) {
                    pid_t cmd_pid;
                    fscanf(pf, "%d", &cmd_pid);
                    fclose(pf);
                    if (kill(cmd_pid, 0) == 0) {
                        kill(cmd_pid, SIGUSR2);
                        printf(GREEN "[MONITOR-THREAD ] ✅ SIGNAL SENT    — Commander (PID %d) notified\n" RESET, cmd_pid);
                    }
                } else {
                    printf(YELLOW "[MONITOR-THREAD ] ⚠️  COMMANDER OFFLINE — No active commander session\n" RESET);
                }

                log_incident(g_username, "MONITOR_DEADLOCK", cycle_desc);

                // Stop checking until deadlock is resolved
                sleep(30);
            }
            // If same deadlock still present — stay silent, just wait
        } else {
            // Silent when safe — no spam
            memset(last_reported_cycle, 0, sizeof(last_reported_cycle));
        }
    }

    printf(YELLOW "[MONITOR-THREAD ] 🔴 STOPPED\n" RESET);
    return NULL;
}

// ─── Connect to Server ────────────────────────────────────────────────────────
int connect_to_server(const char *username, const char *role) {
    client_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (client_sock < 0) {
        printf(YELLOW "[SOCKET] ⚠️  Could not create socket — running without server\n" RESET);
        return 0;
    }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &addr.sin_addr);

    if (connect(client_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        printf(YELLOW "[SOCKET] ⚠️  Server not reachable — running in offline mode\n" RESET);
        printf(YELLOW "[SOCKET]    Start server with: ./rigguard_server\n" RESET);
        close(client_sock);
        client_sock = -1;
        return 0;
    }

    strncpy(g_username, username, MAX_STR);
    strncpy(g_role,     role,     MAX_STR);

    // Send JOIN packet
    send_packet(client_sock, MSG_JOIN, username, "ALL", role);
    printf(GREEN "[SOCKET] ✅ CONNECTED       — Joined RigGuard server at %s:%d\n" RESET,
           SERVER_IP, SERVER_PORT);
    return 1;
}

void disconnect_from_server() {
    if (client_sock >= 0) {
        send_packet(client_sock, MSG_LEAVE, g_username, "ALL", "Logging out");
        close(client_sock);
        client_sock = -1;
    }
}

// ─── Start Background Threads ─────────────────────────────────────────────────
void start_threads(const char *username, const char *role) {
    strncpy(g_username, username, MAX_STR);
    strncpy(g_role,     role,     MAX_STR);
    threads_running = 1;

    printf(CYAN "\n[SYSTEM] 🧵 THREADS STARTING — Launching background threads...\n" RESET);

    // Thread 1: Socket listener (only if connected)
    if (client_sock >= 0) {
        pthread_create(&listener_tid, NULL, listener_thread, NULL);
        printf(GREEN "[SYSTEM] 🧵 THREAD-1 CREATED — Listener thread (socket message receiver)\n" RESET);
    }

    // Thread 2: Background deadlock monitor
    pthread_create(&monitor_tid, NULL, monitor_thread, NULL);
    printf(GREEN "[SYSTEM] 🧵 THREAD-2 CREATED — Monitor thread (auto deadlock checker every 10s)\n" RESET);
    fflush(stdout);
}

void stop_threads() {
    threads_running = 0;
    disconnect_from_server();

    // Wake monitor thread (it's sleeping)
    pthread_cancel(monitor_tid);
    pthread_join(monitor_tid, NULL);

    if (client_sock < 0 && listener_tid) {
        pthread_cancel(listener_tid);
    }
    printf(YELLOW "[SYSTEM] 🧵 THREADS STOPPED — All background threads terminated\n" RESET);
}

// ─── Socket Send Helpers ──────────────────────────────────────────────────────
void socket_send_message(const char *sender, const char *receiver, const char *msg) {
    if (client_sock < 0) return; // offline mode
    send_packet(client_sock, MSG_CHAT, sender, receiver, msg);
    printf(CYAN "[SOCKET] 📤 SENT             — Message to %s via TCP socket\n" RESET, receiver);
}

void socket_send_emergency(const char *sender, const char *msg) {
    if (client_sock < 0) return;
    send_packet(client_sock, MSG_BROADCAST, sender, "ALL", msg);
    printf(RED "[SOCKET] 📤 EMERGENCY SENT   — Broadcast via TCP socket to all crew\n" RESET);
}
