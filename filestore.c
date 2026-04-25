#include "filestore.h"
#include <sys/stat.h>
#include <sys/types.h>

// ─── Simple hash (djb2) ───────────────────────────────────────────────────────
static unsigned long hash_password(const char *str) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c;
    return hash % 99999999;
}

// ─── Init default CSV files if they don't exist ───────────────────────────────
void init_data_files() {
    FILE *f;

    // Create data/ directory if it doesn't exist
    mkdir("data", 0755);
    printf(GREEN "[SYSTEM] ✅ INIT — data/ directory ready\n" RESET);

    // users.csv
    f = fopen(FILE_USERS, "r");
    if (!f) {
        f = fopen(FILE_USERS, "w");
        fprintf(f, "username,password_hash,role,login_attempts,locked\n");
        fprintf(f, "commander,%lu,Rig_Commander,0,0\n",        hash_password("cmd123"));
        fprintf(f, "ravi,%lu,Fire_Safety_Officer,0,0\n",       hash_password("fire123"));
        fprintf(f, "arjun,%lu,Drilling_Operator,0,0\n",        hash_password("drill123"));
        fprintf(f, "meera,%lu,Maintenance_Engineer,0,0\n",     hash_password("maint123"));
        fprintf(f, "priya,%lu,Comms_Officer,0,0\n",            hash_password("comms123"));
        fclose(f);
        printf(GREEN "[SYSTEM] ✅ INIT — users.csv created with default crew\n" RESET);
    } else {
        fclose(f);
    }

    // resources.csv
    f = fopen(FILE_RESOURCES, "r");
    if (!f) {
        f = fopen(FILE_RESOURCES, "w");
        fprintf(f, "resource,held_by,status\n");
        fprintf(f, "Fire_Suppression,none,0\n");
        fprintf(f, "Emergency_Pump,none,0\n");
        fprintf(f, "Pressure_Valve,none,0\n");
        fprintf(f, "Power_Module,none,0\n");
        fprintf(f, "Comms_Terminal,none,0\n");
        fclose(f);
        printf(GREEN "[SYSTEM] ✅ INIT — resources.csv created\n" RESET);
    } else {
        fclose(f);
    }

    // messages.csv
    f = fopen(FILE_MESSAGES, "r");
    if (!f) {
        f = fopen(FILE_MESSAGES, "w");
        fprintf(f, "timestamp,sender,receiver,message\n");
        fclose(f);
        printf(GREEN "[SYSTEM] ✅ INIT — messages.csv created\n" RESET);
    } else {
        fclose(f);
    }

    // incidents.csv
    f = fopen(FILE_INCIDENTS, "r");
    if (!f) {
        f = fopen(FILE_INCIDENTS, "w");
        fprintf(f, "timestamp,actor,action,detail\n");
        fclose(f);
        printf(GREEN "[SYSTEM] ✅ INIT — incidents.csv created\n" RESET);
    } else {
        fclose(f);
    }
}

// ─── Load Users ───────────────────────────────────────────────────────────────
int load_users(User users[], int *count) {
    pthread_mutex_lock(&file_mutex);
    printf(YELLOW "[SYSTEM] 🔒 FILE MUTEX LOCKED  — Reading users.csv\n" RESET);

    FILE *f = fopen(FILE_USERS, "r");
    if (!f) { pthread_mutex_unlock(&file_mutex); return 0; }

    char line[512];
    *count = 0;
    fgets(line, sizeof(line), f); // skip header
    while (fgets(line, sizeof(line), f) && *count < MAX_USERS) {
        char hash_str[MAX_STR];
        sscanf(line, "%[^,],%[^,],%[^,],%d,%d",
            users[*count].username,
            hash_str,
            users[*count].role,
            &users[*count].login_attempts,
            &users[*count].locked);
        strncpy(users[*count].password, hash_str, MAX_STR);
        (*count)++;
    }
    fclose(f);

    printf(YELLOW "[SYSTEM] 🔓 FILE MUTEX UNLOCKED — Done reading users.csv\n" RESET);
    pthread_mutex_unlock(&file_mutex);
    return 1;
}

// ─── Save Users ───────────────────────────────────────────────────────────────
void save_users(User users[], int count) {
    pthread_mutex_lock(&file_mutex);
    printf(YELLOW "[SYSTEM] 🔒 FILE MUTEX LOCKED  — Writing users.csv\n" RESET);

    FILE *f = fopen(FILE_USERS, "w");
    fprintf(f, "username,password_hash,role,login_attempts,locked\n");
    for (int i = 0; i < count; i++) {
        fprintf(f, "%s,%s,%s,%d,%d\n",
            users[i].username,
            users[i].password,
            users[i].role,
            users[i].login_attempts,
            users[i].locked);
    }
    fclose(f);

    printf(YELLOW "[SYSTEM] 🔓 FILE MUTEX UNLOCKED — Done writing users.csv\n" RESET);
    pthread_mutex_unlock(&file_mutex);
}

