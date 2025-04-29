#include "../include/command_processor.h"
#include "../include/btree.h"
#include "../include/cursor.h"
#include "../include/utils.h"
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>

void print_constants()
{
  printf("ROW_SIZE: %d\n", ROW_SIZE);
  printf("COMMON_NODE_HEADER_SIZE: %lu\n", COMMON_NODE_HEADER_SIZE);
  printf("LEAF_NODE_HEADER_SIZE: %lu\n", LEAF_NODE_HEADER_SIZE);
  printf("LEAF_NODE_CELL_SIZE: %lu\n", LEAF_NODE_CELL_SIZE);
  printf("LEAF_NODE_SPACE_FOR_CELLS: %lu\n", LEAF_NODE_SPACE_FOR_CELLS);
  printf("LEAF_NODE_MAX_CELLS: %lu\n", LEAF_NODE_MAX_CELLS);
}

void indent(uint32_t level)
{
  for (uint32_t i = 0; i < level; i++)
  {
    printf("  ");
  }
}

// Meta command implementation
MetaCommandResult do_meta_command(Input_Buffer *buf, Database *db) {
    if (strcmp(buf->buffer, ".exit") == 0) {
        db_close_database(db);
        exit(EXIT_SUCCESS);
    } else if (strncmp(buf->buffer, ".btree", 6) == 0) {
        // Check if a specific table name is provided
        char table_name[MAX_TABLE_NAME] = {0};
        int args = sscanf(buf->buffer, ".btree %s", table_name);
        
        if (args == 1 && table_name[0] != '\0') {
            // Show B-tree for specific table
            int table_idx = catalog_find_table(&db->catalog, table_name);
            if (table_idx == -1) {
                printf("Error: Table '%s' not found.\n", table_name);
                return META_COMMAND_SUCCESS;
            }
            
            // Open the table temporarily if it's not the active one
            Table *table_to_show = NULL;
            bool temp_table = false;
            
            if (db->active_table && strcmp(db->catalog.tables[db->catalog.active_table].name, table_name) == 0) {
                // Use existing active table
                table_to_show = db->active_table;
            } else {
                // Open the table temporarily
                table_to_show = db_open(db->catalog.tables[table_idx].filename);
                table_to_show->root_page_num = db->catalog.tables[table_idx].root_page_num;
                temp_table = true;
            }
            
            printf("Tree for table '%s':\n", table_name);
            print_tree(table_to_show->pager, table_to_show->root_page_num, 0);
            
            // Close the temporary table if we created one
            if (temp_table) {
                db_close(table_to_show);
            }
        } else {
            // Show B-tree for active table (original behavior)
            if (db->active_table == NULL) {
                printf("Error: No active table selected.\n");
                return META_COMMAND_SUCCESS;
            }
            
            TableDef* active_table_def = catalog_get_active_table(&db->catalog);
            printf("Tree for active table '%s':\n", active_table_def->name);
            print_tree(db->active_table->pager, db->active_table->root_page_num, 0);
        }
        
        return META_COMMAND_SUCCESS;
    } else if (strcmp(buf->buffer, ".constants") == 0) {
        printf("Constants:\n");
        print_constants();
        return META_COMMAND_SUCCESS;
    }
    
    return META_COMMAND_UNRECOGNIZED_COMMAND;
}

