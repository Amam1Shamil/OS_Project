# Compiler and Flags
CC = gcc
CFLAGS = -g -Wall -pthread
TSAN_FLAGS = -fsanitize=thread -g

# Source files for the server
SRC = server.c queue.c users.c concurrency.c
OBJ = $(SRC:.c=.o)
TARGET = server

# Source file for the client
CLIENT_SRC = client.c
CLIENT_TARGET = client

# Default target
all: $(TARGET) $(CLIENT_TARGET)

# Rule to build the server
$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ)

# Rule to build the client
$(CLIENT_TARGET): $(CLIENT_SRC)
	$(CC) $(CFLAGS) -o $(CLIENT_TARGET) $(CLIENT_SRC)

# Rule to build the server with ThreadSanitizer
tsan: CFLAGS += $(TSAN_FLAGS)
tsan: clean $(TARGET) $(CLIENT_TARGET)

# Generic rule for object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean rule
clean:
	rm -f $(TARGET) $(CLIENT_TARGET) $(OBJ)

# Phony targets
.PHONY: all clean tsan