// ─── Load Resources ───────────────────────────────────────────────────────────
int load_resources(Resource res[], int *count) {
    pthread_mutex_lock(&file_mutex);
    printf(YELLOW "[SYSTEM] 🔒 FILE MUTEX LOCKED  — Reading resources.csv\n" RESET);

    FILE *f = fopen(FILE_RESOURCES, "r");
    if (!f) { pthread_mutex_unlock(&file_mutex); return 0; }

    char line[256];
    *count = 0;
    fgets(line, sizeof(line), f); // skip header
    while (fgets(line, sizeof(line), f) && *count < MAX_RESOURCES) {
        sscanf(line, "%[^,],%[^,],%d",
            res[*count].resource,
            res[*count].held_by,
            &res[*count].status);
        (*count)++;
    }
    fclose(f);

    printf(YELLOW "[SYSTEM] 🔓 FILE MUTEX UNLOCKED — Done reading resources.csv\n" RESET);
    pthread_mutex_unlock(&file_mutex);
    return 1;
}

// ─── Save Resources ───────────────────────────────────────────────────────────
void save_resources(Resource res[], int count) {
    pthread_mutex_lock(&file_mutex);
    printf(YELLOW "[SYSTEM] 🔒 FILE MUTEX LOCKED  — Writing resources.csv\n" RESET);

    FILE *f = fopen(FILE_RESOURCES, "w");
    fprintf(f, "resource,held_by,status\n");
    for (int i = 0; i < count; i++) {
        fprintf(f, "%s,%s,%d\n", res[i].resource, res[i].held_by, res[i].status);
    }
    fclose(f);

    printf(YELLOW "[SYSTEM] 🔓 FILE MUTEX UNLOCKED — Done writing resources.csv\n" RESET);
    pthread_mutex_unlock(&file_mutex);
}

// ─── Append Message ───────────────────────────────────────────────────────────
void append_message(const char *sender, const char *receiver, const char *msg) {
    pthread_mutex_lock(&file_mutex);
    printf(YELLOW "[SYSTEM] 🔒 FILE MUTEX LOCKED  — Writing messages.csv\n" RESET);

    FILE *f = fopen(FILE_MESSAGES, "a");
    char ts[MAX_STR];
    get_timestamp(ts, sizeof(ts));
    fprintf(f, "%s,%s,%s,%s\n", ts, sender, receiver, msg);
    fclose(f);

    printf(YELLOW "[SYSTEM] 🔓 FILE MUTEX UNLOCKED — Done writing messages.csv\n" RESET);
    pthread_mutex_unlock(&file_mutex);
}

// ─── Print Messages ───────────────────────────────────────────────────────────
void print_messages(const char *for_user) {
    FILE *f = fopen(FILE_MESSAGES, "r");
    if (!f) { printf(RED "No messages found.\n" RESET); return; }

    char line[512];
    char ts[MAX_STR], sender[MAX_STR], receiver[MAX_STR], msg[MAX_MSG];
    int found = 0;

    print_separator();
    printf(CYAN "📋 CREW MESSAGE BOARD\n" RESET);
    print_separator();

    fgets(line, sizeof(line), f); // skip header
    while (fgets(line, sizeof(line), f)) {
        // Parse CSV line
        char tmp[512];
        strncpy(tmp, line, sizeof(tmp));
        char *tok = strtok(tmp, ",");
        if (!tok) continue; strncpy(ts, tok, MAX_STR);
        tok = strtok(NULL, ","); if (!tok) continue; strncpy(sender, tok, MAX_STR);
        tok = strtok(NULL, ","); if (!tok) continue; strncpy(receiver, tok, MAX_STR);
        tok = strtok(NULL, "\n"); if (!tok) continue; strncpy(msg, tok, MAX_MSG);

        // Show if addressed to this user or ALL
        if (strcmp(receiver, "ALL") == 0 ||
            strcmp(receiver, for_user) == 0 ||
            strcmp(sender, for_user) == 0) {
            printf(WHITE "[%s] " RESET, ts);
            printf(GREEN "%s" RESET " → " MAGENTA "%s" RESET ": %s\n", sender, receiver, msg);
            found = 1;
        }
    }
    if (!found) printf(YELLOW "No messages for you yet.\n" RESET);
    print_separator();
    fclose(f);
}

// ─── Log Incident ─────────────────────────────────────────────────────────────
void log_incident(const char *actor, const char *action, const char *detail) {
    pthread_mutex_lock(&file_mutex);

    FILE *f = fopen(FILE_INCIDENTS, "a");
    char ts[MAX_STR];
    get_timestamp(ts, sizeof(ts));
    fprintf(f, "%s,%s,%s,%s\n", ts, actor, action, detail);
    fclose(f);

    pthread_mutex_unlock(&file_mutex);
}

// ─── Print Incidents ──────────────────────────────────────────────────────────
void print_incidents() {
    FILE *f = fopen(FILE_INCIDENTS, "r");
    if (!f) { printf(RED "No incident logs found.\n" RESET); return; }

    char line[512];
    print_separator();
    printf(CYAN "📜 INCIDENT LOG\n" RESET);
    print_separator();

    fgets(line, sizeof(line), f); // skip header
    while (fgets(line, sizeof(line), f)) {
        // Color code by action type
        if (strstr(line, "DEADLOCK"))
            printf(RED "%s" RESET, line);
        else if (strstr(line, "RECOVERY") || strstr(line, "GRANTED"))
            printf(GREEN "%s" RESET, line);
        else if (strstr(line, "REQUESTED") || strstr(line, "SLEEPING"))
            printf(YELLOW "%s" RESET, line);
        else
            printf(WHITE "%s" RESET, line);
    }
    print_separator();
    fclose(f);
}
