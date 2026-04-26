#include "utils.h"

pthread_mutex_t resource_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t file_mutex     = PTHREAD_MUTEX_INITIALIZER;
sem_t *resource_sem[MAX_RESOURCES];
int    num_resources = 0;

void get_timestamp(char *buf, int size) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(buf, size, "%Y-%m-%d %H:%M:%S", t);
}

void print_banner() {
    printf(CYAN);
    printf("\n╔══════════════════════════════════════════════════╗\n");
    printf("║   🛢️   RIGGUARD — OIL RIG EMERGENCY SYSTEM   🛢️  ║\n");
    printf("║      Concurrent Resource Management System       ║\n");
    printf("╚══════════════════════════════════════════════════╝\n");
    printf(RESET);
}

void print_separator() {
    printf(CYAN "──────────────────────────────────────────────────\n" RESET);
}

void clear_screen() { printf("\033[2J\033[H"); }

void color_print(const char *color, const char *msg) {
    printf("%s%s%s\n", color, msg, RESET);
}
