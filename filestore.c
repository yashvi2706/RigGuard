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

// ─── Init default CSV files ───────────────────────────────────────────────────
void init_data_files() {
    FILE *f;
    mkdir("data", 0755);
    printf(GREEN "[SYSTEM] ✅ INIT — data/ directory ready\n" RESET);

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
    } else fclose(f);

    f = fopen(FILE_RESOURCES, "r");
    if (!f) {
        f = fopen(FILE_RESOURCES, "w");
        fprintf(f, "resource,held_by,waited_by,status\n");
        fprintf(f, "Fire_Suppression,none,none,0\n");
        fprintf(f, "Emergency_Pump,none,none,0\n");
        fprintf(f, "Pressure_Valve,none,none,0\n");
        fprintf(f, "Power_Module,none,none,0\n");
        fprintf(f, "Comms_Terminal,none,none,0\n");
        fclose(f);
        printf(GREEN "[SYSTEM] ✅ INIT — resources.csv created\n" RESET);
    } else fclose(f);

    f = fopen(FILE_MESSAGES, "r");
    if (!f) { f = fopen(FILE_MESSAGES, "w"); fprintf(f, "timestamp,sender,receiver,message\n"); fclose(f); }
    else fclose(f);

    f = fopen(FILE_INCIDENTS, "r");
    if (!f) { f = fopen(FILE_INCIDENTS, "w"); fprintf(f, "timestamp,actor,action,detail\n"); fclose(f); }
    else fclose(f);
}

// ─── Load Users ───────────────────────────────────────────────────────────────
int load_users(User users[], int *count) {
    pthread_mutex_lock(&file_mutex);
    FILE *f = fopen(FILE_USERS, "r");
    if (!f) { pthread_mutex_unlock(&file_mutex); return 0; }
    char line[512];
    *count = 0;
    fgets(line, sizeof(line), f);
    while (fgets(line, sizeof(line), f) && *count < MAX_USERS) {
        char hash_str[MAX_STR];
        sscanf(line, "%[^,],%[^,],%[^,],%d,%d",
            users[*count].username, hash_str, users[*count].role,
            &users[*count].login_attempts, &users[*count].locked);
        strncpy(users[*count].password, hash_str, MAX_STR);
        (*count)++;
    }
    fclose(f);
    pthread_mutex_unlock(&file_mutex);
    return 1;
}

// ─── Save Users ───────────────────────────────────────────────────────────────
void save_users(User users[], int count) {
    pthread_mutex_lock(&file_mutex);
    FILE *f = fopen(FILE_USERS, "w");
    fprintf(f, "username,password_hash,role,login_attempts,locked\n");
    for (int i = 0; i < count; i++)
        fprintf(f, "%s,%s,%s,%d,%d\n", users[i].username, users[i].password,
                users[i].role, users[i].login_attempts, users[i].locked);
    fclose(f);
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
    fgets(line, sizeof(line), f);
    while (fgets(line, sizeof(line), f) && *count < MAX_RESOURCES) {
        sscanf(line, "%[^,],%[^,],%[^,],%d",
            res[*count].resource, res[*count].held_by,
            res[*count].waited_by, &res[*count].status);
        (*count)++;
    }
    fclose(f);
    printf(YELLOW "[SYSTEM] 🔓 FILE MUTEX UNLOCKED — Done reading resources.csv\n" RESET);
    pthread_mutex_unlock(&file_mutex);
    return 1;
}

// ─── Silent version — no mutex print (used in background/menu contexts) ───────
int load_resources_silent(Resource res[], int *count) {
    pthread_mutex_lock(&file_mutex);
    FILE *f = fopen(FILE_RESOURCES, "r");
    if (!f) { pthread_mutex_unlock(&file_mutex); return 0; }
    char line[256];
    *count = 0;
    fgets(line, sizeof(line), f);
    while (fgets(line, sizeof(line), f) && *count < MAX_RESOURCES) {
        sscanf(line, "%[^,],%[^,],%[^,],%d",
            res[*count].resource, res[*count].held_by,
            res[*count].waited_by, &res[*count].status);
        (*count)++;
    }
    fclose(f);
    pthread_mutex_unlock(&file_mutex);
    return 1;
}

