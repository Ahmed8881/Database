CC = gcc
CFLAGS = -Iinclude -Wall -Wextra -std=c11 -g
# Debug flags
ifdef DEBUG
		CFLAGS += -DDEBUG -O0
else
		CFLAGS += -O2
endif

CFLAGS += -Ivendor/

SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin
SRC_SOURCES = $(wildcard $(SRC_DIR)/*.c) vendor/cJSON/cJSON.c
SRC_OBJECTS = $(SRC_SOURCES:%.c=$(OBJ_DIR)/%.o)
CLI_EXECUTABLE = $(BIN_DIR)/db-cli
SERVER_EXECUTABLE = $(BIN_DIR)/db-server

all: $(CLI_EXECUTABLE) $(SERVER_EXECUTABLE)
	@mkdir -p Database

# CLI version (main.c + sources)
$(CLI_EXECUTABLE): $(SRC_OBJECTS) $(OBJ_DIR)/main.o
	@mkdir -p $(BIN_DIR)
	$(CC) $(SRC_OBJECTS) $(OBJ_DIR)/main.o -o $@

# Server version (test.c + sources)  
$(SERVER_EXECUTABLE): $(SRC_OBJECTS) $(OBJ_DIR)/test.o
	@mkdir -p $(BIN_DIR)
	$(CC) $(SRC_OBJECTS) $(OBJ_DIR)/test.o -o $@

$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@


clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR) __pycache__ .pytest_cache 

rmdb:
	rm -rf Database

test:
	# -vv for verbose output
	python3 -m pytest -vv tests/test_db.py

# Legacy compatibility - builds CLI version
legacy: $(CLI_EXECUTABLE)
	@ln -sf db-cli $(BIN_DIR)/db-project

.PHONY: all clean test legacy
