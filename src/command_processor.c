#include "../include/command_processor.h"
#include "../include/btree.h"
#include "../include/cursor.h"
#include "../include/table.h"
#include <stdint.h>
#include <string.h>

void print_constants()
{
  printf("ROW_SIZE: %d\n", ROW_SIZE);
  printf("COMMON_NODE_HEADER_SIZE: %ld\n", COMMON_NODE_HEADER_SIZE);
  printf("LEAF_NODE_HEADER_SIZE: %ld\n", LEAF_NODE_HEADER_SIZE);
  printf("LEAF_NODE_CELL_SIZE: %ld\n", LEAF_NODE_CELL_SIZE);
  printf("LEAF_NODE_SPACE_FOR_CELLS: %ld\n", LEAF_NODE_SPACE_FOR_CELLS);
  printf("LEAF_NODE_MAX_CELLS: %ld\n", LEAF_NODE_MAX_CELLS);
}

void indent(uint32_t level)
{
  for (uint32_t i = 0; i < level; i++)
  {
    printf("  ");
  }
}

MetaCommandResult do_meta_command(Input_Buffer *buf, Table *table)
{
  if (strcmp(buf->buffer, ".exit") == 0)
  {
    db_close(table);
    exit(EXIT_SUCCESS);
  }
  else if (strcmp(buf->buffer, ".btree") == 0)
  {
    printf("Tree:\n");
    print_tree(table->pager, 0, 0);
    return META_COMMAND_SUCCESS;
  }
  else if (strcmp(buf->buffer, ".constants") == 0)
  {
    printf("Constants:\n");
    print_constants();
    return META_COMMAND_SUCCESS;
  }

  else
  {
    return META_COMMAND_UNRECOGNIZED_COMMAND;
  }
}

PrepareResult prepare_insert(Input_Buffer *buf, Statement *statement)
{
  statement->type = STATEMENT_INSERT;

  char *keyword = strtok(buf->buffer, " ");
  char *id_string = strtok(NULL, " ");
  char *username = strtok(NULL, " ");
  char *email = strtok(NULL, " ");

  if (id_string == NULL || username == NULL || email == NULL)
  {
    return PREPARE_SYNTAX_ERROR;
  }

  int id = atoi(id_string);
  if (id < 0)
  {
    return PREPARE_NEGATIVE_ID;
  }

  if (strlen(username) > COLUMN_USERNAME_SIZE ||
      strlen(email) > COLUMN_EMAIL_SIZE)
  {
    return PREPARE_STRING_TOO_LONG;
  }

  statement->row_to_insert.id = id;
  strcpy(statement->row_to_insert.username, username);
  strcpy(statement->row_to_insert.email, email);

  return PREPARE_SUCCESS;
}

PrepareResult prepare_statement(Input_Buffer *buf, Statement *statement)
{
  if (strncmp(buf->buffer, "insert", 6) == 0)
  {
    return prepare_insert(buf, statement);
  }
  if (strcmp(buf->buffer, "select") == 0)
  {
    statement->type = STATEMENT_SELECT;
    return PREPARE_SUCCESS;
  }
  if (strncmp(buf->buffer, "select where id = ", 18) == 0)
  {
    statement->type = STATEMENT_SELECT_BY_ID;
    statement->id_to_select = atoi(buf->buffer + 18);
    return PREPARE_SUCCESS;
  }
  if (strncmp(buf->buffer, "update ", 7) == 0)
  {
    statement->type = STATEMENT_UPDATE;
    char *id_string = strtok(buf->buffer + 7, " ");
    statement->id_to_update = atoi(id_string);
    char *field = strtok(NULL, " ");
    char *value = strtok(NULL, " ");
    if (field == NULL || value == NULL)
    {
      return PREPARE_SYNTAX_ERROR;
    }
    if (strcmp(field, "username") == 0)
    {
      if (strlen(value) > COLUMN_USERNAME_SIZE)
      {
        return PREPARE_STRING_TOO_LONG;
      }
      strcpy(statement->row_to_insert.username, value);
      strcpy(statement->row_to_insert.email, "");
    }
    else if (strcmp(field, "email") == 0)
    {
      if (strlen(value) > COLUMN_EMAIL_SIZE)
      {
        return PREPARE_STRING_TOO_LONG;
      }
      strcpy(statement->row_to_insert.email, value);
      strcpy(statement->row_to_insert.username, "");
    }
    else
    {
      return PREPARE_SYNTAX_ERROR;
    }

    return PREPARE_SUCCESS;
  }
  if (strncmp(buf->buffer, "delete where id = ", 18) == 0)
  {
    statement->type = STATEMENT_DELETE;
    statement->id_to_delete = atoi(buf->buffer + 18);
    return PREPARE_SUCCESS;
  }

  return PREPARE_UNRECOGNIZED_STATEMENT;
}

