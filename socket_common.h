#ifndef SOCKET_COMMON_H
#define SOCKET_COMMON_H

#include "utils.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#define SERVER_PORT     9000
#define SERVER_IP       "127.0.0.1"
#define MAX_CLIENTS     20
#define SOCK_BUF        512

// ─── Message types sent over socket ──────────────────────────────────────────
#define MSG_CHAT        "CHAT"       // normal message
#define MSG_BROADCAST   "BROADCAST"  // emergency broadcast
#define MSG_ALERT       "ALERT"      // deadlock alert from monitor thread
#define MSG_JOIN        "JOIN"       // crew member connected
#define MSG_LEAVE       "LEAVE"      // crew member disconnected
#define MSG_SERVER      "SERVER"     // server announcement

// ─── Socket packet structure ──────────────────────────────────────────────────
typedef struct {
    char type[16];        // MSG_CHAT, MSG_BROADCAST, etc.
    char sender[MAX_STR]; // who sent it
    char receiver[MAX_STR];// "ALL" or specific username
    char body[MAX_MSG];   // message content
    char timestamp[MAX_STR];
} Packet;

// ─── Global client socket (used by client to talk to server) ─────────────────
extern int client_sock;
extern char g_username[MAX_STR];
extern char g_role[MAX_STR];

// ─── Send a packet over socket ────────────────────────────────────────────────
static inline int send_packet(int sock, const char *type, const char *sender,
                               const char *receiver, const char *body) {
    Packet p;
    memset(&p, 0, sizeof(p));
    strncpy(p.type,     type,     sizeof(p.type)-1);
    strncpy(p.sender,   sender,   sizeof(p.sender)-1);
    strncpy(p.receiver, receiver, sizeof(p.receiver)-1);
    strncpy(p.body,     body,     sizeof(p.body)-1);
    get_timestamp(p.timestamp, sizeof(p.timestamp));
    return send(sock, &p, sizeof(p), 0);
}

#endif
