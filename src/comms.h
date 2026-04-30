#ifndef COMMS_H
#define COMMS_H

#include "utils.h"
#include "filestore.h"

void send_message(const char *sender);
void view_messages(const char *username);
void broadcast_emergency(const char *sender, const char *alert);

#endif