// ─── Save Resources ───────────────────────────────────────────────────────────
void save_resources(Resource res[], int count) {
    pthread_mutex_lock(&file_mutex);
    printf(YELLOW "[SYSTEM] 🔒 FILE MUTEX LOCKED  — Writing resources.csv\n" RESET);
    FILE *f = fopen(FILE_RESOURCES, "w");
    fprintf(f, "resource,held_by,waited_by,status\n");
    for (int i = 0; i < count; i++)
        fprintf(f, "%s,%s,%s,%d\n", res[i].resource, res[i].held_by,
                res[i].waited_by, res[i].status);
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
    char line[512], ts[MAX_STR], sender[MAX_STR], receiver[MAX_STR], msg[MAX_MSG];
    int found = 0;
    print_separator();
    printf(CYAN "📋 CREW MESSAGE BOARD\n" RESET);
    print_separator();
    fgets(line, sizeof(line), f);
    while (fgets(line, sizeof(line), f)) {
        char tmp[512];
        strncpy(tmp, line, sizeof(tmp));
        char *tok = strtok(tmp, ",");
        if (!tok) continue; strncpy(ts, tok, MAX_STR);
        tok = strtok(NULL, ","); if (!tok) continue; strncpy(sender, tok, MAX_STR);
        tok = strtok(NULL, ","); if (!tok) continue; strncpy(receiver, tok, MAX_STR);
        tok = strtok(NULL, "\n"); if (!tok) continue; strncpy(msg, tok, MAX_MSG);
        if (strcmp(receiver, "ALL") == 0 || strcmp(receiver, for_user) == 0 || strcmp(sender, for_user) == 0) {
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
    fgets(line, sizeof(line), f);
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, "DEADLOCK"))      printf(RED "%s" RESET, line);
        else if (strstr(line, "RECOVERY") || strstr(line, "GRANTED")) printf(GREEN "%s" RESET, line);
        else if (strstr(line, "BLOCKED"))  printf(YELLOW "%s" RESET, line);
        else                               printf(WHITE "%s" RESET, line);
    }
    print_separator();
    fclose(f);
}

// ─── Register New Crew Member ─────────────────────────────────────────────────
void register_crew(const char *admin_role) {
    print_separator();
    printf(CYAN "👤 REGISTER NEW CREW MEMBER\n" RESET);
    print_separator();

    // Only commander can register
    if (strcmp(admin_role, "Rig_Commander") != 0) {
        printf(RED "❌ ACCESS DENIED — Only Rig Commander can register new crew!\n" RESET);
        return;
    }

    User users[MAX_USERS];
    int count = 0;
    load_users(users, &count);

    if (count >= MAX_USERS) {
        printf(RED "❌ MAX CREW REACHED — Cannot add more crew members.\n" RESET);
        return;
    }

    char username[MAX_STR], password[MAX_STR];
    printf(WHITE "New crew username: " RESET);
    scanf("%s", username);

    // Check if username already exists
    for (int i = 0; i < count; i++) {
        if (strcmp(users[i].username, username) == 0) {
            printf(RED "❌ USERNAME TAKEN — '%s' already exists!\n" RESET, username);
            return;
        }
    }

    printf(WHITE "Password for %s: " RESET, username);
    scanf("%s", password);

    // Show roles
    printf(WHITE "\nAssign Role:\n" RESET);
    for (int i = 0; i < NUM_ROLES; i++)
        printf("  %d. %s\n", i+1, ROLES[i]);
    printf(WHITE "Role choice (1-%d): " RESET, NUM_ROLES);
    int role_choice;
    scanf("%d", &role_choice);
    if (role_choice < 1 || role_choice > NUM_ROLES) {
        printf(RED "❌ Invalid role choice.\n" RESET);
        return;
    }

    // Add new user
    strncpy(users[count].username, username, MAX_STR);
    unsigned long hash = 5381;
    for (int i = 0; password[i]; i++) hash = ((hash << 5) + hash) + password[i];
    hash = hash % 99999999;
    snprintf(users[count].password, MAX_STR, "%lu", hash);
    strncpy(users[count].role, ROLES[role_choice-1], MAX_STR);
    users[count].login_attempts = 0;
    users[count].locked = 0;
    count++;

    save_users(users, count);

    printf(GREEN "\n✅ CREW REGISTERED  — %s joined as %s\n" RESET, username, ROLES[role_choice-1]);
    printf(GREEN "   They can now login with their credentials.\n" RESET);

    char detail[MAX_MSG];
    snprintf(detail, sizeof(detail), "New crew: %s Role: %s", username, ROLES[role_choice-1]);
    log_incident("commander", "CREW_REGISTERED", detail);
    print_separator();
}

