CC = gcc
CFLAGS = -Wall -Wextra -g -Iinclude
LDFLAGS = -lrt -pthread

SRC_DIR = src
BIN_DIR = .

MAIN_SRC = $(SRC_DIR)/main.c

MM_SRC = $(SRC_DIR)/memory_manager.c $(SRC_DIR)/hash_table.c

PM_SRC = $(SRC_DIR)/pm.c

# Targets
all: main memory_manager pm

main: $(MAIN_SRC)
	$(CC) $(CFLAGS) -o $(BIN_DIR)/main $(MAIN_SRC) $(LDFLAGS)

memory_manager: $(MM_SRC)
	$(CC) $(CFLAGS) -o $(BIN_DIR)/memory_manager $(MM_SRC) $(LDFLAGS)

pm: $(PM_SRC)
	$(CC) $(CFLAGS) -o $(BIN_DIR)/pm $(PM_SRC) $(LDFLAGS)

clean:
	rm -f $(BIN_DIR)/main $(BIN_DIR)/memory_manager $(BIN_DIR)/pm