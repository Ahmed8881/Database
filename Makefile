CC = gcc
CFLAGS = -Iinclude -Wall -Wextra -std=c11 -g
# Debug flags
ifdef DEBUG
		CFLAGS += -DDEBUG -O0
else
		CFLAGS += -O2
endif
SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin
SOURCES = $(wildcard $(SRC_DIR)/*.c) main.c
OBJECTS = $(SOURCES:%.c=$(OBJ_DIR)/%.o)
EXECUTABLE = $(BIN_DIR)/db-project

all: $(EXECUTABLE)
	@mkdir -p Database
$(EXECUTABLE): $(OBJECTS)
	@mkdir -p $(BIN_DIR)
	$(CC) $(OBJECTS) -o $@

$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(OBJ_DIR)/$(SRC_DIR)
	$(CC) $(CFLAGS) -c $< -o $@


clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR) __pycache__ .pytest_cache *.db Database
test:
	# -vv for verbose output
	python3 -m pytest -vv test_db.py

.PHONY: all clean
