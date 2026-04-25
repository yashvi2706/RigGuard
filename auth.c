#include "auth.h"

// Simple djb2 hash
static unsigned long hash_str(const char *str) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c;
    return hash % 99999999;
}

int login(char *username_out, char *role_out) {
    User users[MAX_USERS];
    int count = 0;

    print_banner();
    print_separator();
    printf(CYAN "🔐 CREW AUTHENTICATION\n" RESET);
    print_separator();

    // Default credentials hint
    printf(WHITE "Default Crew Credentials:\n" RESET);
    printf("  commander / cmd123    (Rig Commander)\n");
    printf("  ravi      / fire123   (Fire Safety Officer)\n");
    printf("  arjun     / drill123  (Drilling Operator)\n");
    printf("  meera     / maint123  (Maintenance Engineer)\n");
    printf("  priya     / comms123  (Comms Officer)\n");
    print_separator();

    char username[MAX_STR], password[MAX_STR];
    printf(WHITE "Username: " RESET);
    scanf("%s", username);

    load_users(users, &count);

    // Find user
    int idx = -1;
    for (int i = 0; i < count; i++) {
        if (strcmp(users[i].username, username) == 0) {
            idx = i;
            break;
        }
    }

    if (idx == -1) {
        printf(RED "❌ AUTH FAILED — User '%s' not found.\n" RESET, username);
        log_incident(username, "LOGIN_FAILED", "User not found");
        return 0;
    }

    // Check if locked
    if (users[idx].locked) {
        printf(RED "🔒 ACCOUNT LOCKED — Too many failed attempts. Contact Rig Commander.\n" RESET);
        log_incident(username, "LOGIN_BLOCKED", "Account locked");
        return 0;
    }

    // Password input (hidden with asterisks simulation)
    printf(WHITE "Password: " RESET);
    scanf("%s", password);

    unsigned long input_hash = hash_str(password);
    unsigned long stored_hash = strtoul(users[idx].password, NULL, 10);

    if (input_hash != stored_hash) {
        users[idx].login_attempts++;
        printf(RED "❌ AUTH FAILED — Wrong password. Attempt %d/%d\n" RESET,
               users[idx].login_attempts, MAX_LOGIN_TRIES);
        log_incident(username, "LOGIN_FAILED", "Wrong password");

        if (users[idx].login_attempts >= MAX_LOGIN_TRIES) {
            users[idx].locked = 1;
            printf(RED "🚨 ACCOUNT LOCKED — %s has been locked after %d failed attempts!\n" RESET,
                   username, MAX_LOGIN_TRIES);
            log_incident(username, "ACCOUNT_LOCKED", "Max attempts reached");
        }
        save_users(users, count);
        return 0;
    }

    // Successful login
    users[idx].login_attempts = 0;
    save_users(users, count);

    strncpy(username_out, users[idx].username, MAX_STR);
    strncpy(role_out, users[idx].role, MAX_STR);

    printf(GREEN "\n✅ AUTH SUCCESS — Welcome, %s!\n" RESET, username);
    printf(CYAN "   Role: %s\n" RESET, users[idx].role);
    log_incident(username, "LOGIN_SUCCESS", users[idx].role);

    return 1;
}

void logout(const char *username) {
    printf(YELLOW "\n[%s] 🚪 LOGGING OUT — Session ended.\n" RESET, username);
    log_incident(username, "LOGOUT", "Session ended");
}

int is_commander(const char *role) {
    return strcmp(role, "Rig_Commander") == 0;
}
