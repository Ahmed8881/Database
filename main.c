#include <stdbool.h>
#include <stdint.h>
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

#pragma region Row

#define COLUMN_USERNAME_SIZE 32
#define COLUMN_EMAIL_SIZE 255
typedef struct {
  uint32_t id;
  char username[COLUMN_USERNAME_SIZE];
  char email[COLUMN_EMAIL_SIZE];
} Row;

#define size_of_attribute(Struct, Attribute) sizeof(((Struct *)0)->Attribute)

const uint32_t ID_SIZE = size_of_attribute(Row, id);
const uint32_t USERNAME_SIZE = size_of_attribute(Row, username);
const uint32_t EMAIL_SIZE = size_of_attribute(Row, email);
const uint32_t ID_OFFSET = 0;
const uint32_t USERNAME_OFFSET = ID_OFFSET + ID_SIZE;
const uint32_t EMAIL_OFFSET = USERNAME_OFFSET + USERNAME_SIZE;
const uint32_t ROW_SIZE = ID_SIZE + USERNAME_SIZE + EMAIL_SIZE;

void serialize_row(Row *source, void *destination) {
  memcpy(destination + ID_OFFSET, &(source->id), ID_SIZE);
  memcpy(destination + USERNAME_OFFSET, &(source->username), USERNAME_SIZE);
  memcpy(destination + EMAIL_OFFSET, &(source->email), EMAIL_SIZE);
}

void deserialize_row(void *source, Row *destination) {
  memcpy(&(destination->id), source + ID_OFFSET, ID_SIZE);
  memcpy(&(destination->username), source + USERNAME_OFFSET, USERNAME_SIZE);
  memcpy(&(destination->email), source + EMAIL_OFFSET, EMAIL_SIZE);
}

void print_row(Row *row) {
  printf("(%d, %s, %s)\n", row->id, row->username, row->email);
}

#pragma endregion Row

#pragma region Table

const uint32_t PAGE_SIZE = 4096;
#define TABLE_MAX_PAGES 100
const uint32_t ROWS_PER_PAGE = PAGE_SIZE / ROW_SIZE;
const uint32_t TABLE_MAX_ROWS = ROWS_PER_PAGE * TABLE_MAX_PAGES;

typedef struct {
  uint32_t num_rows;
  void *pages[TABLE_MAX_PAGES];
} Table;

void *row_slot(Table *table, uint32_t row_num) {
  uint32_t page_num = row_num / ROWS_PER_PAGE;
  void *page = table->pages[page_num];
  if (page == NULL) {
    // Only allocate memory when we try to access page
    page = table->pages[page_num] = malloc(PAGE_SIZE);
  }
  uint32_t row_offset = row_num % ROWS_PER_PAGE;
  uint32_t byte_offset = row_offset * ROW_SIZE;
  return page + byte_offset;
}

Table *new_table() {
  Table *table = (Table *)malloc(sizeof(Table));
  table->num_rows = 0;
  for (uint32_t i = 0; i < TABLE_MAX_PAGES; i++) {
    table->pages[i] = NULL;
  }
  return table;
}

void free_table(Table *table) {
  for (uint32_t i = 0; i < TABLE_MAX_PAGES; i++) {
    free(table->pages[i]);
  }
  free(table);
}

#pragma endregion Table

#pragma region commandProcessor

typedef enum {
  META_COMMAND_SUCCESS,
  META_COMMAND_UNRECOGNIZED_COMMAND
} MetaCommandResult;

typedef enum {
  PREPARE_SUCCESS,
  PREPARE_UNRECOGNIZED_STATEMENT,
  PREPARE_SYNTAX_ERROR
} PrepareResult;

typedef enum { EXECUTE_SUCCESS, EXECUTE_TABLE_FULL } ExecuteResult;

typedef enum { STATEMENT_INSERT, STATEMENT_SELECT } StatementType;

typedef struct {
  StatementType type;
  Row row_to_insert;
} Statement;

MetaCommandResult do_meta_command(Input_Buffer *buf) {
  if (strcmp(buf->buffer, ".exit") == 0) {
    printf("Exiting......\n");
    exit(EXIT_SUCCESS);
  } else {
    return META_COMMAND_UNRECOGNIZED_COMMAND;
  }
}

PrepareResult prepare_statement(Input_Buffer *buf, Statement *statement) {
  if (strncmp(buf->buffer, "insert", 6) == 0) {
    statement->type = STATEMENT_INSERT;
    int args_assigned = sscanf(
        buf->buffer, "insert %d %s %s", &(statement->row_to_insert.id),
        statement->row_to_insert.username, statement->row_to_insert.email);
    if (args_assigned < 3) {
      return PREPARE_SYNTAX_ERROR;
    }
    return PREPARE_SUCCESS;
  }
  if (strcmp(buf->buffer, "select") == 0) {
    statement->type = STATEMENT_SELECT;
    return PREPARE_SUCCESS;
  }

  return PREPARE_UNRECOGNIZED_STATEMENT;
}

ExecuteResult execute_insert(Statement *statement, Table *table) {
  if (table->num_rows >= TABLE_MAX_ROWS) {
    return EXECUTE_TABLE_FULL;
  }

  Row *row_to_insert = &(statement->row_to_insert);

  serialize_row(row_to_insert, row_slot(table, table->num_rows));
  table->num_rows++;

  return EXECUTE_SUCCESS;
}

ExecuteResult execute_select(Statement *statement, Table *table) {
  Row row;
  for (uint32_t i = 0; i < table->num_rows; i++) {
    deserialize_row(row_slot(table, i), &row);
    print_row(&row);
  }
  return EXECUTE_SUCCESS;
}

ExecuteResult execute_statement(Statement *statement, Table *table) {
  switch (statement->type) {
  case (STATEMENT_INSERT):
    return execute_insert(statement, table);
  case (STATEMENT_SELECT):
    return execute_select(statement, table);
  }
}
#pragma endregion commandProcessor

int main(int argc, char *argv[]) {
  Table *table = new_table();
  Input_Buffer *input_buf = newInputBuffer();
  while (true) {
    read_input(input_buf);
    if (input_buf->buffer[0] == '.') {
      switch (do_meta_command(input_buf)) {
      case META_COMMAND_SUCCESS:
        continue;
      case META_COMMAND_UNRECOGNIZED_COMMAND:
        printf("Unrecognized command %s\n", input_buf->buffer);
        continue;
      }
    } else {
      Statement statement;
      switch (prepare_statement(input_buf, &statement)) {
      case PREPARE_SUCCESS:
        break;
      case PREPARE_SYNTAX_ERROR:
        printf("Syntax error. Could not parse statement.\n");
        continue;
      case PREPARE_UNRECOGNIZED_STATEMENT:
        printf("Unrecognized keyword at the start of '%s'.\n",
               input_buf->buffer);
        continue;
      }
    }
  }
}
