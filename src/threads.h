#ifndef THREADS_H
#define THREADS_H

#include "utils.h"
#include "socket_common.h"

// Start background threads
void start_threads(const char *username, const char *role);
void stop_threads();

// Connect to server
int  connect_to_server(const char *username, const char *role);
void disconnect_from_server();

// Send message/broadcast via socket
void socket_send_message(const char *sender, const char *receiver, const char *msg);
void socket_send_emergency(const char *sender, const char *msg);

#endif