PrepareResult prepare_insert(Input_Buffer *buf, Statement *statement)
{
  statement->type = STATEMENT_INSERT;
  
  char *sql = buf->buffer;
  
  // Check for "INSERT INTO table_name VALUES (values...)" syntax
  char *into_keyword = strcasestr(sql, "into");
  char *values_keyword = strcasestr(sql, "values");
  
  if (into_keyword && values_keyword) {
    // New syntax: INSERT INTO table_name VALUES (1, "name", etc)
    char table_name[MAX_TABLE_NAME];
    
    // Extract table name between "into" and "values"
    int table_name_start = (into_keyword - sql) + 4; // Skip "into"
    while (sql[table_name_start] == ' ') table_name_start++; // Skip spaces
    
    int table_name_end = (values_keyword - sql);
    while (sql[table_name_end-1] == ' ') table_name_end--; // Trim trailing spaces
    
    int table_name_len = table_name_end - table_name_start;
    if (table_name_len <= 0 || table_name_len >= MAX_TABLE_NAME) {
      return PREPARE_SYNTAX_ERROR;
    }
    
    strncpy(table_name, sql + table_name_start, table_name_len);
    table_name[table_name_len] = '\0';
    
    // Store table name in statement
    strncpy(statement->table_name, table_name, MAX_TABLE_NAME - 1);
    statement->table_name[MAX_TABLE_NAME - 1] = '\0';
    
    // Now extract the values - find opening parenthesis after VALUES
    char *open_paren = strchr(values_keyword, '(');
    if (!open_paren) {
      return PREPARE_SYNTAX_ERROR;
    }
    
    // Extract ID, name and other values
    char *id_str = open_paren + 1;
    while (*id_str == ' ') id_str++; // Skip spaces
    
    // Parse the ID
    char *comma = strchr(id_str, ',');
    if (!comma) {
      return PREPARE_SYNTAX_ERROR;
    }
    
    char id_buffer[32];
    int id_len = comma - id_str;
    strncpy(id_buffer, id_str, id_len);
    id_buffer[id_len] = '\0';
    
    // Convert ID to integer
    int id = atoi(id_buffer);
    if (id < 0) {
      return PREPARE_NEGATIVE_ID;
    }
    
    // Parse username/name (may be in quotes)
    char *name_start = comma + 1;
    while (*name_start == ' ') name_start++; // Skip spaces
    
    char *name_end;
    char username[COLUMN_USERNAME_SIZE + 1] = {0};
    
    if (*name_start == '"' || *name_start == '\'') {
      // Name is in quotes
      char quote_char = *name_start;
      name_start++; // Skip opening quote
      name_end = strchr(name_start, quote_char);
      if (!name_end) {
        return PREPARE_SYNTAX_ERROR;
      }
      
      int name_len = name_end - name_start;
      if (name_len > COLUMN_USERNAME_SIZE) {
        return PREPARE_STRING_TOO_LONG;
      }
      
      strncpy(username, name_start, name_len);
      username[name_len] = '\0';
      
      // Move past the closing quote
      name_end++;
    } else {
      // Name is not in quotes
      name_end = strchr(name_start, ',');
      if (!name_end) {
        return PREPARE_SYNTAX_ERROR;
      }
      
      int name_len = name_end - name_start;
      if (name_len > COLUMN_USERNAME_SIZE) {
        return PREPARE_STRING_TOO_LONG;
      }
      
      strncpy(username, name_start, name_len);
      username[name_len] = '\0';
    }
    
    // Parse the third value (could be email, grade, etc.)
    char *third_value_start = name_end + 1;
    while (*third_value_start == ' ') third_value_start++; // Skip spaces
    
    char *third_value_end;
    char email[COLUMN_EMAIL_SIZE + 1] = {0};
    
    if (*third_value_start == '"' || *third_value_start == '\'') {
      // Value is in quotes
      char quote_char = *third_value_start;
      third_value_start++; // Skip opening quote
      third_value_end = strchr(third_value_start, quote_char);
      if (!third_value_end) {
        return PREPARE_SYNTAX_ERROR;
      }
      
      int value_len = third_value_end - third_value_start;
      if (value_len > COLUMN_EMAIL_SIZE) {
        return PREPARE_STRING_TOO_LONG;
      }
      
      strncpy(email, third_value_start, value_len);
      email[value_len] = '\0';
    } else {
      // Value is not in quotes - could be a number or unquoted string
      third_value_end = strchr(third_value_start, ')');
      if (!third_value_end) {
        return PREPARE_SYNTAX_ERROR;
      }
      
      int value_len = third_value_end - third_value_start;
      if (value_len > COLUMN_EMAIL_SIZE) {
        return PREPARE_STRING_TOO_LONG;
      }
      
      strncpy(email, third_value_start, value_len);
      email[value_len] = '\0';
    }
    
    // Store the values in statement
    statement->row_to_insert.id = id;
    strcpy(statement->row_to_insert.username, username);
    strcpy(statement->row_to_insert.email, email);
    
    return PREPARE_SUCCESS;
  } else {
    // Old syntax: insert 1 username email
    // Use the existing implementation
    char *keyword = strtok(buf->buffer, " ");  // "insert"
    char *id_string = strtok(NULL, " ");
    char *username = strtok(NULL, " ");
    char *email = strtok(NULL, " ");

    if (id_string == NULL || username == NULL || email == NULL) {
      return PREPARE_SYNTAX_ERROR;
    }

    int id = atoi(id_string);
    if (id < 0) {
      return PREPARE_NEGATIVE_ID;
    }
    
    if (strlen(username) > COLUMN_USERNAME_SIZE) {
      return PREPARE_STRING_TOO_LONG;
    }
    
    if (strlen(email) > COLUMN_EMAIL_SIZE) {
      return PREPARE_STRING_TOO_LONG;
    }

    statement->row_to_insert.id = id;
    strcpy(statement->row_to_insert.username, username);
    strcpy(statement->row_to_insert.email, email);
    
    return PREPARE_SUCCESS;
  }
}

