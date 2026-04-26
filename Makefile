CC = gcc
CFLAGS = -Wall -Wextra -g
LDFLAGS = -lpthread

SRCS = main.c utils.c filestore.c auth.c resources.c deadlock.c comms.c
OBJS = $(SRCS:.c=.o)
TARGET = rigguard

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LDFLAGS)
	@echo "✅ Build successful! Run with: ./rigguard"

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
	@echo "🧹 Cleaned build files"

reset:
	rm -f data/*.csv data/.sem_init data/rigguard.lock
	@for name in rigguard_sem_0 rigguard_sem_1 rigguard_sem_2 rigguard_sem_3 rigguard_sem_4; do \
		rm -f /dev/shm/sem.$$name 2>/dev/null; \
	done
	@echo "🔄 Reset all CSV data and semaphores"

.PHONY: all clean reset

reset_sems:
	@for name in rigguard_sem_0 rigguard_sem_1 rigguard_sem_2 rigguard_sem_3 rigguard_sem_4; do \
		rm -f /dev/shm/sem.$$name 2>/dev/null; \
	done
	@echo "🔄 Named semaphores cleared from /dev/shm"
