#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma region InputHandling
typedef struct {
  char *buffer;
  size_t buffer_length;
  ssize_t input_length;
} Input_Buffer;

Input_Buffer *newInputBuffer() {
  Input_Buffer *buf = (Input_Buffer *)malloc(sizeof(Input_Buffer));
  buf->buffer = NULL;
  buf->buffer_length = buf->input_length = 0;
  return buf;
}

void print_prompt() { printf("sqlite > "); }

void read_input(Input_Buffer *buf) {
  size_t buffer_size = 1024;
  buf->buffer = (char *)malloc(buffer_size);
  if (buf->buffer == NULL) {
    perror("Unable to allocate buffer");
    exit(EXIT_FAILURE);
  }

  print_prompt();

  size_t position = 0;
  int c;

  while (true) {
    c = fgetc(stdin);
    if (c == '\n' || c == EOF) {
      buf->buffer[position] = '\0';
      buf->input_length = position;
      break;
    } else {
      buf->buffer[position] = c;
    }
    position++;

    if (position >= buffer_size) {
      // double the buffer size
      buffer_size *= 2;
      buf->buffer = (char *)realloc(buf->buffer, buffer_size);
      if (buf->buffer == NULL) {
        perror("Unable to reallocate buffer");
        exit(EXIT_FAILURE);
      }
    }
  }
}

void free_input_buffer(Input_Buffer *buf) {
  free(buf->buffer);
  free(buf);
}
#pragma endregion InputHandling

#pragma region commandProcessor
typedef enum {
  META_COMMAND_SUCCESS,
  META_COMMAND_UNRECOGNIZED_COMMAND
} MetaCommandResult;

typedef enum { PREPARE_SUCCESS, PREPARE_UNRECOGNIZED_STATEMENT } PrepareResult;

typedef enum { STATEMENT_INSERT, STATEMENT_SELECT } StatementType;

typedef struct {
  StatementType type;
} Statement;

MetaCommandResult do_meta_command(Input_Buffer *buf) {
  if (strcmp(buf->buffer, ".exit") == 0) {
    printf("Exiting......");
    exit(EXIT_SUCCESS);
  } else {
    return META_COMMAND_UNRECOGNIZED_COMMAND;
  }
}

PrepareResult prepare_statement(Input_Buffer *buf, Statement *statement) {
  if (strncmp(buf->buffer, "insert", 6) == 0) {
    statement->type = STATEMENT_INSERT;
    return PREPARE_SUCCESS;
  }
  if (strcmp(buf->buffer, "select") == 0) {
    statement->type = STATEMENT_SELECT;
    return PREPARE_SUCCESS;
  }

  return PREPARE_UNRECOGNIZED_STATEMENT;
}

void execute_statement(Statement *statement) {
  switch (statement->type) {
  case (STATEMENT_INSERT):
    printf("This is where we would do an insert.\n");
    break;
  case (STATEMENT_SELECT):
    printf("This is where we would do a select.\n");
    break;
  }
}
#pragma endregion commandProcessor

int main(int argc, char *argv[]) {
  Input_Buffer *input_buf = newInputBuffer();
  while (true) {
    read_input(input_buf);
    if (input_buf->buffer[0] == ".") {
      switch (do_meta_command(input_buf)) {
      case META_COMMAND_SUCCESS:
        continue;
      case META_COMMAND_UNRECOGNIZED_COMMAND:
        printf("Unrecognized command %s\n", input_buf->buffer);
        continue;
      }
    } else {
      Statement statement;
      PrepareResult result = prepare_statement(input_buf, &statement);
      switch (result) {
      case PREPARE_SUCCESS:
        execute_statement(&statement);
        continue;

      case PREPARE_UNRECOGNIZED_STATEMENT:
        printf("Unrecognized command %s\n", input_buf->buffer);
        continue;
      }
    }
  }
}
