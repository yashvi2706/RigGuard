#ifndef RESOURCES_H
#define RESOURCES_H

#include "utils.h"
#include "filestore.h"

void init_semaphores();
void request_resource(const char *username, const char *role);
void release_resource(const char *username);
void view_allocation_table();

#endif
