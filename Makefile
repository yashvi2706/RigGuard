CC      = gcc
CFLAGS  = -Wall -Wextra -g
LDFLAGS = -lpthread

# Source directory
SRC = src

# Client sources
CLIENT_SRCS = $(SRC)/main.c $(SRC)/utils.c $(SRC)/filestore.c $(SRC)/auth.c \
              $(SRC)/resources.c $(SRC)/deadlock.c $(SRC)/comms.c \
              $(SRC)/threads.c $(SRC)/signals.c

# Server sources
SERVER_SRCS = $(SRC)/server.c $(SRC)/utils.c $(SRC)/filestore.c

all: rigguard rigguard_server
	@echo ""
	@echo "✅ Build complete!"
	@echo "   Run server first:  ./rigguard_server"
	@echo "   Then run clients:  ./rigguard"

rigguard: $(CLIENT_SRCS)
	$(CC) $(CFLAGS) -o rigguard $(CLIENT_SRCS) $(LDFLAGS)
	@echo "✅ rigguard (client) built!"

rigguard_server: $(SERVER_SRCS)
	$(CC) $(CFLAGS) -o rigguard_server $(SERVER_SRCS) $(LDFLAGS)
	@echo "✅ rigguard_server built!"

clean:
	rm -f rigguard rigguard_server
	@echo "🧹 Cleaned binaries"

reset:
	rm -f data/*.csv data/.sem_init data/rigguard.lock
	rm -rf data/pids
	@for i in $$(seq 0 19); do rm -f /dev/shm/sem.rigguard_sem_$$i 2>/dev/null; done
	@echo "🔄 Reset all data and semaphores"

.PHONY: all clean reset
