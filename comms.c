#include "comms.h"
#include "threads.h"
#include "signals.h"

void send_message(const char *sender) {
    print_separator();
    printf(CYAN "✉️  SEND MESSAGE\n" RESET);
    print_separator();

    User users[MAX_USERS];
    int ucount = 0;
    load_users(users, &ucount);

    printf(WHITE "Send to:\n" RESET);
    printf("  0. 📢 ALL (Broadcast)\n");
    for (int i = 0; i < ucount; i++) {
        if (strcmp(users[i].username, sender) != 0)
            printf("  %d. %s (%s)\n", i+1, users[i].username, users[i].role);
    }

    printf(WHITE "Choice: " RESET);
    int choice;
    scanf("%d", &choice);

    char receiver[MAX_STR];
    if (choice == 0) {
        strncpy(receiver, "ALL", MAX_STR);
    } else if (choice >= 1 && choice <= ucount) {
        strncpy(receiver, users[choice-1].username, MAX_STR);
    } else {
        printf(RED "❌ Invalid choice.\n" RESET);
        return;
    }

    printf(WHITE "Message: " RESET);
    char msg[MAX_MSG];
    getchar(); // flush newline
    fgets(msg, sizeof(msg), stdin);
    msg[strcspn(msg, "\n")] = 0; // strip newline

    printf(YELLOW "\n[THREAD-%-10s] 🔒 MUTEX LOCKED      — Entering critical section (message board)\n" RESET, sender);
    append_message(sender, receiver, msg);
    printf(GREEN "[THREAD-%-10s] ✅ MESSAGE SENT       — To: %s\n" RESET, sender, receiver);
    printf(YELLOW "[THREAD-%-10s] 🔓 MUTEX UNLOCKED    — Exiting critical section\n" RESET, sender);

    // Also send via TCP socket for real-time delivery
    socket_send_message(sender, receiver, msg);

    char detail[MAX_MSG];
    snprintf(detail, sizeof(detail), "To=%s Msg=%s", receiver, msg);
    log_incident(sender, "MESSAGE_SENT", detail);
    print_separator();
}

void view_messages(const char *username) {
    print_messages(username);
}

void broadcast_emergency(const char *sender, const char *alert) {
    printf(RED "\n[THREAD-%-10s] 🚨 EMERGENCY ALERT   — Broadcasting: %s\n" RESET, sender, alert);
    printf(RED "[SYSTEM        ] 📢 SEMAPHORE POST    — Waking all waiting crew threads\n" RESET);

    append_message(sender, "ALL", alert);
    log_incident(sender, "EMERGENCY_BROADCAST", alert);

    // Send emergency via TCP socket to all crew instantly
    socket_send_emergency(sender, alert);

    // Also send SIGUSR1 signal to all active crew processes
    printf(RED "[SIGNAL] 🚨 SIGUSR1 SENDING  — Emergency signal to all crew processes\n" RESET);
    signal_all_crew(SIGUSR1, alert);

    // Signal ALL resource semaphores unconditionally
    Resource dyn_res[MAX_RESOURCES];
    int dyn_count = 0;
    load_resources_silent(dyn_res, &dyn_count);
    num_resources = dyn_count;
    for (int i = 0; i < dyn_count; i++) {
        sem_post(resource_sem[i]);
        printf(YELLOW "[SYSTEM        ] 📢 SEMAPHORE POST    — Waking threads blocked on %s\n" RESET, dyn_res[i].resource);
    }
    fflush(stdout);
}
