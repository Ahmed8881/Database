#include "../include/command_processor.h"
#include "../include/btree.h"
#include "../include/cursor.h"
#include "../include/utils.h"
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>  // Add this for isspace() function

void print_constants()
{
  printf("ROW_SIZE: %d\n", ROW_SIZE);
  printf("COMMON_NODE_HEADER_SIZE: %lu\n", COMMON_NODE_HEADER_SIZE);
  printf("LEAF_NODE_HEADER_SIZE: %lu\n", LEAF_NODE_HEADER_SIZE);
  printf("LEAF_NODE_CELL_HEADER_SIZE: %lu\n", LEAF_NODE_CELL_HEADER_SIZE);
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
    } else if (strcmp(buf->buffer, ".users") == 0) {
        acl_list_active_users(&db->acl);
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
      // Check for alternative format: "insert into users (1, "abdul")"
      open_paren = strchr(into_keyword, '(');
      if (!open_paren || open_paren < values_keyword) {
        return PREPARE_SYNTAX_ERROR;
      }
    }
    
    // Find closing parenthesis
    char *close_paren = strrchr(open_paren, ')');
    if (!close_paren) {
      return PREPARE_SYNTAX_ERROR;
    }
    
    // This is where we'll store our values
    statement->num_values = 0;
    statement->values = NULL;
    
    // Extract all values between parentheses
    char *value_str = open_paren + 1;
    while (value_str < close_paren && statement->num_values < MAX_COLUMNS) {
      while (*value_str == ' ' || *value_str == '\t') value_str++; // Skip spaces
      
      if (value_str >= close_paren) break;
      
      // Allocate space for the new value
      statement->values = realloc(statement->values, 
                                (statement->num_values + 1) * sizeof(char*));
      if (!statement->values) {
        return PREPARE_SYNTAX_ERROR;
      }
      
      // Handle quoted strings
      if (*value_str == '"' || *value_str == '\'') {
        char quote_char = *value_str;
        value_str++; // Skip opening quote
        
        // Find closing quote
        char *end_quote = strchr(value_str, quote_char);
        if (!end_quote || end_quote >= close_paren) {
          return PREPARE_SYNTAX_ERROR;
        }
        
        int value_len = end_quote - value_str;
        char *value = malloc(value_len + 1);
        if (!value) return PREPARE_SYNTAX_ERROR;
        
        strncpy(value, value_str, value_len);
        value[value_len] = '\0';
        
        statement->values[statement->num_values++] = value;
        value_str = end_quote + 1;
      } else {
        // Handle non-quoted values (numbers, etc.)
        char *comma = strchr(value_str, ',');
        if (!comma || comma > close_paren) comma = close_paren;
        
        int value_len = comma - value_str;
        while (value_len > 0 && (value_str[value_len-1] == ' ' || value_str[value_len-1] == '\t')) 
          value_len--; // Trim trailing spaces
        
        char *value = malloc(value_len + 1);
        if (!value) return PREPARE_SYNTAX_ERROR;
        
        strncpy(value, value_str, value_len);
        value[value_len] = '\0';
        
        statement->values[statement->num_values++] = value;
        value_str = comma;
      }
      
      // Skip comma if present
      if (*value_str == ',') value_str++;
    }
    
    // For backward compatibility, still populate the old row_to_insert structure
    if (statement->num_values >= 1) {
      statement->row_to_insert.id = atoi(statement->values[0]);
      // Check for negative ID - must check after conversion to int
      if (atoi(statement->values[0]) < 0) {
        for (uint32_t i = 0; i < statement->num_values; i++) {
          free(statement->values[i]);
        }
        free(statement->values);
        return PREPARE_NEGATIVE_ID;
      }
    }
    
    if (statement->num_values >= 2) {
      strncpy(statement->row_to_insert.username, 
              statement->values[1], 
              COLUMN_USERNAME_SIZE);
      statement->row_to_insert.username[COLUMN_USERNAME_SIZE] = '\0';
    }
    
    if (statement->num_values >= 3) {
      strncpy(statement->row_to_insert.email, 
              statement->values[2], 
              COLUMN_EMAIL_SIZE);
      statement->row_to_insert.email[COLUMN_EMAIL_SIZE] = '\0';
    }
    
    return PREPARE_SUCCESS;
  } else {
    // Old syntax or partial new syntax: look for parentheses directly after table name
    char *open_paren = strchr(sql, '(');
    if (open_paren) {
      // This looks like "insert into users (1, "abdul")" format
      
      // Extract table name before the parenthesis
      char table_name[MAX_TABLE_NAME];
      char *table_start = sql + 6; // Skip "insert"
      while (*table_start == ' ') table_start++; // Skip spaces
      
      // Check if "into" is present
      if (strncasecmp(table_start, "into", 4) == 0) {
        table_start += 4; // Skip "into"
        while (*table_start == ' ') table_start++; // Skip spaces
      }
      
      // Find end of table name (space or parenthesis)
      char *table_end = table_start;
      while (*table_end && *table_end != ' ' && *table_end != '(') table_end++;
      
      int table_name_len = table_end - table_start;
      if (table_name_len <= 0 || table_name_len >= MAX_TABLE_NAME) {
        return PREPARE_SYNTAX_ERROR;
      }
      
      strncpy(table_name, table_start, table_name_len);
      table_name[table_name_len] = '\0';
      
      // Store table name in statement
      strncpy(statement->table_name, table_name, MAX_TABLE_NAME - 1);
      statement->table_name[MAX_TABLE_NAME - 1] = '\0';
      
      // Find closing parenthesis
      char *close_paren = strchr(open_paren, ')');
      if (!close_paren) {
        return PREPARE_SYNTAX_ERROR;
      }
      
      // Process values inside parentheses just like the other format
      statement->num_values = 0;
      statement->values = NULL;
      
      char *value_str = open_paren + 1;
      while (value_str < close_paren && statement->num_values < MAX_COLUMNS) {
        while (*value_str == ' ' || *value_str == '\t') value_str++; // Skip spaces
        
        if (value_str >= close_paren) break;
        
        // Allocate space for the new value
        statement->values = realloc(statement->values, 
                                  (statement->num_values + 1) * sizeof(char*));
        if (!statement->values) {
          return PREPARE_SYNTAX_ERROR;
        }
        
        // Handle quoted strings
        if (*value_str == '"' || *value_str == '\'') {
          char quote_char = *value_str;
          value_str++; // Skip opening quote
          
          // Find closing quote
          char *end_quote = strchr(value_str, quote_char);
          if (!end_quote || end_quote >= close_paren) {
            return PREPARE_SYNTAX_ERROR;
          }
          
          int value_len = end_quote - value_str;
          char *value = malloc(value_len + 1);
          if (!value) return PREPARE_SYNTAX_ERROR;
          
          strncpy(value, value_str, value_len);
          value[value_len] = '\0';
          
          statement->values[statement->num_values++] = value;
          value_str = end_quote + 1;
        } else {
          // Handle non-quoted values (numbers, etc.)
          char *comma = strchr(value_str, ',');
          if (!comma || comma > close_paren) comma = close_paren;
          
          int value_len = comma - value_str;
          while (value_len > 0 && (value_str[value_len-1] == ' ' || value_str[value_len-1] == '\t')) 
            value_len--; // Trim trailing spaces
          
          char *value = malloc(value_len + 1);
          if (!value) return PREPARE_SYNTAX_ERROR;
          
          strncpy(value, value_str, value_len);
          value[value_len] = '\0';
          
          statement->values[statement->num_values++] = value;
          value_str = comma;
        }
        
        // Skip comma if present
        if (*value_str == ',') value_str++;
      }
      
      // For backward compatibility, still populate the old row_to_insert structure
      if (statement->num_values >= 1) {
        statement->row_to_insert.id = atoi(statement->values[0]);
        // Check for negative ID - must check after conversion to int
        if (atoi(statement->values[0]) < 0) {
          for (uint32_t i = 0; i < statement->num_values; i++) {
            free(statement->values[i]);
          }
          free(statement->values);
          return PREPARE_NEGATIVE_ID;
        }
      }
      
      if (statement->num_values >= 2) {
        strncpy(statement->row_to_insert.username, 
                statement->values[1], 
                COLUMN_USERNAME_SIZE);
        statement->row_to_insert.username[COLUMN_USERNAME_SIZE] = '\0';
      }
      
      if (statement->num_values >= 3) {
        strncpy(statement->row_to_insert.email, 
                statement->values[2], 
                COLUMN_EMAIL_SIZE);
        statement->row_to_insert.email[COLUMN_EMAIL_SIZE] = '\0';
      }
      
      return PREPARE_SUCCESS;
    } else {
      // Old syntax: insert 1 username email
      // Use the existing implementation
      char *id_string = strtok(buf->buffer, " ");  // Will get "insert"
      id_string = strtok(NULL, " ");  // Get the actual ID
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
  else if (strncasecmp(buf->buffer, "create user", 11) == 0) {
    return prepare_create_user(buf, statement);
  }
  else if (strncasecmp(buf->buffer, "drop user", 9) == 0) {
    return prepare_drop_user(buf, statement);
  }
  else if (strncasecmp(buf->buffer, "grant role", 10) == 0) {
    return prepare_grant_role(buf, statement);
  }
  else if (strncasecmp(buf->buffer, "revoke role", 11) == 0) {
    return prepare_revoke_role(buf, statement);
  }
  else if (strncasecmp(buf->buffer, "login", 5) == 0) {
    return prepare_login(buf, statement);
  }
  else if (strncasecmp(buf->buffer, "logout", 6) == 0) {
    statement->type = STATEMENT_LOGOUT;
    return PREPARE_SUCCESS;
  }
  else if (strncasecmp(buf->buffer, "enable auth", 11) == 0) {
    statement->type = STATEMENT_ENABLE_AUTH;
    return PREPARE_SUCCESS;
  }
  else if (strncasecmp(buf->buffer, "disable auth", 12) == 0) {
    statement->type = STATEMENT_DISABLE_AUTH;
    return PREPARE_SUCCESS;
  }
  
  return PREPARE_UNRECOGNIZED_STATEMENT;
}

ExecuteResult execute_insert(Statement *statement, Table *table)
{
  // Get active table definition from database catalog
  TableDef* table_def = catalog_get_active_table(&statement->db->catalog);
  if (!table_def) {
    printf("Error: No active table definition found.\n");
    return EXECUTE_UNRECOGNIZED_STATEMENT;
  }

  // Create a dynamic row
  DynamicRow row;
  dynamic_row_init(&row, table_def);

  // Get primary key from the first value
  uint32_t key_to_insert = 0;
  
  // Check if we have values to insert
  if (!statement->values || statement->num_values == 0) {
    // Use the legacy row_to_insert approach for backward compatibility
    key_to_insert = statement->row_to_insert.id;
    
    // Set the primary key (assuming first column is the key)
    if (table_def->num_columns > 0 && table_def->columns[0].type == COLUMN_TYPE_INT) {
      dynamic_row_set_int(&row, table_def, 0, key_to_insert);
      #ifdef DEBUG
      printf("DEBUG: Set primary key %d using legacy approach\n", key_to_insert);
      #endif
    }
    
    // Fill in other column values if they exist
    if (table_def->num_columns > 1) {
      if (table_def->columns[1].type == COLUMN_TYPE_STRING) {
        dynamic_row_set_string(&row, table_def, 1, statement->row_to_insert.username);
        #ifdef DEBUG
        printf("DEBUG: Set column 1 to '%s' using legacy approach\n", statement->row_to_insert.username);
        #endif
      }
    }
    
    if (table_def->num_columns > 2) {
      if (table_def->columns[2].type == COLUMN_TYPE_STRING) {
        dynamic_row_set_string(&row, table_def, 2, statement->row_to_insert.email);
        #ifdef DEBUG
        printf("DEBUG: Set column 2 to '%s' using legacy approach\n", statement->row_to_insert.email);
        #endif
      }
    }
  } else {
    // Use the new values array for more flexible column handling
    key_to_insert = atoi(statement->values[0]);
    #ifdef DEBUG
    printf("DEBUG: Inserting new row with %d columns\n", statement->num_values);
    #endif
    
    // Set values for each column
    for (uint32_t i = 0; i < table_def->num_columns && i < statement->num_values; i++) {
      ColumnDef* col = &table_def->columns[i];
      char* value = statement->values[i];
      
      #ifdef DEBUG
      printf("DEBUG: Setting column %d (%s) to value '%s'\n", i, col->name, value);
      #endif
      
      switch (col->type) {
        case COLUMN_TYPE_INT:
          dynamic_row_set_int(&row, table_def, i, atoi(value));
          break;
        case COLUMN_TYPE_STRING:
          dynamic_row_set_string(&row, table_def, i, value);
          break;
        case COLUMN_TYPE_FLOAT:
          dynamic_row_set_float(&row, table_def, i, atof(value));
          break;
        case COLUMN_TYPE_BOOLEAN:
          dynamic_row_set_boolean(&row, table_def, i, 
              (strcasecmp(value, "true") == 0 || strcmp(value, "1") == 0));
          break;
        // Add other cases as needed
        default:
          // For now, just skip unsupported types
          #ifdef DEBUG
          printf("DEBUG: Unsupported type for column %d\n", i);
          #endif
          break;
      }
    }
  }
  
  // Debug print: Show what we're about to insert
  #ifdef DEBUG
  printf("Inserting row with key: %d\n", key_to_insert);
  print_dynamic_row(&row, table_def); // Add this to see the row content before insertion
  #endif
  
  Cursor *cursor = table_find(table, key_to_insert);
  if (!cursor) {
    printf("Error: Failed to create cursor for insertion.\n");
    dynamic_row_free(&row);
    return EXECUTE_UNRECOGNIZED_STATEMENT;
  }

  // Handle duplicate key
  void *cur_node = get_page(table->pager, cursor->page_num);
  if (cursor->cell_num < (*leaf_node_num_cells(cur_node)) && 
      key_to_insert == *leaf_node_key(cur_node, cursor->cell_num)) {
    printf("Error: Duplicate key detected: %d\n", key_to_insert);
    dynamic_row_free(&row);
    free(cursor);
    
    // Free the values array if it exists
    if (statement->values) {
      for (uint32_t i = 0; i < statement->num_values; i++) {
        free(statement->values[i]);
      }
      free(statement->values);
      statement->values = NULL;
      statement->num_values = 0;
    }
    
    return EXECUTE_DUPLICATE_KEY;
  }

  leaf_node_insert(cursor, key_to_insert, &row, table_def);
  printf("Row successfully inserted with key: %d\n", key_to_insert);
  
  free(cursor);
  dynamic_row_free(&row);
  
  // Free the values array if it exists
  if (statement->values) {
    for (uint32_t i = 0; i < statement->num_values; i++) {
      free(statement->values[i]);
    }
    free(statement->values);
    statement->values = NULL;
    statement->num_values = 0;
  }

  return EXECUTE_SUCCESS;
}

// Fix unused parameter warning by explicitly marking it
ExecuteResult execute_select(Statement *statement, Table *table)
{
  (void)statement; // Mark parameter as used
  
  TableDef* table_def = catalog_get_active_table(&statement->db->catalog);
  if (!table_def) {
    return EXECUTE_UNRECOGNIZED_STATEMENT;
  }
  
  #ifdef DEBUG
  printf("DEBUG: Selecting from table with %d columns\n", table_def->num_columns);
  #endif
  
  Cursor *cursor = table_start(table);
  DynamicRow row;
  dynamic_row_init(&row, table_def);

  // Print column names as header
  printf("| ");
  for (uint32_t i = 0; i < table_def->num_columns; i++) {
    printf("%s | ", table_def->columns[i].name);
  }
  printf("\n");
  
  // Print separator line
  for (uint32_t i = 0; i < table_def->num_columns; i++) {
    printf("|-%s-", "----------");
  }
  printf("|\n");
  
  while (!(cursor->end_of_table)) {
    void* value = cursor_value(cursor);
    
    deserialize_dynamic_row(value, table_def, &row);
    print_dynamic_row(&row, table_def);
    cursor_advance(cursor);
  }

  dynamic_row_free(&row);
  free(cursor);

  return EXECUTE_SUCCESS;
}

ExecuteResult execute_select_by_id(Statement *statement, Table *table)
{
  TableDef* table_def = catalog_get_active_table(&statement->db->catalog);
  if (!table_def) {
    return EXECUTE_UNRECOGNIZED_STATEMENT;
  }
  
  Cursor *cursor = table_find(table, statement->id_to_select);
  
  if (!cursor->end_of_table) {
    DynamicRow row;
    dynamic_row_init(&row, table_def);
    
    deserialize_dynamic_row(cursor_value(cursor), table_def, &row);
    print_dynamic_row(&row, table_def);
    
    dynamic_row_free(&row);
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
    // For variable-sized cells, we need to manually copy and shift each cell
    uint32_t current_pos = cursor->cell_num;
    
    // Get pointer to the cell being deleted
    void *cell_to_delete = leaf_node_cell(node, current_pos);
    
    // Get pointer to the next cell
    void *next_cell = leaf_node_next_cell(node, current_pos);
    
    // Calculate total size of data after this cell
    uint32_t bytes_to_move = 0;
    for (uint32_t i = current_pos + 1; i < num_cells; i++) {
      bytes_to_move += leaf_node_cell_size(node, i);
    }
    
    // Move all following cells backward
    if (bytes_to_move > 0) {
      memmove(cell_to_delete, next_cell, bytes_to_move);
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
                    if (column->size == 0) {  // Set default if parsing failed or explicit 0
                        column->size = 1024;  // 1KB default
                    }
                } else {
                    column->size = 1024;  // Default if parsing failed
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
                    if (column->size == 0) {  // Set default if parsing failed or explicit 0
                        column->size = 255;  // Default string size
                    }
                } else {
                    column->size = 255;  // Default if parsing failed
                }
            } else {
                // Default string size
                column->size = 255;
            }
            
            // No need to add space for null terminator - this is handled in dynamic_row_init
            
            #ifdef DEBUG
            printf("DEBUG: Set string column '%s' size to %u\n", column->name, column->size);
            #endif
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

// Fix unused parameter warning
PrepareResult prepare_show_tables(Input_Buffer* buf, Statement* statement) {
    (void)buf; // Mark parameter as used
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

// Fix unused parameter warning
ExecuteResult execute_show_tables(Statement* statement, Database* db) {
    (void)statement; // Mark parameter as used
    
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
    // Handle ACL-specific commands first
    switch (statement->type) {
        case STATEMENT_CREATE_USER:
            return execute_create_user(statement, db);
            
        case STATEMENT_DROP_USER:
            return execute_drop_user(statement, db);
            
        case STATEMENT_GRANT_ROLE:
            return execute_grant_role(statement, db);
            
        case STATEMENT_REVOKE_ROLE:
            return execute_revoke_role(statement, db);
            
        case STATEMENT_LOGIN:
            return execute_login(statement, db);
            
        case STATEMENT_LOGOUT:
            return execute_logout(statement, db);
            
        case STATEMENT_ENABLE_AUTH:
            return execute_enable_auth(statement, db);
            
        case STATEMENT_DISABLE_AUTH:
            return execute_disable_auth(statement, db);
    }
    
    // For non-ACL commands, check authentication if required
    if (db->auth_required && db->acl.current_user[0] == '\0') {
        printf("Error: Authentication required. Please login first.\n");
        return EXECUTE_UNRECOGNIZED_STATEMENT;
    }
    
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

// Add these functions after execute_show_tables
ExecuteResult execute_create_user(Statement *statement, Database *db) {
    // Only admin can create users
    if (db->auth_required && !acl_is_admin(&db->acl, db->acl.current_user)) {
        printf("Error: Only admin users can create new users.\n");
        return EXECUTE_UNRECOGNIZED_STATEMENT; 
    }
    
    // Add user to ACL
    if (!acl_add_user(&db->acl, statement->user.username, statement->user.password)) {
        printf("Error: Failed to create user '%s'. User might already exist.\n", 
               statement->user.username);
        return EXECUTE_FAILED;
    }
    
    // Assign role (if specified)
    if (statement->user.role_specified) {
        RoleType role;
        if (strcasecmp(statement->user.role_str, "admin") == 0) {
            role = ROLE_ADMIN;
        } else if (strcasecmp(statement->user.role_str, "developer") == 0) {
            role = ROLE_DEVELOPER;
        } else {
            role = ROLE_USER;
        }
        
        if (!acl_assign_role(&db->acl, statement->user.username, role)) {
            // If role assignment fails but it's an admin role, give a specific message
            if (role == ROLE_ADMIN) {
                printf("Error: Cannot create admin user '%s'. Only one admin user allowed.\n", 
                       statement->user.username);
                // Clean up by removing the user we just created
                acl_delete_user(&db->acl, statement->user.username);
                return EXECUTE_FAILED;
            } else {
                printf("Error: Failed to assign role to user '%s'.\n", statement->user.username);
                return EXECUTE_FAILED;
            }
        }
    }
    
    // printf("User '%s' created successfully.\n", statement->user.username);
    return EXECUTE_SUCCESS;
}

ExecuteResult execute_drop_user(Statement *statement, Database *db) {
    // Only admin can drop users
    if (db->auth_required && !acl_is_admin(&db->acl, db->acl.current_user)) {
        printf("Error: Only admin users can drop users.\n");
        return EXECUTE_UNRECOGNIZED_STATEMENT;
    }
    
    // Don't allow dropping current user
    if (strcmp(statement->user.username, db->acl.current_user) == 0) {
        printf("Error: Cannot drop currently logged in user.\n");
        return EXECUTE_UNRECOGNIZED_STATEMENT;
    }
    
    if (acl_delete_user(&db->acl, statement->user.username)) {
        printf("User '%s' dropped successfully.\n", statement->user.username);
        acl_save(&db->acl, db->name);
        return EXECUTE_SUCCESS;
    } else {
        printf("Failed to drop user '%s'. User may not exist.\n", statement->user.username);
        return EXECUTE_UNRECOGNIZED_STATEMENT;
    }
}

ExecuteResult execute_grant_role(Statement *statement, Database *db) {
    // Only admin can grant roles
    if (db->auth_required && !acl_is_admin(&db->acl, db->acl.current_user)) {
        printf("Error: Only admin users can grant roles.\n");
        return EXECUTE_UNRECOGNIZED_STATEMENT;
    }
    
    if (acl_assign_role(&db->acl, statement->user.username, statement->user.role)) {
        const char* role_name;
        switch (statement->user.role) {
            case ROLE_ADMIN: role_name = "Admin"; break;
            case ROLE_DEVELOPER: role_name = "Developer"; break;
            case ROLE_USER: role_name = "User"; break;
            default: role_name = "Unknown"; break;
        }
        
        printf("Granted role '%s' to user '%s'.\n", role_name, statement->user.username);
        acl_save(&db->acl, db->name);
        return EXECUTE_SUCCESS;
    } else {
        printf("Failed to grant role to user '%s'. User may not exist.\n", statement->user.username);
        return EXECUTE_UNRECOGNIZED_STATEMENT;
    }
}

ExecuteResult execute_revoke_role(Statement *statement, Database *db) {
    // Only admin can revoke roles
    if (db->auth_required && !acl_is_admin(&db->acl, db->acl.current_user)) {
        printf("Error: Only admin users can revoke roles.\n");
        return EXECUTE_UNRECOGNIZED_STATEMENT;
    }
    
    // Don't allow revoking from current user
    if (strcmp(statement->user.username, db->acl.current_user) == 0) {
        printf("Error: Cannot revoke role from currently logged in user.\n");
        return EXECUTE_UNRECOGNIZED_STATEMENT;
    }
    
    if (acl_remove_role(&db->acl, statement->user.username)) {
        printf("Revoked role from user '%s'.\n", statement->user.username);
        acl_save(&db->acl, db->name);
        return EXECUTE_SUCCESS;
    } else {
        printf("Failed to revoke role from user '%s'. User may not exist or have no role.\n", statement->user.username);
        return EXECUTE_UNRECOGNIZED_STATEMENT;
    }
}

ExecuteResult execute_login(Statement *statement, Database *db) {
    if (acl_authenticate(&db->acl, statement->user.username, statement->user.password)) {
        printf("Logged in as '%s'.\n", statement->user.username);
        return EXECUTE_SUCCESS;
    } else {
        printf("Invalid username or password.\n");
        return EXECUTE_UNRECOGNIZED_STATEMENT;
    }
}

ExecuteResult execute_logout(Statement *statement, Database *db) {
    if (statement->user.username[0] != '\0') {
        // Logout specific user
        return acl_logout_user(&db->acl, statement->user.username) ? 
            EXECUTE_SUCCESS : EXECUTE_UNRECOGNIZED_STATEMENT;
    } else {
        // Need to determine which user is executing this command
        printf("Error: Please specify which user to logout.\n");
        printf("Usage: LOGOUT username\n");
        return EXECUTE_UNRECOGNIZED_STATEMENT;
    }
}

ExecuteResult execute_enable_auth(Statement *statement, Database *db) {
    (void)statement; // Unused parameter
    
    db_enable_auth(db);
    return EXECUTE_SUCCESS;
}

ExecuteResult execute_disable_auth(Statement *statement, Database *db) {
    (void)statement; // Unused parameter
    
    // Only admin can disable auth
    if (db->auth_required && !acl_is_admin(&db->acl, db->acl.current_user)) {
        printf("Error: Only admin users can disable authentication.\n");
        return EXECUTE_UNRECOGNIZED_STATEMENT;
    }
    
    db_disable_auth(db);
    return EXECUTE_SUCCESS;
}

// Parse CREATE USER command
PrepareResult prepare_create_user(Input_Buffer *buf, Statement *statement) {
    statement->type = STATEMENT_CREATE_USER;
    
    // Format: CREATE USER username WITH PASSWORD 'password' [ROLE 'role']
    char *sql = buf->buffer;
    char *username_start = strcasestr(sql, "user") + 4;
    while (*username_start == ' ') username_start++;
    
    // Find the end of username (before WITH)
    char *username_end = strcasestr(username_start, "with");
    if (!username_end) {
        return PREPARE_SYNTAX_ERROR;
    }
    
    // Extract username
    int username_len = username_end - username_start;
    while (username_len > 0 && isspace(username_start[username_len-1])) username_len--;
    if (username_len <= 0 || username_len >= MAX_USERNAME_SIZE) {
        return PREPARE_SYNTAX_ERROR;
    }
    
    strncpy(statement->user.username, username_start, username_len);
    statement->user.username[username_len] = '\0';
    
    // Find PASSWORD
    char *pwd_keyword = strcasestr(username_end, "password");
    if (!pwd_keyword) {
        return PREPARE_SYNTAX_ERROR;
    }
    
    // Skip to password value (after PASSWORD)
    char *pwd_start = pwd_keyword + 8;
    while (*pwd_start == ' ') pwd_start++;
    
    // Initialize role fields
    statement->user.role_specified = false;
    statement->user.role = ROLE_USER; // default role
    
    // Handle quoted password
    if (*pwd_start == '\'' || *pwd_start == '\"') {
        char quote = *pwd_start;
        pwd_start++; // Skip opening quote
        char *pwd_end = strchr(pwd_start, quote);
        if (!pwd_end) {
            return PREPARE_SYNTAX_ERROR;
        }
        
        int pwd_len = pwd_end - pwd_start;
        if (pwd_len <= 0 || pwd_len >= MAX_PASSWORD_SIZE) {
            return PREPARE_SYNTAX_ERROR;
        }
        
        strncpy(statement->user.password, pwd_start, pwd_len);
        statement->user.password[pwd_len] = '\0';
        
        // Check for optional ROLE
        char *role_keyword = strcasestr(pwd_end, "role");
        if (role_keyword) {
            statement->user.role_specified = true;
            char *role_start = role_keyword + 4;
            while (*role_start == ' ') role_start++;
            
            // Handle quoted role
            if (*role_start == '\'' || *role_start == '\"') {
                char quote = *role_start;
                role_start++; // Skip opening quote
                char *role_end = strchr(role_start, quote);
                if (!role_end) {
                    return PREPARE_SYNTAX_ERROR;
                }
                
                int role_len = role_end - role_start;
                strncpy(statement->user.role_str, role_start, role_len);
                statement->user.role_str[role_len] = '\0';
                
                // Convert role name to enum
                if (strcasecmp(statement->user.role_str, "admin") == 0) {
                    statement->user.role = ROLE_ADMIN;
                } else if (strcasecmp(statement->user.role_str, "developer") == 0) {
                    statement->user.role = ROLE_DEVELOPER;
                } else if (strcasecmp(statement->user.role_str, "user") == 0) {
                    statement->user.role = ROLE_USER;
                } else {
                    return PREPARE_SYNTAX_ERROR;
                }
            }
        }
        
        return PREPARE_SUCCESS;
    }
    
    return PREPARE_SYNTAX_ERROR;
}

// Parse DROP USER command
PrepareResult prepare_drop_user(Input_Buffer *buf, Statement *statement) {
    statement->type = STATEMENT_DROP_USER;
    
    // Parse: DROP USER username
    char *token = strtok(buf->buffer, " \t"); // Skip "DROP"
    token = strtok(NULL, " \t");              // Skip "USER"
    token = strtok(NULL, " \t");              // Get username
    
    if (!token) {
        return PREPARE_SYNTAX_ERROR;
    }
    
    strncpy(statement->user.username, token, MAX_USERNAME_SIZE - 1);
    statement->user.username[MAX_USERNAME_SIZE - 1] = '\0';
    
    return PREPARE_SUCCESS;
}

// Parse GRANT ROLE command
PrepareResult prepare_grant_role(Input_Buffer *buf, Statement *statement) {
    statement->type = STATEMENT_GRANT_ROLE;
    
    // Parse: GRANT ROLE role TO username
    char *token = strtok(buf->buffer, " \t"); // Skip "GRANT"
    token = strtok(NULL, " \t");              // Skip "ROLE"
    token = strtok(NULL, " \t");              // Get role
    
    if (!token) {
        return PREPARE_SYNTAX_ERROR;
    }
    
    if (strcasecmp(token, "admin") == 0) {
        statement->user.role = ROLE_ADMIN;
    } else if (strcasecmp(token, "developer") == 0) {
        statement->user.role = ROLE_DEVELOPER;
    } else if (strcasecmp(token, "user") == 0) {
        statement->user.role = ROLE_USER;
    } else {
        return PREPARE_SYNTAX_ERROR;
    }
    
    token = strtok(NULL, " \t");              // Skip "TO"
    if (!token || strcasecmp(token, "to") != 0) {
        return PREPARE_SYNTAX_ERROR;
    }
    
    token = strtok(NULL, " \t");              // Get username
    if (!token) {
        return PREPARE_SYNTAX_ERROR;
    }
    
    strncpy(statement->user.username, token, MAX_USERNAME_SIZE - 1);
    statement->user.username[MAX_USERNAME_SIZE - 1] = '\0';
    
    return PREPARE_SUCCESS;
}

// Parse REVOKE ROLE command
PrepareResult prepare_revoke_role(Input_Buffer *buf, Statement *statement) {
    statement->type = STATEMENT_REVOKE_ROLE;
    
    // Parse: REVOKE ROLE FROM username
    char *token = strtok(buf->buffer, " \t"); // Skip "REVOKE"
    token = strtok(NULL, " \t");              // Skip "ROLE"
    
    if (!token || strcasecmp(token, "role") != 0) {
        return PREPARE_SYNTAX_ERROR;
    }
    
    token = strtok(NULL, " \t");              // Skip "FROM"
    if (!token || strcasecmp(token, "from") != 0) {
        return PREPARE_SYNTAX_ERROR;
    }
    
    token = strtok(NULL, " \t");              // Get username
    if (!token) {
        return PREPARE_SYNTAX_ERROR;
    }
    
    strncpy(statement->user.username, token, MAX_USERNAME_SIZE - 1);
    statement->user.username[MAX_USERNAME_SIZE - 1] = '\0';
    
    return PREPARE_SUCCESS;
}

// Parse LOGIN command
PrepareResult prepare_login(Input_Buffer *buf, Statement *statement) {
    statement->type = STATEMENT_LOGIN;
    
    // Parse: LOGIN username 'password'
    char *sql = buf->buffer;
    char *username_start = sql + 5; // Skip "LOGIN"
    while (*username_start == ' ') username_start++;
    
    // Find end of username (space before password)
    char *username_end = username_start;
    while (*username_end && !isspace(*username_end)) username_end++;
    
    if (username_end == username_start) {
        return PREPARE_SYNTAX_ERROR;
    }
    
    // Extract username
    int username_len = username_end - username_start;
    strncpy(statement->user.username, username_start, username_len);
    statement->user.username[username_len] = '\0';
    
    // Find password (should be quoted)
    char *pwd_start = username_end;
    while (*pwd_start && isspace(*pwd_start)) pwd_start++;
    
    if (*pwd_start == '\'' || *pwd_start == '\"') {
        char quote = *pwd_start;
        pwd_start++; // Skip opening quote
        char *pwd_end = strchr(pwd_start, quote);
        if (!pwd_end) {
            return PREPARE_SYNTAX_ERROR;
        }
        
        int pwd_len = pwd_end - pwd_start;
        strncpy(statement->user.password, pwd_start, pwd_len);
        statement->user.password[pwd_len] = '\0';
        
        return PREPARE_SUCCESS;
    }
    
    return PREPARE_SYNTAX_ERROR;
}

// Parse LOGOUT command
PrepareResult prepare_logout(Input_Buffer *buf, Statement *statement) {
    statement->type = STATEMENT_LOGOUT;
    
    // Check if a specific username is provided
    char* token = strtok(buf->buffer, " \t");  // Skip "LOGOUT"
    token = strtok(NULL, " \t");  // Get username if provided
    
    if (token) {
        // Logout a specific user
        strncpy(statement->user.username, token, MAX_USERNAME_SIZE - 1);
        statement->user.username[MAX_USERNAME_SIZE - 1] = '\0';
    } else {
        // No username specified, will logout the current session's user (determined later)
        statement->user.username[0] = '\0';
    }
    
    return PREPARE_SUCCESS;
}

// Add this function after the existing functions
bool check_permission(Database* db, Statement* statement) {
    // Admin can do anything
    if (acl_is_admin(&db->acl, db->acl.current_user)) {
        return true;
    }
    
    // Map statement type to command type for permission checking
    CommandType cmd_type;
    switch (statement->type) {
        case STATEMENT_SELECT:
        case STATEMENT_SELECT_BY_ID:
        case STATEMENT_SHOW_TABLES:
            cmd_type = CMD_READ;
            break;
            
        case STATEMENT_INSERT:
        case STATEMENT_UPDATE:
            cmd_type = CMD_WRITE;
            break;
            
        case STATEMENT_DELETE:
            cmd_type = CMD_DELETE;  // Map DELETE to its own command type
            break;
            
        case STATEMENT_CREATE_TABLE:
        case STATEMENT_CREATE_DATABASE:
            cmd_type = CMD_CREATE;
            break;
            
        case STATEMENT_DROP_USER:
            cmd_type = CMD_DROP;
            break;
            
        case STATEMENT_GRANT_ROLE:
            cmd_type = CMD_GRANT;
            break;
            
        case STATEMENT_REVOKE_ROLE:
            cmd_type = CMD_REVOKE;
            break;
            
        default:
            // For any other command types, delegate to the acl_has_permission
            return acl_has_permission(&db->acl, db->acl.current_user, CMD_READ);
    }
    
    // Check if user has permission for this command type
    return acl_has_permission(&db->acl, db->acl.current_user, cmd_type);
}