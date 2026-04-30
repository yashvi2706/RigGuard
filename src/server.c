// ─────────────────────────────────────────────────────────────────────────────
//  RigGuard Central Server — TCP socket server
//  Relays messages between all connected crew members in real-time
// ─────────────────────────────────────────────────────────────────────────────
#include "socket_common.h"
#include <signal.h>

// ─── Connected client table ───────────────────────────────────────────────────
typedef struct {
    int  fd;
    char username[MAX_STR];
    char role[MAX_STR];
    int  active;
} ClientEntry;

static ClientEntry clients[MAX_CLIENTS];
static int         client_count = 0;
static pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

int        client_sock = -1; // unused in server but needed for linking
char       g_username[MAX_STR] = "SERVER";
char       g_role[MAX_STR]     = "Server";

// ─── Broadcast packet to all connected clients (optionally skip one) ──────────
static void broadcast_to_all(Packet *p, int skip_fd) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active && clients[i].fd != skip_fd) {
            if (send(clients[i].fd, p, sizeof(Packet), MSG_NOSIGNAL) < 0) {
                clients[i].active = 0;
            }
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

// ─── Send to specific user ────────────────────────────────────────────────────
static void send_to_user(Packet *p, const char *username) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active && strcmp(clients[i].username, username) == 0) {
            send(clients[i].fd, p, sizeof(Packet), MSG_NOSIGNAL);
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

// ─── Remove client from table ─────────────────────────────────────────────────
static void remove_client(int fd) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].fd == fd) {
            printf(YELLOW "[SERVER] 🔌 DISCONNECTED   — %s left the rig\n" RESET, clients[i].username);

            // No leave announcement — crew don't need to know who logged out
            clients[i].active = 0;
            client_count--;
            pthread_mutex_unlock(&clients_mutex);
            close(fd);
            return;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

// ─── Per-client handler thread ────────────────────────────────────────────────
static void *handle_client(void *arg) {
    int fd = *(int *)arg;
    free(arg);

    Packet p;
    ssize_t n;

    // First packet must be JOIN with username/role
    n = recv(fd, &p, sizeof(p), 0);
    if (n <= 0 || strcmp(p.type, MSG_JOIN) != 0) {
        close(fd);
        return NULL;
    }

    // Register client
    pthread_mutex_lock(&clients_mutex);
    int slot = -1;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!clients[i].active) { slot = i; break; }
    }
    if (slot == -1) {
        pthread_mutex_unlock(&clients_mutex);
        close(fd);
        return NULL;
    }
    clients[slot].fd     = fd;
    clients[slot].active = 1;
    strncpy(clients[slot].username, p.sender,   MAX_STR);
    strncpy(clients[slot].role,     p.body,     MAX_STR);
    client_count++;
    printf(GREEN "[SERVER] ✅ CONNECTED      — %s (%s) joined | Total crew: %d\n" RESET,
           p.sender, p.body, client_count);
    pthread_mutex_unlock(&clients_mutex);

    // No join announcement — crew don't need to know who logged in

    // Main receive loop
    while ((n = recv(fd, &p, sizeof(p), 0)) > 0) {
        printf(CYAN "[SERVER] 📨 RELAY          — [%s] %s → %s: %s\n" RESET,
               p.type, p.sender, p.receiver, p.body);

        if (strcmp(p.type, MSG_BROADCAST) == 0) {
            // Emergency broadcast — send to ALL
            broadcast_to_all(&p, -1);
        } else if (strcmp(p.type, MSG_ALERT) == 0) {
            // Alert — send ONLY to specified receiver (commander)
            if (strcmp(p.receiver, "ALL") == 0)
                broadcast_to_all(&p, -1);
            else
                send_to_user(&p, p.receiver);
        } else if (strcmp(p.receiver, "ALL") == 0) {
            broadcast_to_all(&p, -1);
        } else {
            // Direct message to specific user only (no echo to sender)
            send_to_user(&p, p.receiver);
        }
    }

    remove_client(fd);
    return NULL;
}

// ─── Main server ──────────────────────────────────────────────────────────────
int main() {
    signal(SIGPIPE, SIG_IGN); // ignore broken pipe

    printf(CYAN);
    printf("\n╔══════════════════════════════════════════════════╗\n");
    printf("║   🛢️   RIGGUARD CENTRAL SERVER                   ║\n");
    printf("║      TCP Socket Relay — Port %-5d              ║\n", SERVER_PORT);
    printf("╚══════════════════════════════════════════════════╝\n");
    printf(RESET);

    memset(clients, 0, sizeof(clients));

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); exit(1); }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(SERVER_PORT);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind"); exit(1);
    }
    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("listen"); exit(1);
    }

    printf(GREEN "[SERVER] 🚀 LISTENING      — Waiting for crew on port %d...\n\n" RESET, SERVER_PORT);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t clen = sizeof(client_addr);
        int cfd = accept(server_fd, (struct sockaddr *)&client_addr, &clen);
        if (cfd < 0) continue;

        printf(YELLOW "[SERVER] 🔗 NEW CONNECTION — %s\n" RESET, inet_ntoa(client_addr.sin_addr));

        int *fdp = malloc(sizeof(int));
        *fdp = cfd;
        pthread_t tid;
        pthread_create(&tid, NULL, handle_client, fdp);
        pthread_detach(tid);
    }

    close(server_fd);
    return 0;
}
