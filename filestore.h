#ifndef FILESTORE_H
#define FILESTORE_H

#include "utils.h"

// User file operations
void     init_data_files();
int      load_users(User users[], int *count);
void     save_users(User users[], int count);

// Resource file operations
int      load_resources(Resource res[], int *count);
void     save_resources(Resource res[], int count);

// Message operations
void     append_message(const char *sender, const char *receiver, const char *msg);
void     print_messages(const char *for_user);

// Incident log
void     log_incident(const char *actor, const char *action, const char *detail);
void     print_incidents();

#endif
