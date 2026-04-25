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
	rm -f data/*.csv
	@echo "🔄 Reset all CSV data files"

.PHONY: all clean reset
