CC      = gcc
CFLAGS  = -Wall -Wextra -g
LDFLAGS = -lpthread

# Client sources
CLIENT_SRCS = main.c utils.c filestore.c auth.c resources.c deadlock.c comms.c threads.c signals.c
CLIENT_OBJS = $(CLIENT_SRCS:.c=.o)

# Server sources
SERVER_SRCS = server.c utils.c filestore.c
SERVER_OBJS = $(SERVER_SRCS:.c=.o)

all: rigguard rigguard_server

rigguard: $(CLIENT_OBJS)
	$(CC) $(CFLAGS) -o rigguard $(CLIENT_OBJS) $(LDFLAGS)
	@echo "✅ rigguard (client) built!"

rigguard_server: server.o utils.o filestore.o
	$(CC) $(CFLAGS) -o rigguard_server server.o utils.o filestore.o $(LDFLAGS)
	@echo "✅ rigguard_server built!"

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(CLIENT_OBJS) server.o rigguard rigguard_server
	@echo "🧹 Cleaned!"

reset:
	rm -f data/*.csv data/.sem_init data/rigguard.lock
	@for name in $$(seq 0 19); do rm -f /dev/shm/sem.rigguard_sem_$$name 2>/dev/null; done
	@echo "🔄 Reset data and semaphores"

.PHONY: all clean reset
