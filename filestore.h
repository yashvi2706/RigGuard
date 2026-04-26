#ifndef FILESTORE_H
#define FILESTORE_H

#include "utils.h"

void init_data_files();
int  load_users(User users[], int *count);
void save_users(User users[], int count);
int  load_resources(Resource res[], int *count);
void save_resources(Resource res[], int count);
void append_message(const char *sender, const char *receiver, const char *msg);
void print_messages(const char *for_user);
void log_incident(const char *actor, const char *action, const char *detail);
void print_incidents();

// New: registration and equipment management
void register_crew(const char *admin_role);
void add_equipment(const char *admin_role);
void unlock_crew(const char *admin_role);

#endif
