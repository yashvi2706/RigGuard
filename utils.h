#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/stat.h>

// ─── Constants ───────────────────────────────────────────────────────────────
#define MAX_USERS       20
#define MAX_RESOURCES   20
#define MAX_MESSAGES    100
#define MAX_INCIDENTS   200
#define MAX_STR         64
#define MAX_MSG         256
#define MAX_LOGIN_TRIES 3

// ─── File Paths ───────────────────────────────────────────────────────────────
#define FILE_USERS      "data/users.csv"
#define FILE_RESOURCES  "data/resources.csv"
#define FILE_MESSAGES   "data/messages.csv"
#define FILE_INCIDENTS  "data/incidents.csv"

// ─── Roles ────────────────────────────────────────────────────────────────────
#define NUM_ROLES 5
static const char *ROLES[NUM_ROLES] = {
    "Rig_Commander",
    "Fire_Safety_Officer",
    "Drilling_Operator",
    "Maintenance_Engineer",
    "Comms_Officer"
};

// ─── Structs ──────────────────────────────────────────────────────────────────
typedef struct {
    char username[MAX_STR];
    char password[MAX_STR];
    char role[MAX_STR];
    int  login_attempts;
    int  locked;
} User;

typedef struct {
    char resource[MAX_STR];
    char held_by[MAX_STR];
    char waited_by[MAX_STR];
    int  status;             // 0=free, 1=allocated
} Resource;

typedef struct {
    char timestamp[MAX_STR];
    char sender[MAX_STR];
    char receiver[MAX_STR];
    char message[MAX_MSG];
} Message;

// ─── Global Mutex ────────────────────────────────────────────────────────────
extern pthread_mutex_t resource_mutex;
extern pthread_mutex_t file_mutex;

// Dynamic semaphore array (supports up to MAX_RESOURCES)
extern sem_t *resource_sem[MAX_RESOURCES];
extern int    num_resources; // actual count loaded from CSV

// ─── Utility Functions ────────────────────────────────────────────────────────
void get_timestamp(char *buf, int size);
void print_banner();
void print_separator();
void clear_screen();
void color_print(const char *color, const char *msg);

// Colors
#define RED     "\033[1;31m"
#define GREEN   "\033[1;32m"
#define YELLOW  "\033[1;33m"
#define CYAN    "\033[1;36m"
#define MAGENTA "\033[1;35m"
#define WHITE   "\033[1;37m"
#define RESET   "\033[0m"

#endif