ExecuteResult execute_insert(Statement *statement, Table *table)
{
  void *node = get_page(table->pager, table->root_page_num);
  uint32_t num_cells = *leaf_node_num_cells(node);

  Row *row_to_insert = &(statement->row_to_insert);
  uint32_t key_to_insert = row_to_insert->id;
  Cursor *cursor = table_find(table, key_to_insert);
  if (cursor->cell_num < num_cells)
  {
    // if key already exists
    uint32_t key_at_index = *leaf_node_key(node, cursor->cell_num);
    if (key_at_index == key_to_insert)
    {
      free(cursor);
      return EXECUTE_DUPLICATE_KEY;
    }
  }

  leaf_node_insert(cursor, row_to_insert->id, row_to_insert);
  free(cursor);

  return EXECUTE_SUCCESS;
}

ExecuteResult execute_update(Statement *statement, Table *table)
{
  Cursor *cursor = table_find(table, statement->id_to_update);
  if (cursor->end_of_table)
  {
    printf("Record not found.\n");
    free(cursor);
    return EXECUTE_SUCCESS;
  }
  Row row;
  deserialize_row(cursor_value(cursor), &row);

  if (strlen(statement->row_to_insert.username) > 0)
  {
    strcpy(row.username, statement->row_to_insert.username);
  }
  if (strlen(statement->row_to_insert.email) > 0)
  {
    strcpy(row.email, statement->row_to_insert.email);
  }

  serialize_row(&row, cursor_value(cursor));
  free(cursor);
  return EXECUTE_SUCCESS;
}

ExecuteResult execute_select(Statement *statement, Table *table)
{
  Cursor *cursor = table_start(table);
  // just to silence unused variable warning
  if (statement)
  {
  }
  Row row;
  while (!cursor->end_of_table)
  {
    deserialize_row(cursor_value(cursor), &row);
    print_row(&row);
    cursor_advance(cursor);
  }
  free(cursor);

  return EXECUTE_SUCCESS;
}

ExecuteResult execute_select_by_id(Statement *statement, Table *table)
{
  Cursor *cursor = table_find(table, statement->id_to_select);
  Row row;
  if (cursor->end_of_table)
  {
    printf("Record not found.\n");
    free(cursor);
    return EXECUTE_SUCCESS;
  }
  deserialize_row(cursor_value(cursor), &row);
  print_row(&row);
  free(cursor);
  return EXECUTE_SUCCESS;
}

ExecuteResult execute_delete(Statement *statement, Table *table)
{
  Cursor *cursor = table_find(table, statement->id_to_delete);
  if (cursor->end_of_table)
  {
    printf("Record not found.\n");
    free(cursor);
    return EXECUTE_SUCCESS;
  }
  void *node = get_page(table->pager, cursor->page_num);
  uint32_t num_cells = *leaf_node_num_cells(node);
  for (uint32_t i = cursor->cell_num; i < num_cells - 1; i++)
  {
    memcpy(leaf_node_cell(node, i), leaf_node_cell(node, i + 1), LEAF_NODE_CELL_SIZE);
  }
  (*leaf_node_num_cells(node))--;
  free(cursor);
  return EXECUTE_SUCCESS;
}

ExecuteResult execute_statement(Statement *statement, Table *table)
{
  switch (statement->type)
  {
  case (STATEMENT_INSERT):
    return execute_insert(statement, table);
  case (STATEMENT_SELECT):
    return execute_select(statement, table);
  case (STATEMENT_SELECT_BY_ID):
    return execute_select_by_id(statement, table);
  case (STATEMENT_UPDATE):
    return execute_update(statement, table);
  case (STATEMENT_DELETE):
    return execute_delete(statement, table);
  }
  return EXECUTE_UNRECOGNIZED_STATEMENT;
}