// ─── Add New Equipment (Commander only) ──────────────────────────────────────
void add_equipment(const char *admin_role) {
    print_separator();
    printf(CYAN "🔧 ADD NEW EQUIPMENT\n" RESET);
    print_separator();

    if (strcmp(admin_role, "Rig_Commander") != 0) {
        printf(RED "❌ ACCESS DENIED — Only Rig Commander can add new equipment!\n" RESET);
        return;
    }

    Resource res[MAX_RESOURCES];
    int rcount = 0;
    load_resources(res, &rcount);

    if (rcount >= MAX_RESOURCES) {
        printf(RED "❌ MAX EQUIPMENT REACHED — Cannot add more.\n" RESET);
        return;
    }

    char eq_name[MAX_STR];
    printf(WHITE "New equipment name (no spaces, use _ ): " RESET);
    scanf("%s", eq_name);

    // Check duplicate
    for (int i = 0; i < rcount; i++) {
        if (strcmp(res[i].resource, eq_name) == 0) {
            printf(RED "❌ EQUIPMENT EXISTS — '%s' already in system!\n" RESET, eq_name);
            return;
        }
    }

    // Add to resources.csv
    strncpy(res[rcount].resource,   eq_name, MAX_STR);
    strncpy(res[rcount].held_by,    "none",  MAX_STR);
    strncpy(res[rcount].waited_by,  "none",  MAX_STR);
    res[rcount].status = 0;
    rcount++;

    save_resources(res, rcount);

    // Open a new named semaphore for this resource
    char sem_name[MAX_STR];
    snprintf(sem_name, sizeof(sem_name), "/rigguard_sem_%d", rcount-1);
    sem_unlink(sem_name);
    resource_sem[rcount-1] = sem_open(sem_name, O_CREAT, 0666, 0);
    if (resource_sem[rcount-1] == SEM_FAILED) {
        printf(RED "❌ SEMAPHORE FAILED — Could not create semaphore for %s\n" RESET, eq_name);
    } else {
        printf(GREEN "📢 SEMAPHORE CREATED — %s semaphore initialized\n" RESET, eq_name);
    }
    num_resources = rcount;

    printf(GREEN "\n✅ EQUIPMENT ADDED  — %s is now available for crew\n" RESET, eq_name);
    printf(GREEN "   All crew can now request this equipment.\n" RESET);

    char detail[MAX_MSG];
    snprintf(detail, sizeof(detail), "New equipment: %s", eq_name);
    log_incident("commander", "EQUIPMENT_ADDED", detail);
    print_separator();
}

// ─── Unlock Locked Crew Member (Commander only) ───────────────────────────────
void unlock_crew(const char *admin_role) {
    print_separator();
    printf(CYAN "🔓 UNLOCK CREW ACCOUNT\n" RESET);
    print_separator();

    if (strcmp(admin_role, "Rig_Commander") != 0) {
        printf(RED "❌ ACCESS DENIED — Only Rig Commander can unlock accounts!\n" RESET);
        return;
    }

    User users[MAX_USERS];
    int count = 0;
    load_users(users, &count);

    printf(WHITE "Locked accounts:\n" RESET);
    int found = 0;
    for (int i = 0; i < count; i++) {
        if (users[i].locked) {
            printf(RED "  - %s (%s)\n" RESET, users[i].username, users[i].role);
            found = 1;
        }
    }
    if (!found) { printf(YELLOW "No locked accounts.\n" RESET); return; }

    printf(WHITE "Username to unlock: " RESET);
    char uname[MAX_STR];
    scanf("%s", uname);

    for (int i = 0; i < count; i++) {
        if (strcmp(users[i].username, uname) == 0 && users[i].locked) {
            users[i].locked = 0;
            users[i].login_attempts = 0;
            save_users(users, count);
            printf(GREEN "✅ ACCOUNT UNLOCKED — %s can now login.\n" RESET, uname);
            log_incident("commander", "ACCOUNT_UNLOCKED", uname);
            return;
        }
    }
    printf(RED "❌ User not found or not locked.\n" RESET);
    print_separator();
}
