#include "utils.h"
#include "filestore.h"
#include "auth.h"
#include "resources.h"
#include "deadlock.h"
#include "comms.h"

void show_menu(const char *username, const char *role) {
    printf(CYAN "\n╔══════════════════════════════════════════════════╗\n" RESET);
    printf(CYAN "║   🛢️   RIGGUARD — OIL RIG EMERGENCY SYSTEM       ║\n" RESET);
    printf(CYAN "╠══════════════════════════════════════════════════╣\n" RESET);
    printf(CYAN "║  " WHITE "Operator : %-38s" CYAN "║\n" RESET, username);
    printf(CYAN "║  " WHITE "Role     : %-38s" CYAN "║\n" RESET, role);
    printf(CYAN "╠══════════════════════════════════════════════════╣\n" RESET);
    printf(CYAN "║  " WHITE "1. 📋 View Crew Message Board              " CYAN "║\n" RESET);
    printf(CYAN "║  " WHITE "2. ✉️  Send Message to Crew                 " CYAN "║\n" RESET);
    printf(CYAN "║  " WHITE "3. 🔧 Request Critical Equipment           " CYAN "║\n" RESET);
    printf(CYAN "║  " WHITE "4. 🔓 Release Equipment                    " CYAN "║\n" RESET);
    printf(CYAN "║  " WHITE "5. 📊 View Equipment Allocation Table      " CYAN "║\n" RESET);
    printf(CYAN "║  " WHITE "6. ⚠️  Run Deadlock Detection               " CYAN "║\n" RESET);
    printf(CYAN "║  " WHITE "7. 🚨 Trigger Emergency Broadcast          " CYAN "║\n" RESET);
    printf(CYAN "║  " WHITE "8. 📜 View Incident Logs                   " CYAN "║\n" RESET);
    printf(CYAN "║  " WHITE "9. 🚪 Logout                               " CYAN "║\n" RESET);
    printf(CYAN "╚══════════════════════════════════════════════════╝\n" RESET);
    printf(WHITE "Choice: " RESET);
}

int main() {
    // Initialize
    printf(CYAN "[SYSTEM] 🚀 BOOT             — RigGuard initializing...\n" RESET);
    init_data_files();
    init_semaphores();
    printf(GREEN "[SYSTEM] ✅ READY            — All systems operational\n\n" RESET);

    char username[MAX_STR], role[MAX_STR];

    // Login loop
    int logged_in = 0;
    int attempts = 0;
    while (!logged_in && attempts < 3) {
        logged_in = login(username, role);
        if (!logged_in) {
            attempts++;
            if (attempts < 3) {
                printf(YELLOW "Try again (%d/3)...\n\n" RESET, attempts);
            }
        }
    }

    if (!logged_in) {
        printf(RED "❌ Too many failed login attempts. Exiting.\n" RESET);
        return 1;
    }

    // Main menu loop
    int running = 1;
    while (running) {
        show_menu(username, role);

        int choice;
        scanf("%d", &choice);

        switch (choice) {
            case 1:
                view_messages(username);
                break;

            case 2:
                send_message(username);
                break;

            case 3:
                request_resource(username, role);
                break;

            case 4:
                release_resource(username);
                break;

            case 5:
                view_allocation_table();
                break;

            case 6:
                run_deadlock_check(username);
                break;

            case 7: {
                char alert[MAX_MSG];
                printf(WHITE "Emergency Alert Message: " RESET);
                getchar();
                fgets(alert, sizeof(alert), stdin);
                alert[strcspn(alert, "\n")] = 0;
                broadcast_emergency(username, alert);
                break;
            }

            case 8:
                print_incidents();
                break;

            case 9:
                logout(username);
                running = 0;
                break;

            default:
                printf(RED "❌ Invalid choice. Try again.\n" RESET);
        }

        if (running) {
            printf(WHITE "\nPress Enter to continue..." RESET);
            getchar(); getchar();
        }
    }

    // Cleanup semaphores
    for (int i = 0; i < NUM_RESOURCES; i++)
        sem_destroy(&resource_sem[i]);

    printf(GREEN "\n[SYSTEM] 🛢️  SHUTDOWN        — RigGuard session ended safely.\n" RESET);
    return 0;
}