PrepareResult prepare_statement(Input_Buffer *buf, Statement *statement)
{
  // Initialize table name as empty
  statement->table_name[0] = '\0';
  
  if (strncasecmp(buf->buffer, "insert", 6) == 0) {
    return prepare_insert(buf, statement);
  }
  else if (strncasecmp(buf->buffer, "select", 6) == 0) {
    // Check for "SELECT * FROM table_name" syntax
    char *from_keyword = strcasestr(buf->buffer, "from");
    
    if (from_keyword) {
      // New syntax: SELECT * FROM table_name [WHERE id = x]
      statement->type = STATEMENT_SELECT;
      
      // Extract table name after FROM
      char *table_start = from_keyword + 4; // Skip "from"
      while (*table_start == ' ') table_start++; // Skip spaces
      
      // Find end of table name (space or end of string)
      char *table_end = table_start;
      while (*table_end && *table_end != ' ' && *table_end != '\0') table_end++;
      
      int table_name_len = table_end - table_start;
      if (table_name_len <= 0 || table_name_len >= MAX_TABLE_NAME) {
        return PREPARE_SYNTAX_ERROR;
      }
      
      // Store table name in statement
      strncpy(statement->table_name, table_start, table_name_len);
      statement->table_name[table_name_len] = '\0';
      
      // Check for WHERE clause
      char *where_keyword = strcasestr(table_end, "where");
      if (where_keyword) {
        statement->type = STATEMENT_SELECT_BY_ID;
        
        // Parse id after "where id ="
        char *id_str = strstr(where_keyword, "id =");
        if (id_str) {
          id_str += 4; // Skip "id ="
          while (*id_str == ' ') id_str++; // Skip spaces
          statement->id_to_select = atoi(id_str);
          return PREPARE_SUCCESS;
        } else {
          return PREPARE_SYNTAX_ERROR;
        }
      }
      
      return PREPARE_SUCCESS;
    } else {
      // Old syntax: select or select where id = x
      // Check if this is a select by ID query
      char* id_str = strstr(buf->buffer, "where id =");
      if (id_str) {
        statement->type = STATEMENT_SELECT_BY_ID;
        statement->id_to_select = atoi(id_str + 10);
        return PREPARE_SUCCESS;
      } else {
        statement->type = STATEMENT_SELECT;
        return PREPARE_SUCCESS;
      }
    }
  }
  else if (strncasecmp(buf->buffer, "update", 6) == 0) {
    statement->type = STATEMENT_UPDATE;
    
    // New SQL-like syntax: UPDATE table_name SET column = value WHERE id = X
    char *sql = buf->buffer;
    char *table_start = sql + 6; // Skip "update"
    while (*table_start == ' ') table_start++; // Skip spaces
    
    // Find table name end (space before SET)
    char *table_end = strcasestr(table_start, "set");
    if (!table_end) {
      return PREPARE_SYNTAX_ERROR;
    }
    
    // Extract table name
    int table_name_len = table_end - table_start;
    while (table_start[table_name_len-1] == ' ') table_name_len--; // Trim trailing spaces
    
    if (table_name_len <= 0 || table_name_len >= MAX_TABLE_NAME) {
      return PREPARE_SYNTAX_ERROR;
    }
    
    strncpy(statement->table_name, table_start, table_name_len);
    statement->table_name[table_name_len] = '\0';
    
    // Find column name and value
    char *column_start = table_end + 3; // Skip "set"
    while (*column_start == ' ') column_start++; // Skip spaces
    
    char *equals_pos = strchr(column_start, '=');
    if (!equals_pos) {
      return PREPARE_SYNTAX_ERROR;
    }
    
    // Extract column name
    int column_name_len = equals_pos - column_start;
    while (column_start[column_name_len-1] == ' ') column_name_len--; // Trim trailing spaces
    
    char column_name[MAX_COLUMN_NAME] = {0};
    strncpy(column_name, column_start, column_name_len);
    column_name[column_name_len] = '\0';
    
    // Extract value (handling quotes)
    char *value_start = equals_pos + 1;
    while (*value_start == ' ') value_start++; // Skip spaces
    
    char *value_end;
    char update_value[COLUMN_EMAIL_SIZE] = {0}; // Using email size as it's larger
    
    if (*value_start == '"' || *value_start == '\'') {
      // Value is in quotes
      char quote_char = *value_start;
      value_start++; // Skip opening quote
      value_end = strchr(value_start, quote_char);
      if (!value_end) {
        return PREPARE_SYNTAX_ERROR;
      }
      
      int value_len = value_end - value_start;
      strncpy(update_value, value_start, value_len);
      update_value[value_len] = '\0';
    } else {
      // Value is not in quotes
      value_end = strcasestr(value_start, "where");
      if (!value_end) {
        value_end = value_start + strlen(value_start);
      }
      
      int value_len = value_end - value_start;
      while (value_start[value_len-1] == ' ') value_len--; // Trim trailing spaces
      
      strncpy(update_value, value_start, value_len);
      update_value[value_len] = '\0';
    }
    
    // Save update info to statement
    strncpy(statement->column_to_update, column_name, MAX_COLUMN_NAME - 1);
    statement->column_to_update[MAX_COLUMN_NAME - 1] = '\0';
    
    strncpy(statement->update_value, update_value, COLUMN_EMAIL_SIZE - 1);
    statement->update_value[COLUMN_EMAIL_SIZE - 1] = '\0';
    
    // Find ID to update
    char* id_str = strstr(buf->buffer, "where id =");
    if (id_str) {
      statement->id_to_update = atoi(id_str + 10);
      return PREPARE_SUCCESS;
    } else {
      return PREPARE_SYNTAX_ERROR;
    }
  }
  else if (strncasecmp(buf->buffer, "delete", 6) == 0) {
    statement->type = STATEMENT_DELETE;
    
    // New SQL-like syntax: DELETE FROM table_name WHERE id = X
    char *sql = buf->buffer;
    char *from_keyword = strcasestr(sql, "from");
    
    if (from_keyword) {
      // Extract table name
      char *table_start = from_keyword + 4; // Skip "from"
      while (*table_start == ' ') table_start++; // Skip spaces
      
      char *table_end = strcasestr(table_start, "where");
      if (!table_end) {
        return PREPARE_SYNTAX_ERROR;
      }
      
      int table_name_len = table_end - table_start;
      while (table_start[table_name_len-1] == ' ') table_name_len--; // Trim trailing spaces
      
      if (table_name_len <= 0 || table_name_len >= MAX_TABLE_NAME) {
        return PREPARE_SYNTAX_ERROR;
      }
      
      strncpy(statement->table_name, table_start, table_name_len);
      statement->table_name[table_name_len] = '\0';
      
      // Parse id in "where id = X"
      char *id_str = strstr(table_end, "id =");
      if (id_str) {
        statement->id_to_delete = atoi(id_str + 4);
        return PREPARE_SUCCESS;
      }
    }
    
    // Fallback to old syntax
    char* id_str = strstr(buf->buffer, "where id =");
    if (id_str) {
      statement->id_to_delete = atoi(id_str + 10);
      return PREPARE_SUCCESS;
    }
    
    return PREPARE_SYNTAX_ERROR;
  }
  
  else if (strncasecmp(buf->buffer, "create table", 12) == 0) {
    return prepare_create_table(buf, statement);
  }
  else if (strncasecmp(buf->buffer, "use table", 9) == 0) {
    return prepare_use_table(buf, statement);
  }
  else if (strncasecmp(buf->buffer, "show tables", 11) == 0) {
    return prepare_show_tables(buf, statement);
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

  // Handle special case of duplicate key
  void *cur_node = get_page(table->pager, cursor->page_num);
  if (cursor->cell_num < (*leaf_node_num_cells(cur_node)) && 
      key_to_insert == *leaf_node_key(cur_node, cursor->cell_num)) {
      return EXECUTE_DUPLICATE_KEY;
  }

  leaf_node_insert(cursor, row_to_insert->id, row_to_insert);

  free(cursor);

  return EXECUTE_SUCCESS;
}

ExecuteResult execute_select(Statement *statement, Table *table)
{
  Cursor *cursor = table_start(table);

  Row row;
  while (!(cursor->end_of_table)) {
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
  if (!cursor->end_of_table) {
    deserialize_row(cursor_value(cursor), &row);
    print_row(&row);
  } else {
    printf("No row found with id %d\n", statement->id_to_select);
  }
  
  free(cursor);
  
  return EXECUTE_SUCCESS;
}

ExecuteResult execute_update(Statement *statement, Table *table)
{
  // Find the row with the given id
  Cursor *cursor = table_find(table, statement->id_to_update);
  
  if (cursor->end_of_table) {
    printf("No row found with id %d\n", statement->id_to_update);
    free(cursor);
    return EXECUTE_SUCCESS;
  }
  
  // Get current row data
  Row row;
  deserialize_row(cursor_value(cursor), &row);
  
  // Update the appropriate field based on column name
  if (strcasecmp(statement->column_to_update, "name") == 0 || 
      strcasecmp(statement->column_to_update, "username") == 0) {
    strncpy(row.username, statement->update_value, COLUMN_USERNAME_SIZE);
  }
  else if (strcasecmp(statement->column_to_update, "email") == 0) {
    strncpy(row.email, statement->update_value, COLUMN_EMAIL_SIZE);
  }
  else {
    printf("Unknown column: %s\n", statement->column_to_update);
    free(cursor);
    return EXECUTE_SUCCESS;
  }
  
  // Save updated row back to the database
  serialize_row(&row, cursor_value(cursor));
  
  free(cursor);
  return EXECUTE_SUCCESS;
}

ExecuteResult execute_delete(Statement *statement, Table *table)
{
  // Find the row with the given id
  Cursor *cursor = table_find(table, statement->id_to_delete);
  
  if (cursor->end_of_table) {
    printf("No row found with id %d\n", statement->id_to_delete);
    free(cursor);
    return EXECUTE_SUCCESS;
  }
  
  // Get the leaf node and number of cells
  void *node = get_page(table->pager, cursor->page_num);
  uint32_t num_cells = *leaf_node_num_cells(node);
  
  // Shift cells to overwrite the one being deleted
  if (cursor->cell_num < num_cells - 1) {
    for (uint32_t i = cursor->cell_num; i < num_cells - 1; i++) {
      memcpy(leaf_node_cell(node, i), leaf_node_cell(node, i + 1), LEAF_NODE_CELL_SIZE);
    }
  }
  
  // Decrement number of cells
  (*leaf_node_num_cells(node))--;
  
  free(cursor);
  return EXECUTE_SUCCESS;
}

PrepareResult prepare_create_table(Input_Buffer* buf, Statement* statement) {
    statement->type = STATEMENT_CREATE_TABLE;
    
    // Tokenize: CREATE TABLE name (col1 type1, col2 type2, ...)
    char* token = strtok(buf->buffer, " \t");  // Skip "CREATE"
    token = strtok(NULL, " \t");               // Skip "TABLE"
    token = strtok(NULL, " \t(");              // Get table name
    
    if (!token) {
        return PREPARE_SYNTAX_ERROR;
    }
    
    strncpy(statement->table_name, token, MAX_TABLE_NAME - 1);
    statement->table_name[MAX_TABLE_NAME - 1] = '\0';
    
    // Parse columns: col1 type1, col2 type2, ...
    statement->num_columns = 0;
    
    // Tokenize the column definitions
    char* col_name = strtok(NULL, " \t,)");
    while (col_name && statement->num_columns < MAX_COLUMNS) {
        // Get column type
        char* col_type = strtok(NULL, " \t,)");
        if (!col_type) {
            return PREPARE_SYNTAX_ERROR;
        }
        
        ColumnDef* column = &statement->columns[statement->num_columns];
        strncpy(column->name, col_name, MAX_COLUMN_NAME - 1);
        column->name[MAX_COLUMN_NAME - 1] = '\0';
        
        if (strcasecmp(col_type, "INT") == 0) {
            column->type = COLUMN_TYPE_INT;
            column->size = sizeof(int32_t);
        } else if (strcasecmp(col_type, "FLOAT") == 0) {
            column->type = COLUMN_TYPE_FLOAT;
            column->size = sizeof(float);
        } else if (strcasecmp(col_type, "BOOLEAN") == 0) {
            column->type = COLUMN_TYPE_BOOLEAN;
            column->size = sizeof(uint8_t);
        } else if (strcasecmp(col_type, "DATE") == 0) {
            column->type = COLUMN_TYPE_DATE;
            column->size = sizeof(int32_t);
        } else if (strcasecmp(col_type, "TIME") == 0) {
            column->type = COLUMN_TYPE_TIME;
            column->size = sizeof(int32_t);
        } else if (strcasecmp(col_type, "TIMESTAMP") == 0) {
            column->type = COLUMN_TYPE_TIMESTAMP;
            column->size = sizeof(int64_t);
        } else if (strncasecmp(col_type, "BLOB", 4) == 0) {
            column->type = COLUMN_TYPE_BLOB;
            
            // Check if blob has a size: BLOB(n)
            char* size_start = strchr(col_type, '(');
            if (size_start) {
                char* size_end = strchr(size_start, ')');
                if (size_end) {
                    *size_end = '\0';
                    char* size_str = size_start + 1;
                    column->size = atoi(size_str);
                }
            } else {
                // Default blob size
                column->size = 1024; // 1KB default
            }
        } else if (strncasecmp(col_type, "STRING", 6) == 0) {
            column->type = COLUMN_TYPE_STRING;
            
            // Check if string has a size: STRING(n)
            char* size_start = strchr(col_type, '(');
            if (size_start) {
                char* size_end = strchr(size_start, ')');
                if (size_end) {
                    *size_end = '\0';
                    char* size_str = size_start + 1;
                    column->size = atoi(size_str);
                    column->size++; // Add space for null terminator
                }
            } else {
                // Default string size
                column->size = 255;
            }
        } else {
            return PREPARE_SYNTAX_ERROR;
        }
        
        statement->num_columns++;
        col_name = strtok(NULL, " \t,)");
    }
    
    if (statement->num_columns == 0) {
        return PREPARE_SYNTAX_ERROR;
    }
    
    return PREPARE_SUCCESS;
}

PrepareResult prepare_use_table(Input_Buffer* buf, Statement* statement) {
    statement->type = STATEMENT_USE_TABLE;
    
    // Parse: USE TABLE table_name
    char* token = strtok(buf->buffer, " \t");  // Skip "USE"
    token = strtok(NULL, " \t");               // Skip "TABLE"
    token = strtok(NULL, " \t");               // Get table name
    
    if (!token) {
        return PREPARE_SYNTAX_ERROR;
    }
    
    strncpy(statement->table_name, token, MAX_TABLE_NAME - 1);
    statement->table_name[MAX_TABLE_NAME - 1] = '\0';
    
    return PREPARE_SUCCESS;
}

PrepareResult prepare_show_tables(Input_Buffer* buf, Statement* statement) {
    statement->type = STATEMENT_SHOW_TABLES;
    return PREPARE_SUCCESS;
}

ExecuteResult execute_create_table(Statement* statement, Database* db) {
    if (db_create_table(db, statement->table_name, statement->columns, statement->num_columns)) {
        printf("Table created: %s\n", statement->table_name);
        return EXECUTE_SUCCESS;
    } else {
        printf("Failed to create table: %s\n", statement->table_name);
        return EXECUTE_UNRECOGNIZED_STATEMENT;
    }
}

ExecuteResult execute_use_table(Statement* statement, Database* db) {
    if (db_use_table(db, statement->table_name)) {
        printf("Using table: %s\n", statement->table_name);
        return EXECUTE_SUCCESS;
    } else {
        printf("Table not found: %s\n", statement->table_name);
        return EXECUTE_UNRECOGNIZED_STATEMENT;
    }
}

ExecuteResult execute_show_tables(Statement* statement, Database* db) {
    printf("Tables in database %s:\n", db->name);
    
    if (db->catalog.num_tables == 0) {
        printf("  No tables found.\n");
    } else {
        for (uint32_t i = 0; i < db->catalog.num_tables; i++) {
            printf("  %s%s\n", 
                db->catalog.tables[i].name,
                (i == db->catalog.active_table && db->active_table != NULL) ? " (active)" : "");
        }
    }
    
    return EXECUTE_SUCCESS;
}

// Add the execute statement implementation
ExecuteResult execute_statement(Statement *statement, Database *db) {
    // Check if we need to switch tables first, but skip this for CREATE TABLE
    if (statement->table_name[0] != '\0' && statement->type != STATEMENT_CREATE_TABLE) {
        // A table was specified in the query
        
        // Check if we need to switch
        bool need_switch = true;
        if (db->active_table != NULL) {
            TableDef* active_table = catalog_get_active_table(&db->catalog);
            if (active_table && strcmp(active_table->name, statement->table_name) == 0) {
                need_switch = false;  // Already using this table
            }
        }
        
        // Switch tables if needed
        if (need_switch) {
            if (!db_use_table(db, statement->table_name)) {
                printf("Table not found: %s\n", statement->table_name);
                return EXECUTE_UNRECOGNIZED_STATEMENT;
            }
            // Table successfully switched
        }
    }
    
    // Now execute the statement with the correct table active
    switch (statement->type) {
        case STATEMENT_INSERT:
            if (db->active_table == NULL) {
                printf("Error: No active table selected.\n");
                return EXECUTE_SUCCESS;
            }
            return execute_insert(statement, db->active_table);
            
        case STATEMENT_SELECT:
            if (db->active_table == NULL) {
                printf("Error: No active table selected.\n");
                return EXECUTE_SUCCESS;
            }
            return execute_select(statement, db->active_table);
            
        case STATEMENT_SELECT_BY_ID:
            if (db->active_table == NULL) {
                printf("Error: No active table selected.\n");
                return EXECUTE_SUCCESS;
            }
            return execute_select_by_id(statement, db->active_table);
            
        case STATEMENT_UPDATE:
            if (db->active_table == NULL) {
                printf("Error: No active table selected.\n");
                return EXECUTE_SUCCESS;
            }
            return execute_update(statement, db->active_table);
            
        case STATEMENT_DELETE:
            if (db->active_table == NULL) {
                printf("Error: No active table selected.\n");
                return EXECUTE_SUCCESS;
            }
            return execute_delete(statement, db->active_table);
            
        case STATEMENT_CREATE_TABLE:
            return execute_create_table(statement, db);
            
        case STATEMENT_USE_TABLE:
            return execute_use_table(statement, db);
            
        case STATEMENT_SHOW_TABLES:
            return execute_show_tables(statement, db);
            
        case STATEMENT_CREATE_DATABASE:
        case STATEMENT_USE_DATABASE:
            // These should be handled separately
            return EXECUTE_UNRECOGNIZED_STATEMENT;
            
        default:
            return EXECUTE_UNRECOGNIZED_STATEMENT;
    }
}

PrepareResult prepare_database_statement(Input_Buffer *buf, Statement *statement) {
    if (strncasecmp(buf->buffer, "create database", 15) == 0) {
        statement->type = STATEMENT_CREATE_DATABASE;
        
        // Parse database name
        char* token = strtok(buf->buffer, " \t");  // Skip "CREATE"
        token = strtok(NULL, " \t");              // Skip "DATABASE"
        token = strtok(NULL, " \t");              // Get database name
        
        if (!token) {
            return PREPARE_SYNTAX_ERROR;
        }
        
        strncpy(statement->database_name, token, sizeof(statement->database_name) - 1);
        statement->database_name[sizeof(statement->database_name) - 1] = '\0';
        
        return PREPARE_SUCCESS;
    }
    else if (strncasecmp(buf->buffer, "use database", 12) == 0) {
        statement->type = STATEMENT_USE_DATABASE;
        
        // Parse database name
        char* token = strtok(buf->buffer, " \t");  // Skip "USE"
        token = strtok(NULL, " \t");              // Skip "DATABASE"
        token = strtok(NULL, " \t");              // Get database name
        
        if (!token) {
            return PREPARE_SYNTAX_ERROR;
        }
        
        strncpy(statement->database_name, token, sizeof(statement->database_name) - 1);
        statement->database_name[sizeof(statement->database_name) - 1] = '\0';
        
        return PREPARE_SUCCESS;
    }
    
    return PREPARE_UNRECOGNIZED_STATEMENT;
}

ExecuteResult execute_database_statement(Statement *statement, Database **db_ptr) {
    switch (statement->type) {
        case STATEMENT_CREATE_DATABASE:
            // Close current database if open
            if (*db_ptr) {
                db_close_database(*db_ptr);
                *db_ptr = NULL;
            }
            
            // Create and open the new database
            *db_ptr = db_create_database(statement->database_name);
            if (!*db_ptr) {
                return EXECUTE_UNRECOGNIZED_STATEMENT;
            }
            
            printf("Database created: %s\n", statement->database_name);
            return EXECUTE_SUCCESS;
            
        case STATEMENT_USE_DATABASE:
            // Close current database if open
            if (*db_ptr) {
                db_close_database(*db_ptr);
                *db_ptr = NULL;
            }
            
            // Open the existing database
            *db_ptr = db_open_database(statement->database_name);
            if (!*db_ptr) {
                return EXECUTE_UNRECOGNIZED_STATEMENT;
            }
            
            printf("Using database: %s\n", statement->database_name);
            return EXECUTE_SUCCESS;
            
        default:
            return EXECUTE_UNRECOGNIZED_STATEMENT;
    }
}