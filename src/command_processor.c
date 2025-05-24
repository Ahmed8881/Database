#include "../include/command_processor.h"
#include "../include/secondary_index.h"
#include "../include/btree.h"
#include "../include/cursor.h"
#include "../include/utils.h"
#include "../include/json_formatter.h" // Add this include for JSON formatting functions
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h> // For strcasecmp on Linux

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
MetaCommandResult do_meta_command(Input_Buffer *buf, Database *db)
{
  if (strcmp(buf->buffer, ".exit") == 0)
  {
    db_close_database(db);
    exit(EXIT_SUCCESS);
  }
  else if (strncmp(buf->buffer, ".btree", 6) == 0)
  {
    // Check if a specific table name is provided
    char table_name[MAX_TABLE_NAME] = {0};
    int args = sscanf(buf->buffer, ".btree %s", table_name);

    if (args == 1 && table_name[0] != '\0')
    {
      // Show B-tree for specific table
      int table_idx = catalog_find_table(&db->catalog, table_name);
      if (table_idx == -1)
      {
        printf("Error: Table '%s' not found.\n", table_name);
        return META_COMMAND_SUCCESS;
      }

      // Open the table temporarily if it's not the active one
      Table *table_to_show = NULL;
      bool temp_table = false;

      if (db->active_table &&
          strcmp(db->catalog.tables[db->catalog.active_table].name,
                 table_name) == 0)
      {
        // Use existing active table
        table_to_show = db->active_table;
      }
      else
      {
        // Open the table temporarily
        table_to_show = db_open(db->catalog.tables[table_idx].filename);
        table_to_show->root_page_num =
            db->catalog.tables[table_idx].root_page_num;
        temp_table = true;
      }

      printf("Tree for table '%s':\n", table_name);
      print_tree(table_to_show->pager, table_to_show->root_page_num, 0);

      // Close the temporary table if we created one
      if (temp_table)
      {
        db_close(table_to_show);
      }
    }
    else
    {
      // Show B-tree for active table (original behavior)
      if (db->active_table == NULL)
      {
        printf("Error: No active table selected.\n");
        return META_COMMAND_SUCCESS;
      }

      TableDef *active_table_def = catalog_get_active_table(&db->catalog);
      printf("Tree for active table '%s':\n", active_table_def->name);
      print_tree(db->active_table->pager, db->active_table->root_page_num, 0);
    }

    return META_COMMAND_SUCCESS;
  }
  else if (strcmp(buf->buffer, ".constants") == 0)
  {
    printf("Constants:\n");
    print_constants();
    return META_COMMAND_SUCCESS;
  }
  // Add transaction commands
  else if (strcmp(buf->buffer, ".txn begin") == 0)
  {
    return META_COMMAND_TXN_BEGIN;
  }
  else if (strcmp(buf->buffer, ".txn commit") == 0)
  {
    return META_COMMAND_TXN_COMMIT;
  }
  else if (strcmp(buf->buffer, ".txn rollback") == 0)
  {
    return META_COMMAND_TXN_ROLLBACK;
  }
  else if (strcmp(buf->buffer, ".txn status") == 0)
  {
    return META_COMMAND_TXN_STATUS;
  }
  else if (strcmp(buf->buffer, ".txn enable") == 0)
  {
    db_enable_transactions(db);
    return META_COMMAND_SUCCESS;
  }
  else if (strcmp(buf->buffer, ".txn disable") == 0)
  {
    db_disable_transactions(db);
    return META_COMMAND_SUCCESS;
  }
  else if (strncmp(buf->buffer, ".format", 7) == 0)
  {
    char format_type[10] = {0};
    int args = sscanf(buf->buffer, ".format %9s", format_type);

    if (args != 1)
    {
      printf("Usage: .format [table|json]\n");
      printf("Current format: %s\n",
             db->output_format == OUTPUT_FORMAT_TABLE ? "table" : "json");
      return META_COMMAND_SUCCESS;
    }

    if (strcasecmp(format_type, "table") == 0)
    {
      db->output_format = OUTPUT_FORMAT_TABLE;
      printf("Output format set to TABLE\n");
    }
    else if (strcasecmp(format_type, "json") == 0)
    {
      db->output_format = OUTPUT_FORMAT_JSON;
      printf("Output format set to JSON\n");
    }
    else
    {
      printf("Unknown format: %s\n", format_type);
      printf("Available formats: table, json\n");
    }

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

  if (into_keyword && values_keyword)
  {
    // New syntax: INSERT INTO table_name VALUES (1, "name", etc)
    char table_name[MAX_TABLE_NAME];

    // Extract table name between "into" and "values"
    int table_name_start = (into_keyword - sql) + 4; // Skip "into"
    while (sql[table_name_start] == ' ')
      table_name_start++; // Skip spaces

    int table_name_end = (values_keyword - sql);
    while (sql[table_name_end - 1] == ' ')
      table_name_end--; // Trim trailing spaces

    int table_name_len = table_name_end - table_name_start;
    if (table_name_len <= 0 || table_name_len >= MAX_TABLE_NAME)
    {
      return PREPARE_SYNTAX_ERROR;
    }

    strncpy(table_name, sql + table_name_start, table_name_len);
    table_name[table_name_len] = '\0';

    // Store table name in statement
    strncpy(statement->table_name, table_name, MAX_TABLE_NAME - 1);
    statement->table_name[MAX_TABLE_NAME - 1] = '\0';

    // Now extract the values - find opening parenthesis after VALUES
    char *open_paren = strchr(values_keyword, '(');
    if (!open_paren)
    {
      return PREPARE_SYNTAX_ERROR;
    }

    // Find closing parenthesis
    char *close_paren = strrchr(open_paren, ')');
    if (!close_paren)
    {
      return PREPARE_SYNTAX_ERROR;
    }

    // This is where we'll store our values
    statement->num_values = 0;
    statement->values = NULL;

    // Extract all values between parentheses
    char *value_str = open_paren + 1;
    while (value_str < close_paren && statement->num_values < MAX_COLUMNS)
    {
      while (*value_str == ' ' || *value_str == '\t')
        value_str++; // Skip spaces

      if (value_str >= close_paren)
        break;

      // Allocate space for the new value
      statement->values = realloc(statement->values,
                                  (statement->num_values + 1) * sizeof(char *));
      if (!statement->values)
      {
        return PREPARE_SYNTAX_ERROR;
      }

      // Handle quoted strings
      if (*value_str == '"' || *value_str == '\'')
      {
        char quote_char = *value_str;
        value_str++; // Skip opening quote

        // Find closing quote
        char *end_quote = strchr(value_str, quote_char);
        if (!end_quote || end_quote >= close_paren)
        {
          return PREPARE_SYNTAX_ERROR;
        }

        int value_len = end_quote - value_str;
        char *value = malloc(value_len + 1);
        if (!value)
          return PREPARE_SYNTAX_ERROR;

        strncpy(value, value_str, value_len);
        value[value_len] = '\0';

        statement->values[statement->num_values++] = value;
        value_str = end_quote + 1;
      }
      else
      {
        // Handle non-quoted values (numbers, etc.)
        char *comma = strchr(value_str, ',');
        if (!comma || comma > close_paren)
          comma = close_paren;

        int value_len = comma - value_str;
        while (value_len > 0 && (value_str[value_len - 1] == ' ' ||
                                 value_str[value_len - 1] == '\t'))
          value_len--; // Trim trailing spaces

        char *value = malloc(value_len + 1);
        if (!value)
          return PREPARE_SYNTAX_ERROR;

        strncpy(value, value_str, value_len);
        value[value_len] = '\0';

        statement->values[statement->num_values++] = value;
        value_str = comma;
      }

      // Skip comma if present
      if (*value_str == ',')
        value_str++;
    }

    // For backward compatibility, still populate the old row_to_insert
    // structure
    if (statement->num_values >= 1)
    {
      statement->row_to_insert.id = atoi(statement->values[0]);
      // Check for negative ID - must check after conversion to int
      if (atoi(statement->values[0]) < 0)
      {
        for (uint32_t i = 0; i < statement->num_values; i++)
        {
          free(statement->values[i]);
        }
        free(statement->values);
        return PREPARE_NEGATIVE_ID;
      }
    }

    if (statement->num_values >= 2)
    {
      strncpy(statement->row_to_insert.username, statement->values[1],
              COLUMN_USERNAME_SIZE);
      statement->row_to_insert.username[COLUMN_USERNAME_SIZE] = '\0';
    }

    if (statement->num_values >= 3)
    {
      strncpy(statement->row_to_insert.email, statement->values[2],
              COLUMN_EMAIL_SIZE);
      statement->row_to_insert.email[COLUMN_EMAIL_SIZE] = '\0';
    }

    return PREPARE_SUCCESS;
  }
  else
  {
    // Check for alternate syntax: INSERT INTO table_name (val1, val2, ...)
    // without the VALUES keyword
    if (into_keyword) {
      char table_name[MAX_TABLE_NAME];

      // Extract table name after "into"
      int table_name_start = (into_keyword - sql) + 4; // Skip "into"
      while (sql[table_name_start] == ' ')
        table_name_start++; // Skip spaces

      // Find opening parenthesis
      char *open_paren = strchr(into_keyword, '(');
      if (!open_paren) {
        return PREPARE_SYNTAX_ERROR;
      }

      // Extract table name between "into" and "("
      int table_name_end = (open_paren - sql);
      while (sql[table_name_end - 1] == ' ')
        table_name_end--; // Trim trailing spaces

      int table_name_len = table_name_end - table_name_start;
      if (table_name_len <= 0 || table_name_len >= MAX_TABLE_NAME)
      {
        return PREPARE_SYNTAX_ERROR;
      }

      strncpy(table_name, sql + table_name_start, table_name_len);
      table_name[table_name_len] = '\0';

      // Store table name in statement
      strncpy(statement->table_name, table_name, MAX_TABLE_NAME - 1);
      statement->table_name[MAX_TABLE_NAME - 1] = '\0';

      // Find closing parenthesis
      char *close_paren = strrchr(open_paren, ')');
      if (!close_paren)
      {
        return PREPARE_SYNTAX_ERROR;
      }

      // This is where we'll store our values
      statement->num_values = 0;
      statement->values = NULL;

      // Extract all values between parentheses
      char *value_str = open_paren + 1;
      while (value_str < close_paren && statement->num_values < MAX_COLUMNS)
      {
        while (*value_str == ' ' || *value_str == '\t')
          value_str++; // Skip spaces

        if (value_str >= close_paren)
          break;

        // Allocate space for the new value
        statement->values = realloc(statement->values,
                                   (statement->num_values + 1) * sizeof(char *));
        if (!statement->values)
        {
          return PREPARE_SYNTAX_ERROR;
        }

        // Handle quoted strings
        if (*value_str == '"' || *value_str == '\'')
        {
          char quote_char = *value_str;
          value_str++; // Skip opening quote

          // Find closing quote
          char *end_quote = strchr(value_str, quote_char);
          if (!end_quote || end_quote >= close_paren)
          {
            return PREPARE_SYNTAX_ERROR;
          }

          int value_len = end_quote - value_str;
          char *value = malloc(value_len + 1);
          if (!value)
            return PREPARE_SYNTAX_ERROR;

          strncpy(value, value_str, value_len);
          value[value_len] = '\0';

          statement->values[statement->num_values++] = value;
          value_str = end_quote + 1;
        }
        else
        {
          // Handle non-quoted values (numbers, etc.)
          char *comma = strchr(value_str, ',');
          if (!comma || comma > close_paren)
            comma = close_paren;

          int value_len = comma - value_str;
          while (value_len > 0 && (value_str[value_len - 1] == ' ' ||
                                  value_str[value_len - 1] == '\t'))
            value_len--; // Trim trailing spaces

          char *value = malloc(value_len + 1);
          if (!value)
            return PREPARE_SYNTAX_ERROR;

          strncpy(value, value_str, value_len);
          value[value_len] = '\0';

          statement->values[statement->num_values++] = value;
          value_str = comma;
        }

        // Skip comma if present
        if (*value_str == ',')
          value_str++;
      }

      // For backward compatibility, still populate the old row_to_insert
      // structure
      if (statement->num_values >= 1)
      {
        statement->row_to_insert.id = atoi(statement->values[0]);
        // Check for negative ID - must check after conversion to int
        if (atoi(statement->values[0]) < 0)
        {
          for (uint32_t i = 0; i < statement->num_values; i++)
          {
            free(statement->values[i]);
          }
          free(statement->values);
          return PREPARE_NEGATIVE_ID;
        }
      }

      if (statement->num_values >= 2)
      {
        strncpy(statement->row_to_insert.username, statement->values[1],
                COLUMN_USERNAME_SIZE);
        statement->row_to_insert.username[COLUMN_USERNAME_SIZE] = '\0';
      }

      if (statement->num_values >= 3)
      {
        strncpy(statement->row_to_insert.email, statement->values[2],
                COLUMN_EMAIL_SIZE);
        statement->row_to_insert.email[COLUMN_EMAIL_SIZE] = '\0';
      }

      return PREPARE_SUCCESS;
    }

    // Old syntax: insert 1 username email
    // Use the existing implementation
    char *id_string = strtok(buf->buffer, " "); // Will get "insert"
    id_string = strtok(NULL, " ");              // Get the actual ID
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

    if (strlen(username) > COLUMN_USERNAME_SIZE)
    {
      return PREPARE_STRING_TOO_LONG;
    }

    if (strlen(email) > COLUMN_EMAIL_SIZE)
    {
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

  // Initialize columns to select
  statement->columns_to_select = NULL;
  statement->num_columns_to_select = 0;
  statement->has_where_clause = false;
  
  // Process authentication commands regardless of database state
  if (strncasecmp(buf->buffer, "login", 5) == 0) {
    return prepare_login(buf, statement);
  }
  else if (strncasecmp(buf->buffer, "logout", 6) == 0) {
    return prepare_logout(buf, statement);
  }
  else if (strncasecmp(buf->buffer, "create user", 11) == 0) {
    return prepare_create_user(buf, statement);
  }

  // Rest of command processing
  if (strncasecmp(buf->buffer, "insert", 6) == 0)
  {
    return prepare_insert(buf, statement);
  }
  else if (strncasecmp(buf->buffer, "select", 6) == 0)
  {
    return prepare_select(buf, statement);
  }
  else if (strncasecmp(buf->buffer, "create index", 12) == 0)
  {
    return prepare_create_index(buf, statement);
  }
  else if (strncasecmp(buf->buffer, "update", 6) == 0)
  {
    statement->type = STATEMENT_UPDATE;

    // SQL-like syntax: UPDATE table_name SET column = value WHERE id = X
    char *sql = buf->buffer;
    char *table_start = sql + 6; // Skip "update"
    while (*table_start == ' ')
      table_start++; // Skip spaces

    // Find table name end (space before SET)
    char *table_end = strcasestr(table_start, "set");
    if (!table_end)
    {
      return PREPARE_SYNTAX_ERROR;
    }

    // Extract table name
    int table_name_len = table_end - table_start;
    while (table_start[table_name_len - 1] == ' ')
      table_name_len--; // Trim trailing spaces

    if (table_name_len <= 0 || table_name_len >= MAX_TABLE_NAME)
    {
      return PREPARE_SYNTAX_ERROR;
    }

    strncpy(statement->table_name, table_start, table_name_len);
    statement->table_name[table_name_len] = '\0';

    // Find column name and value
    char *column_start = table_end + 3; // Skip "set"
    while (*column_start == ' ')
      column_start++; // Skip spaces

    char *equals_pos = strchr(column_start, '=');
    if (!equals_pos)
    {
      return PREPARE_SYNTAX_ERROR;
    }

    // Extract column name
    int column_name_len = equals_pos - column_start;
    while (column_start[column_name_len - 1] == ' ')
      column_name_len--; // Trim trailing spaces

    char column_name[MAX_COLUMN_NAME] = {0};
    strncpy(column_name, column_start, column_name_len);
    column_name[column_name_len] = '\0';

    // Extract value (handling quotes)
    char *value_start = equals_pos + 1;
    while (*value_start == ' ')
      value_start++; // Skip spaces

    char *value_end;
    char update_value[COLUMN_EMAIL_SIZE] = {
        0}; // Using email size as it's larger

    if (*value_start == '"' || *value_start == '\'')
    {
      // Value is in quotes
      char quote_char = *value_start;
      value_start++; // Skip opening quote
      value_end = strchr(value_start, quote_char);
      if (!value_end)
      {
        return PREPARE_SYNTAX_ERROR;
      }

      int value_len = value_end - value_start;
      strncpy(update_value, value_start, value_len);
      update_value[value_len] = '\0';
    }
    else
    {
      // Value is not in quotes
      value_end = strcasestr(value_start, "where");
      if (!value_end)
      {
        value_end = value_start + strlen(value_start);
      }

      int value_len = value_end - value_start;
      while (value_start[value_len - 1] == ' ')
        value_len--; // Trim trailing spaces

      strncpy(update_value, value_start, value_len);
      update_value[value_len] = '\0';
    }

    // Save update info to statement
    strncpy(statement->column_to_update, column_name, MAX_COLUMN_NAME - 1);
    statement->column_to_update[MAX_COLUMN_NAME - 1] = '\0';

    strncpy(statement->update_value, update_value, COLUMN_EMAIL_SIZE - 1);
    statement->update_value[COLUMN_EMAIL_SIZE - 1] = '\0';

    // Find ID to update
    char *id_str = strstr(buf->buffer, "where id =");
    if (id_str)
    {
      statement->id_to_update = atoi(id_str + 10);
      return PREPARE_SUCCESS;
    }
    else
    {
      return PREPARE_SYNTAX_ERROR;
    }
  }
  else if (strncasecmp(buf->buffer, "delete", 6) == 0)
  {
    statement->type = STATEMENT_DELETE;

    // New SQL-like syntax: DELETE FROM table_name WHERE id = X
    char *sql = buf->buffer;
    char *from_keyword = strcasestr(sql, "from");

    if (from_keyword)
    {
      // Extract table name
      char *table_start = from_keyword + 4; // Skip "from"
      while (*table_start == ' ')
        table_start++; // Skip spaces

      char *table_end = strcasestr(table_start, "where");
      if (!table_end)
      {
        return PREPARE_SYNTAX_ERROR;
      }

      int table_name_len = table_end - table_start;
      while (table_start[table_name_len - 1] == ' ')
        table_name_len--; // Trim trailing spaces

      if (table_name_len <= 0 || table_name_len >= MAX_TABLE_NAME)
      {
        return PREPARE_SYNTAX_ERROR;
      }

      strncpy(statement->table_name, table_start, table_name_len);
      statement->table_name[table_name_len] = '\0';

      // Parse id in "where id = X"
      char *id_str = strstr(table_end, "id =");
      if (id_str)
      {
        statement->id_to_delete = atoi(id_str + 4);
        return PREPARE_SUCCESS;
      }
    }

    // Fallback to old syntax
    char *id_str = strstr(buf->buffer, "where id =");
    if (id_str)
    {
      statement->id_to_delete = atoi(id_str + 10);
      return PREPARE_SUCCESS;
    }

    return PREPARE_SYNTAX_ERROR;
  }

  else if (strncasecmp(buf->buffer, "create table", 12) == 0)
  {
    return prepare_create_table(buf, statement);
  }
  else if (strncasecmp(buf->buffer, "use table", 9) == 0)
  {
    return prepare_use_table(buf, statement);
  }
  else if (strncasecmp(buf->buffer, "show tables", 11) == 0)
  {
    return prepare_show_tables(buf, statement);
  }
  else if (strncasecmp(buf->buffer, "show indexes", 12) == 0)
  {
    return prepare_show_indexes(buf, statement);
  }

  return PREPARE_UNRECOGNIZED_STATEMENT;
}

PrepareResult prepare_select(Input_Buffer *buf, Statement *statement)
{
  statement->type = STATEMENT_SELECT;
  char *sql = buf->buffer;

  // Find FROM keyword
  char *from_keyword = strcasestr(sql, "from");
  if (!from_keyword)
  {
    return PREPARE_SYNTAX_ERROR;
  }

  // Extract columns to select
  char *columns_part = sql + 6; // Skip 'select'
  while (*columns_part == ' ')
    columns_part++; // Skip spaces

  int columns_len = from_keyword - columns_part;
  while (columns_part[columns_len - 1] == ' ')
    columns_len--; // Skip trailing spaces

  // Handle column select
  if (columns_len == 1 && columns_part[0] == '*')
  {
    // Select all Columns
    statement->columns_to_select = NULL;
    statement->num_columns_to_select = 0;
  }
  else
  {
    // Parse specific columns
    char columns_buffer[MAX_COLUMN_SIZE];
    strncpy(columns_buffer, columns_part,
            columns_len > (MAX_COLUMN_SIZE - 1) ? (MAX_COLUMN_SIZE - 1) : columns_len);
    columns_buffer[columns_len > (MAX_COLUMN_SIZE - 1) ? (MAX_COLUMN_SIZE - 1) : columns_len] = '\0';

    // Count commas to determine number of columns (commas + 1)
    uint32_t num_columns = 1;
    for (int i = 0; i < columns_len; i++)
    {
      if (columns_buffer[i] == ',')
      {
        num_columns++;
      }
    }

    // Allocate memory for columns
    statement->columns_to_select = malloc(num_columns * sizeof(char *));
    if (!statement->columns_to_select)
    {
      return PREPARE_SYNTAX_ERROR;
    }

    // Parse column names
    char *column_start = columns_buffer;
    uint32_t column_index = 0;

    for (int i = 0; i <= columns_len; i++)
    {
      if (columns_buffer[i] == ',' || columns_buffer[i] == '\0')
      {
        // Found a delimiter or end of string
        columns_buffer[i] = '\0'; // Mark end of this column name

        // Trim leading and trailing spaces
        char *column_name = column_start;
        while (*column_name == ' ')
          column_name++;

        char *end = column_name + strlen(column_name) - 1;
        while (end > column_name && *end == ' ')
        {
          *end = '\0';
          end--;
        }

        if (*column_name != '\0')
        { // Skip empty column names
          statement->columns_to_select[column_index] = my_strdup(column_name);
          if (!statement->columns_to_select[column_index])
          {
            // Handle allocation failure
            for (uint32_t j = 0; j < column_index; j++)
            {
              free(statement->columns_to_select[j]);
            }
            free(statement->columns_to_select);
            statement->columns_to_select = NULL;
            return PREPARE_SYNTAX_ERROR;
          }
          column_index++;
        }

        // Move to next column
        column_start = &columns_buffer[i + 1];
      }
    }

    statement->num_columns_to_select = column_index;

    // If no valid columns found, select all
    if (statement->num_columns_to_select == 0)
    {
      free(statement->columns_to_select);
      statement->columns_to_select = NULL;
      statement->num_columns_to_select = 0;
    }
  }

  // Extract table name between 'from' and potential 'where'
  char *table_start = from_keyword + 4; // Skip from
  while (*table_start == ' ')
    table_start++; // Skip spaces

  // Find end of table name (space or end of string)
  char *table_end = table_start;
  while (*table_end && *table_end != ' ' && *table_end != '\0')
    table_end++;

  int table_name_len = table_end - table_start;
  if (table_name_len <= 0 || table_name_len >= MAX_TABLE_NAME)
  {
    // No table name found
    // Free allocated memory
    free_columns_to_select(statement);
    return PREPARE_SYNTAX_ERROR;
  }

  // Store table name
  strncpy(statement->table_name, table_start, table_name_len);
  statement->table_name[table_name_len] = '\0';

  // Check for 'where' clause
  char *where_keyword = strcasestr(table_end, "where");
  if (where_keyword)
  {
    statement->has_where_clause = true;

    char *condition_start = where_keyword + 5; // Skip "where"
    while (*condition_start == ' ')
      condition_start++;

    // parse condition in format "column = value"
    char *equals_pos = strchr(condition_start, '=');
    if (!equals_pos)
    {
      free_columns_to_select(statement);
      return PREPARE_SYNTAX_ERROR;
    }

    // Extract column name
    int column_name_len = equals_pos - condition_start;
    while (condition_start[column_name_len - 1] == ' ')
      column_name_len--; // Trim trailing spaces
    if (column_name_len <= 0 || column_name_len >= MAX_COLUMN_NAME)
    {
      free_columns_to_select(statement);
      return PREPARE_SYNTAX_ERROR;
    }

    strncpy(statement->where_column, condition_start, column_name_len);
    statement->where_column[column_name_len] = '\0';

    // Extract value
    char *value_start = equals_pos + 1;
    while (*value_start == ' ')
      value_start++; // Skip spaces

    // Handle quoted value
    if (*value_start == '"' || *value_start == '\'')
    {
      char quote_char = *value_start;
      value_start++; // Skip opening quote

      char *value_end = strchr(value_start, quote_char);
      if (!value_end)
      {
        free_columns_to_select(statement);
        return PREPARE_SYNTAX_ERROR;
      }

      int value_len = value_end - value_start;
      strncpy(statement->where_value, value_start, value_len);
      statement->where_value[value_len] = '\0';
    }
    else
    {
      // Unquoted value - read until space or end of string
      char *value_end = value_start;
      while (*value_end && *value_end != ' ' && *value_end != '\0')
        value_end++;

      int value_len = value_end - value_start;
      strncpy(statement->where_value, value_start, value_len);
      statement->where_value[value_len] = '\0';
    }
  }
  return PREPARE_SUCCESS;
}

void free_columns_to_select(Statement *statement)
{
  if (statement->columns_to_select)
  {
    for (uint32_t i = 0; i < statement->num_columns_to_select; i++)
    {
      free(statement->columns_to_select[i]);
    }
    free(statement->columns_to_select);
    statement->columns_to_select = NULL;
    statement->num_columns_to_select = 0;
  }
}

// Modify the execute_insert function to support transactions:

ExecuteResult execute_insert(Statement *statement, Table *table)
{
  // Get active table definition from database catalog
  TableDef *table_def = catalog_get_active_table(&statement->db->catalog);
  if (!table_def)
  {
    printf("Error: No active table definition found.\n");
    return EXECUTE_UNRECOGNIZED_STATEMENT;
  }

  // Check for active transaction if transactions are enabled
  uint32_t txn_id = 0;
  if (txn_manager_is_enabled(&statement->db->txn_manager))
  {
    txn_id = statement->db->active_txn_id;
    if (txn_id == 0)
    {
      // Auto-start a transaction for this operation
      txn_id = db_begin_transaction(statement->db);
      if (txn_id == 0)
      {
        printf("Warning: Could not start transaction for INSERT operation.\n");
      }
    }
  }

  // Rest of your existing execute_insert code...

  // Declare and initialize the row variable
  DynamicRow row;
  dynamic_row_init(&row, table_def);

  // Get primary key from the first value
  uint32_t key_to_insert = 0;

  // Check if we have values to insert
  if (!statement->values || statement->num_values == 0)
  {
    // Use the legacy row_to_insert approach for backward compatibility
    key_to_insert = statement->row_to_insert.id;

    // Set the primary key (assuming first column is the key)
    if (table_def->num_columns > 0 &&
        table_def->columns[0].type == COLUMN_TYPE_INT)
    {
      dynamic_row_set_int(&row, table_def, 0, key_to_insert);
#ifdef DEBUG
      printf("DEBUG: Set primary key %d using legacy approach\n",
             key_to_insert);
#endif
    }

    // Fill in other column values if they exist
    if (table_def->num_columns > 1)
    {
      if (table_def->columns[1].type == COLUMN_TYPE_STRING)
      {
        dynamic_row_set_string(&row, table_def, 1,
                               statement->row_to_insert.username);
#ifdef DEBUG
        printf("DEBUG: Set column 1 to '%s' using legacy approach\n",
               statement->row_to_insert.username);
#endif
      }
    }

    if (table_def->num_columns > 2)
    {
      if (table_def->columns[2].type == COLUMN_TYPE_STRING)
      {
        dynamic_row_set_string(&row, table_def, 2,
                               statement->row_to_insert.email);
#ifdef DEBUG
        printf("DEBUG: Set column 2 to '%s' using legacy approach\n",
               statement->row_to_insert.email);
#endif
      }
    }
  }
  else
  {
    // Use the new values array for more flexible column handling
    key_to_insert = atoi(statement->values[0]);
#ifdef DEBUG
    printf("DEBUG: Inserting new row with %d columns\n", statement->num_values);
#endif

    // Set values for each column
    for (uint32_t i = 0;
         i < table_def->num_columns && i < statement->num_values; i++)
    {
      ColumnDef *col = &table_def->columns[i];
      char *value = statement->values[i];

#ifdef DEBUG
      printf("DEBUG: Setting column %d (%s) to value '%s'\n", i, col->name,
             value);
#endif

      switch (col->type)
      {
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
        dynamic_row_set_boolean(
            &row, table_def, i,
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
  print_dynamic_row(
      &row, table_def); // Add this to see the row content before insertion
#endif

  Cursor *cursor = table_find(table, key_to_insert);
  if (!cursor)
  {
    printf("Error: Failed to create cursor for insertion.\n");
    dynamic_row_free(&row);
    return EXECUTE_UNRECOGNIZED_STATEMENT;
  }

  // Handle duplicate key
  void *cur_node = get_page(table->pager, cursor->page_num);
  if (cursor->cell_num < (*leaf_node_num_cells(cur_node)) &&
      key_to_insert == *leaf_node_key(cur_node, cursor->cell_num))
  {
    printf("Error: Duplicate key detected: %d\n", key_to_insert);
    dynamic_row_free(&row);
    free(cursor);

    // Free the values array if it exists
    if (statement->values)
    {
      for (uint32_t i = 0; i < statement->num_values; i++)
      {
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
  if (statement->values)
  {
    for (uint32_t i = 0; i < statement->num_values; i++)
    {
      free(statement->values[i]);
    }
    free(statement->values);
    statement->values = NULL;
    statement->num_values = 0;
  }

  // After successful insertion and before returning:
  if (txn_id != 0)
  {
    printf("INSERT recorded in transaction %u\n", txn_id);

    // You would normally record the change for potential rollback
    // For a true implementation, you'd need to capture the pre-change state
    // txn_record_change(&statement->db->txn_manager, txn_id, cursor->page_num,
    //                   cursor->cell_num, key_to_insert, NULL, 0);
  }

  return EXECUTE_SUCCESS;
}

ExecuteResult execute_select(Statement *statement, Table *table)
{
  TableDef *table_def = catalog_get_active_table(&statement->db->catalog);
  if (!table_def)
  {
    return EXECUTE_UNRECOGNIZED_STATEMENT;
  }

#ifdef DEBUG
  printf("DEBUG: Selecting from table with %d columns\n",
         table_def->num_columns);
#endif

  // If the statement has a where clause, it's a filtered select
  if (statement->has_where_clause)
  {
    return execute_filtered_select(statement, table);
  }

  Cursor *cursor = table_start(table);
  DynamicRow row;
  dynamic_row_init(&row, table_def);

  int row_count = 0;

  // Choose output format based on database setting
  if (statement->db->output_format == OUTPUT_FORMAT_JSON)
  {
    // JSON format
    start_json_result();
    bool first_row = true;

    while (!(cursor->end_of_table))
    {
      if (!first_row)
      {
        printf(",\n    ");
      }
      else
      {
        printf("    ");
        first_row = false;
      }

      void *value = cursor_value(cursor);
      deserialize_dynamic_row(value, table_def, &row);

      format_row_as_json(&row, table_def, statement->columns_to_select,
                         statement->num_columns_to_select);

      row_count++;
      cursor_advance(cursor);
    }

    end_json_result(row_count);
  }
  else
  {
    // Table format (existing implementation)
    // Print column names as header
    printf("| ");
    if (statement->num_columns_to_select > 0)
    {
      // Print only selected columns
      for (uint32_t i = 0; i < statement->num_columns_to_select; i++)
      {
        printf("%s | ", statement->columns_to_select[i]);
      }
    }
    else
    {
      // Print all column names
      for (uint32_t i = 0; i < table_def->num_columns; i++)
      {
        printf("%s | ", table_def->columns[i].name);
      }
    }
    printf("\n");

    // Print separator line
    for (uint32_t i = 0; i < (statement->num_columns_to_select > 0
                                  ? statement->num_columns_to_select
                                  : table_def->num_columns);
         i++)
    {
      printf("|-%s-", "----------");
    }
    printf("|\n");

    while (!(cursor->end_of_table))
    {
      void *value = cursor_value(cursor);
      deserialize_dynamic_row(value, table_def, &row);

      // Print row data
      printf("| ");

      if (statement->num_columns_to_select > 0)
      {
        // Print only selected columns
        for (uint32_t i = 0; i < statement->num_columns_to_select; i++)
        {
          // Find column index by name
          int column_idx = -1;
          for (uint32_t j = 0; j < table_def->num_columns; j++)
          {
            if (strcasecmp(table_def->columns[j].name,
                           statement->columns_to_select[i]) == 0)
            {
              column_idx = j;
              break;
            }
          }

          if (column_idx != -1)
          {
            print_dynamic_column(&row, table_def, column_idx);
          }
          else
          {
            printf("N/A");
          }
          printf(" | ");
        }
      }
      else
      {
        // Print all columns
        for (uint32_t i = 0; i < table_def->num_columns; i++)
        {
          print_dynamic_column(&row, table_def, i);
          printf(" | ");
        }
      }
      printf("\n");

      row_count++;
      cursor_advance(cursor);
    }
  }

  dynamic_row_free(&row);
  free(cursor);

  // Free allocated memory for columns
  free_columns_to_select(statement);

  return EXECUTE_SUCCESS;
}

ExecuteResult execute_select_by_id(Statement *statement, Table *table)
{
  TableDef *table_def = catalog_get_active_table(&statement->db->catalog);
  if (!table_def)
  {
    return EXECUTE_UNRECOGNIZED_STATEMENT;
  }

  Cursor *cursor = table_find(table, statement->id_to_select);

  if (!cursor->end_of_table)
  {
    DynamicRow row;
    dynamic_row_init(&row, table_def);

    deserialize_dynamic_row(cursor_value(cursor), table_def, &row);

    if (statement->db->output_format == OUTPUT_FORMAT_JSON)
    {
      start_json_result();
      printf("    ");
      format_row_as_json(&row, table_def, NULL, 0); // Show all columns
      end_json_result(1);
    }
    else
    {
      print_dynamic_row(&row, table_def);
    }

    dynamic_row_free(&row);
  }
  else
  {
    if (statement->db->output_format == OUTPUT_FORMAT_JSON)
    {
      start_json_result();
      end_json_result(0);
    }
    else
    {
      printf("No row found with id %d\n", statement->id_to_select);
    }
  }

  free(cursor);
  return EXECUTE_SUCCESS;
}

ExecuteResult execute_update(Statement *statement, Table *table)
{
  // Find the row with the given id
  Cursor *cursor = table_find(table, statement->id_to_update);

  if (cursor->end_of_table)
  {
    printf("No row found with id %d\n", statement->id_to_update);
    free(cursor);
    return EXECUTE_SUCCESS;
  }

  // Get current row data
  Row row;
  deserialize_row(cursor_value(cursor), &row);

  // Update the appropriate field based on column name
  if (strcasecmp(statement->column_to_update, "name") == 0 ||
      strcasecmp(statement->column_to_update, "username") == 0)
  {
    strncpy(row.username, statement->update_value, COLUMN_USERNAME_SIZE);
  }
  else if (strcasecmp(statement->column_to_update, "email") == 0)
  {
    strncpy(row.email, statement->update_value, COLUMN_EMAIL_SIZE);
  }
  else
  {
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

  if (cursor->end_of_table)
  {
    printf("No row found with id %d\n", statement->id_to_delete);
    free(cursor);
    return EXECUTE_SUCCESS;
  }

  // Get the leaf node and number of cells
  void *node = get_page(table->pager, cursor->page_num);
  uint32_t num_cells = *leaf_node_num_cells(node);

  // Shift cells to overwrite the one being deleted
  if (cursor->cell_num < num_cells - 1)
  {
    // For variable-sized cells, we need to manually copy and shift each cell
    uint32_t current_pos = cursor->cell_num;

    // Get pointer to the cell being deleted
    void *cell_to_delete = leaf_node_cell(node, current_pos);

    // Get pointer to the next cell
    void *next_cell = leaf_node_next_cell(node, current_pos);

    // Calculate total size of data after this cell
    uint32_t bytes_to_move = 0;
    for (uint32_t i = current_pos + 1; i < num_cells; i++)
    {
      bytes_to_move += leaf_node_cell_size(node, i);
    }

    // Move all following cells backward
    if (bytes_to_move > 0)
    {
      memmove(cell_to_delete, next_cell, bytes_to_move);
    }
  }

  // Decrement number of cells
  (*leaf_node_num_cells(node))--;

  free(cursor);
  return EXECUTE_SUCCESS;
}

PrepareResult prepare_create_table(Input_Buffer *buf, Statement *statement)
{
  statement->type = STATEMENT_CREATE_TABLE;

  // Tokenize: CREATE TABLE name (col1 type1, col2 type2, ...)
  char *token = strtok(buf->buffer, " \t"); // Skip "CREATE"
  token = strtok(NULL, " \t");              // Skip "TABLE"
  token = strtok(NULL, " \t(");             // Get table name

  if (!token)
  {
    return PREPARE_SYNTAX_ERROR;
  }

  strncpy(statement->table_name, token, MAX_TABLE_NAME - 1);
  statement->table_name[MAX_TABLE_NAME - 1] = '\0';

  // Parse columns: col1 type1, col2 type2, ...
  statement->num_columns = 0;

  // Tokenize the column definitions
  char *col_name = strtok(NULL, " \t,)");
  col_name++; // Skip the opening parenthesis
  while (col_name && statement->num_columns < MAX_COLUMNS)
  {
    // Get column type
    char *col_type = strtok(NULL, " \t,)");
    if (!col_type)
    {
      return PREPARE_SYNTAX_ERROR;
    }

    ColumnDef *column = &statement->columns[statement->num_columns];
    strncpy(column->name, col_name, MAX_COLUMN_NAME - 1);
    column->name[MAX_COLUMN_NAME - 1] = '\0';

    if (strcasecmp(col_type, "INT") == 0)
    {
      column->type = COLUMN_TYPE_INT;
      column->size = sizeof(int32_t);
    }
    else if (strcasecmp(col_type, "FLOAT") == 0)
    {
      column->type = COLUMN_TYPE_FLOAT;
      column->size = sizeof(float);
    }
    else if (strcasecmp(col_type, "BOOLEAN") == 0)
    {
      column->type = COLUMN_TYPE_BOOLEAN;
      column->size = sizeof(uint8_t);
    }
    else if (strcasecmp(col_type, "DATE") == 0)
    {
      column->type = COLUMN_TYPE_DATE;
      column->size = sizeof(int32_t);
    }
    else if (strcasecmp(col_type, "TIME") == 0)
    {
      column->type = COLUMN_TYPE_TIME;
      column->size = sizeof(int32_t);
    }
    else if (strcasecmp(col_type, "TIMESTAMP") == 0)
    {
      column->type = COLUMN_TYPE_TIMESTAMP;
      column->size = sizeof(int64_t);
    }
    else if (strncasecmp(col_type, "BLOB", 4) == 0)
    {
      column->type = COLUMN_TYPE_BLOB;

      // Check if blob has a size: BLOB(n)
      char *size_start = strchr(col_type, '(');
      if (size_start)
      {
        char *size_end = strchr(size_start, ')');
        if (size_end)
        {
          *size_end = '\0';
          char *size_str = size_start + 1;
          column->size = atoi(size_str);
          if (column->size ==
              0)
          {                      // Set default if parsing failed or explicit 0
            column->size = 1024; // 1KB default
          }
        }
        else
        {
          column->size = 1024; // Default if parsing failed
        }
      }
      else
      {
        // Default blob size
        column->size = 1024; // 1KB default
      }
    }
    else if (strncasecmp(col_type, "STRING", 6) == 0)
    {
      column->type = COLUMN_TYPE_STRING;

      // Check if string has a size: STRING(n)
      char *size_start = strchr(col_type, '(');
      if (size_start)
      {
        char *size_end = strchr(size_start, ')');
        if (size_end)
        {
          *size_end = '\0';
          char *size_str = size_start + 1;
          column->size = atoi(size_str);
          if (column->size ==
              0)
          {                     // Set default if parsing failed or explicit 0
            column->size = 255; // Default string size
          }
        }
        else
        {
          column->size = 255; // Default if parsing failed
        }
      }
      else
      {
        // Default string size
        column->size = 255;
      }

      // No need to add space for null terminator - this is handled in
      // dynamic_row_init

#ifdef DEBUG
      printf("DEBUG: Set string column '%s' size to %u\n", column->name,
             column->size);
#endif
    }
    else
    {
      return PREPARE_SYNTAX_ERROR;
    }

    statement->num_columns++;
    col_name = strtok(NULL, " \t,)");
  }

  if (statement->num_columns == 0)
  {
    return PREPARE_SYNTAX_ERROR;
  }

  return PREPARE_SUCCESS;
}

PrepareResult prepare_use_table(Input_Buffer *buf, Statement *statement)
{
  statement->type = STATEMENT_USE_TABLE;

  // Parse: USE TABLE table_name
  char *token = strtok(buf->buffer, " \t"); // Skip "USE"
  token = strtok(NULL, " \t");              // Skip "TABLE"
  token = strtok(NULL, " \t");              // Get table name

  if (!token)
  {
    return PREPARE_SYNTAX_ERROR;
  }

  strncpy(statement->table_name, token, MAX_TABLE_NAME - 1);
  statement->table_name[MAX_TABLE_NAME - 1] = '\0';

  return PREPARE_SUCCESS;
}

// Fix unused parameter warning
PrepareResult prepare_show_tables(Input_Buffer *buf, Statement *statement)
{
  (void)buf; // Mark parameter as used
  statement->type = STATEMENT_SHOW_TABLES;
  return PREPARE_SUCCESS;
}

ExecuteResult execute_create_table(Statement *statement, Database *db)
{
  if (db_create_table(db, statement->table_name, statement->columns,
                      statement->num_columns))
  {
    printf("Table created: %s\n", statement->table_name);
    return EXECUTE_SUCCESS;
  }
  else
  {
    printf("Failed to create table: %s\n", statement->table_name);
    return EXECUTE_UNRECOGNIZED_STATEMENT;
  }
}

ExecuteResult execute_use_table(Statement *statement, Database *db)
{
  printf("Debug: Starting execute_use_table for table: %s\n", statement->table_name);

  if (!db)
  {
    printf("Debug: db is NULL\n");
    return EXECUTE_ERROR;
  }

  printf("Debug: Checking catalog\n");
  // Check if the table exists
  int table_idx = catalog_find_table(&db->catalog, statement->table_name);
  printf("Debug: Table index: %d\n", table_idx);

  if (table_idx == -1)
  {
    printf("Error: Table '%s' not found.\n", statement->table_name);
    return EXECUTE_TABLE_NOT_FOUND;
  }

  printf("Debug: Setting active table index\n");
  db->catalog.active_table = table_idx;

  printf("Debug: Checking existing active table\n");
  // If there's an existing active table, close it first
  if (db->active_table)
  {
    printf("Debug: Closing existing active table\n");
    db_close(db->active_table);
    db->active_table = NULL;
  }

  printf("Debug: Building table path\n");
  // Open the table file
  char table_path[512];
  snprintf(table_path, sizeof(table_path), "Database/%s/Tables/%s.tbl",
           db->name, statement->table_name);

  printf("Debug: Opening table at %s\n", table_path);
  db->active_table = db_open(table_path);
  if (!db->active_table)
  {
    printf("Error: Failed to open table '%s'.\n", statement->table_name);
    return EXECUTE_TABLE_OPEN_ERROR;
  }

  // Set the root page number from catalog
  db->active_table->root_page_num = db->catalog.tables[table_idx].root_page_num;

  // ADD THIS: Open all indexes associated with this table
  TableDef *table_def = &db->catalog.tables[table_idx];
  for (uint32_t i = 0; i < table_def->num_indexes; i++)
  {
    IndexDef *index_def = &table_def->indexes[i];
    printf("Debug: Opening index '%s' at %s\n", index_def->name, index_def->filename);

    // Open the index file
    Table *index_table = db_open(index_def->filename);
    if (!index_table)
    {
      printf("Warning: Failed to open index '%s' on table '%s'\n",
             index_def->name, table_def->name);
      continue;
    }

    // Set the root page number from catalog
    index_table->root_page_num = index_def->root_page_num;

    // We should store this open index table somewhere
    // For now, we'll just close it since we'll reopen when needed
    db_close(index_table);
  }

  printf("Debug: Saving table name\n");
  // Save the table name
  strncpy(db->active_table_name, statement->table_name, MAX_TABLE_NAME - 1);
  db->active_table_name[MAX_TABLE_NAME - 1] = '\0';

  printf("Using table: %s\n", statement->table_name);
  return EXECUTE_SUCCESS;
}
// Add the execute statement implementation
ExecuteResult execute_statement(Statement *statement, Database *db)
{
  // Handle authentication commands regardless of active table
  switch (statement->type) {
    case STATEMENT_LOGIN:
      return execute_login(statement, db);
      
    case STATEMENT_LOGOUT:
      return execute_logout(statement, db);
      
    case STATEMENT_CREATE_USER:
      return execute_create_user(statement, db);
  }

  // Check if we need to switch tables first, but skip this for CREATE TABLE
  if (statement->table_name[0] != '\0' &&
      statement->type != STATEMENT_CREATE_TABLE)
  {
    // A table was specified in the query

    // Check if we need to switch
    bool need_switch = true;
    if (db->active_table != NULL)
    {
      TableDef *active_table = catalog_get_active_table(&db->catalog);
      if (active_table &&
          strcmp(active_table->name, statement->table_name) == 0)
      {
        need_switch = false; // Already using this table
      }
    }

    // Switch tables if needed
    if (need_switch)
    {
      if (!db_use_table(db, statement->table_name))
      {
        printf("Table not found: %s\n", statement->table_name);
        return EXECUTE_UNRECOGNIZED_STATEMENT;
      }
      // Table successfully switched
    }
  }

  // Now execute the statement with the correct table active
  // First check permissions for operations
  bool requires_permission = true;
  const char* operation = NULL;
  
  switch (statement->type) {
    case STATEMENT_INSERT:
      operation = "INSERT";
      break;
      
    case STATEMENT_SELECT:
    case STATEMENT_SELECT_BY_ID:
      operation = "SELECT";
      break;
      
    case STATEMENT_UPDATE:
      operation = "UPDATE";
      break;
      
    case STATEMENT_DELETE:
      operation = "DELETE";
      break;
      
    case STATEMENT_CREATE_TABLE:
    case STATEMENT_CREATE_INDEX:
    case STATEMENT_CREATE_DATABASE:
      operation = "CREATE";
      break;
      
    case STATEMENT_USE_TABLE:
    case STATEMENT_USE_DATABASE:
    case STATEMENT_SHOW_TABLES:
    case STATEMENT_SHOW_INDEXES:
      operation = "SHOW";
      break;
      
    default:
      // For any other statements
      requires_permission = false;
      break;
  }
  
  if (requires_permission && !db_check_permission(db, operation)) {
    printf("Error: Permission denied for this operation.\n");
    printf("You don't have sufficient privileges. Please ask an admin for assistance.\n");
    return EXECUTE_PERMISSION_DENIED;
  }

  // Execute the statement based on type
  switch (statement->type)
  {
  case STATEMENT_INSERT:
    if (db->active_table == NULL)
    {
      printf("Error: No active table selected.\n");
      return EXECUTE_SUCCESS;
    }
    return execute_insert(statement, db->active_table);

  case STATEMENT_SELECT:
    if (db->active_table == NULL)
    {
      printf("Error: No active table selected.\n");
      return EXECUTE_SUCCESS;
    }
    return execute_select(statement, db->active_table);

  case STATEMENT_SELECT_BY_ID:
    if (db->active_table == NULL)
    {
      printf("Error: No active table selected.\n");
      return EXECUTE_SUCCESS;
    }
    return execute_select_by_id(statement, db->active_table);

  case STATEMENT_UPDATE:
    if (db->active_table == NULL)
    {
      printf("Error: No active table selected.\n");
      return EXECUTE_SUCCESS;
    }
    return execute_update(statement, db->active_table);

  case STATEMENT_DELETE:
    if (db->active_table == NULL)
    {
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

  case STATEMENT_CREATE_INDEX:
    return execute_create_index(statement, db);

  case STATEMENT_SHOW_INDEXES:
    return execute_show_indexes(statement, db);

  case STATEMENT_CREATE_DATABASE:
  case STATEMENT_USE_DATABASE:
    // These should be handled separately
    return EXECUTE_UNRECOGNIZED_STATEMENT;

  default:
    return EXECUTE_UNRECOGNIZED_STATEMENT;
  }
}

PrepareResult prepare_database_statement(Input_Buffer *buf,
                                         Statement *statement)
{
  if (strncasecmp(buf->buffer, "create database", 15) == 0)
  {
    statement->type = STATEMENT_CREATE_DATABASE;

    // Parse database name
    char *token = strtok(buf->buffer, " \t"); // Skip "CREATE"
    token = strtok(NULL, " \t");              // Skip "DATABASE"
    token = strtok(NULL, " \t");              // Get database name

    if (!token)
    {
      return PREPARE_SYNTAX_ERROR;
    }

    strncpy(statement->database_name, token,
            sizeof(statement->database_name) - 1);
    statement->database_name[sizeof(statement->database_name) - 1] = '\0';

    return PREPARE_SUCCESS;
  }
  else if (strncasecmp(buf->buffer, "use database", 12) == 0)
  {
    statement->type = STATEMENT_USE_DATABASE;

    // Parse database name
    char *token = strtok(buf->buffer, " \t"); // Skip "USE"
    token = strtok(NULL, " \t");              // Skip "DATABASE"
    token = strtok(NULL, " \t");              // Get database name

    if (!token)
    {
      return PREPARE_SYNTAX_ERROR;
    }

    strncpy(statement->database_name, token,
            sizeof(statement->database_name) - 1);
    statement->database_name[sizeof(statement->database_name) - 1] = '\0';

    return PREPARE_SUCCESS;
  }
  else if (strncasecmp(buf->buffer, "using database", 14) == 0)
  {
    // Helpful error message for common mistake
    printf("Did you mean 'USE DATABASE'? The correct syntax is 'USE DATABASE <name>'.\n");
    return PREPARE_SYNTAX_ERROR;
  }

  return PREPARE_UNRECOGNIZED_STATEMENT;
}

ExecuteResult execute_database_statement(Statement *statement,
                                         Database **db_ptr)
{
  switch (statement->type)
  {
  case STATEMENT_CREATE_DATABASE:
    {
      // Create the new database without closing the current one first
      Database *new_db = db_create_database(statement->database_name);
      if (!new_db)
      {
        return EXECUTE_UNRECOGNIZED_STATEMENT;
      }

      // Transfer authentication state if user is logged in
      if (*db_ptr && auth_is_logged_in(&(*db_ptr)->user_manager)) {
        // Get current username from old database
        const char* username = auth_get_current_username(&(*db_ptr)->user_manager);
        UserRole role = auth_get_current_role(&(*db_ptr)->user_manager);
        
        // Find user in new database and log them in automatically
        // We don't have the password, but we can set the current user directly
        for (uint32_t i = 0; i < new_db->user_manager.count; i++) {
          if (strcmp(new_db->user_manager.users[i].username, username) == 0) {
            new_db->user_manager.current_user_index = i;
            break;
          }
        }
      }

      // Only close the current database after successfully creating the new one
      if (*db_ptr)
      {
        db_close_database(*db_ptr);
      }
      *db_ptr = new_db;

      printf("Database created: %s\n", statement->database_name);
      return EXECUTE_SUCCESS;
    }

  case STATEMENT_USE_DATABASE:
    {
      // Open the new database without closing the current one first
      Database *new_db = db_open_database(statement->database_name);
      if (!new_db)
      {
        return EXECUTE_UNRECOGNIZED_STATEMENT;
      }

      // Transfer authentication state if user is logged in
      if (*db_ptr && auth_is_logged_in(&(*db_ptr)->user_manager)) {
        // Get current username from old database
        const char* username = auth_get_current_username(&(*db_ptr)->user_manager);
        
        // Find user in new database and log them in automatically
        for (uint32_t i = 0; i < new_db->user_manager.count; i++) {
          if (strcmp(new_db->user_manager.users[i].username, username) == 0) {
            new_db->user_manager.current_user_index = i;
            break;
          }
        }
      }

      // Only close the current database after successfully opening the new one
      if (*db_ptr)
      {
        db_close_database(*db_ptr);
      }
      *db_ptr = new_db;

      printf("Using database: %s\n", statement->database_name);
      return EXECUTE_SUCCESS;
    }

  default:
    return EXECUTE_UNRECOGNIZED_STATEMENT;
  }
}

/**
 * Creates a secondary index for the specified table and column.
 *
 * This function builds an auxiliary data structure to speed up queries
 * involving the indexed column. The secondary index allows for faster
 * lookups, range queries, and can improve overall query performance.
 */

// Prepare a CREATE INDEX statement
PrepareResult prepare_create_index(Input_Buffer *buf, Statement *statement)
{
  statement->type = STATEMENT_CREATE_INDEX;
  char *sql = buf->buffer;

  // Parse: CREATE INDEX index_name ON table_name (column_name)
  char *index_keyword = strcasestr(sql, "index");
  if (!index_keyword)
  {
    return PREPARE_SYNTAX_ERROR;
  }

  char *on_keyword = strcasestr(index_keyword, "on");
  if (!on_keyword)
  {
    return PREPARE_SYNTAX_ERROR;
  }

  // Extract index name between "index" and "on"
  char *index_name_start = index_keyword + 5; // Skip "index"
  while (*index_name_start == ' ')
    index_name_start++; // Skip spaces

  int index_name_len = on_keyword - index_name_start;
  // Trim trailing spaces
  while (index_name_len > 0 && index_name_start[index_name_len - 1] == ' ')
    index_name_len--;

  if (index_name_len <= 0 || index_name_len >= MAX_INDEX_NAME)
  {
    return PREPARE_SYNTAX_ERROR;
  }

  strncpy(statement->index_name, index_name_start, index_name_len);
  statement->index_name[index_name_len] = '\0';

  // Extract table name
  char *table_name_start = on_keyword + 2; // Skip "on"
  while (*table_name_start == ' ')
    table_name_start++; // Skip spaces

  char *paren_start = strchr(table_name_start, '(');
  if (!paren_start)
  {
    return PREPARE_SYNTAX_ERROR;
  }

  int table_name_len = paren_start - table_name_start;
  // Trim trailing spaces
  while (table_name_len > 0 && table_name_start[table_name_len - 1] == ' ')
    table_name_len--;

  if (table_name_len <= 0 || table_name_len >= MAX_TABLE_NAME)
  {
    return PREPARE_SYNTAX_ERROR;
  }

  strncpy(statement->table_name, table_name_start, table_name_len);
  statement->table_name[table_name_len] = '\0';

  // Extract column name
  char *column_name_start = paren_start + 1; // Skip "("
  while (*column_name_start == ' ')
    column_name_start++; // Skip spaces

  char *paren_end = strchr(column_name_start, ')');
  if (!paren_end)
  {
    return PREPARE_SYNTAX_ERROR;
  }

  int column_name_len = paren_end - column_name_start;
  // Trim trailing spaces
  while (column_name_len > 0 && column_name_start[column_name_len - 1] == ' ')
    column_name_len--;

  if (column_name_len <= 0 || column_name_len >= MAX_COLUMN_NAME)
  {
    return PREPARE_SYNTAX_ERROR;
  }

  strncpy(statement->where_column, column_name_start, column_name_len);
  statement->where_column[column_name_len] = '\0';

  return PREPARE_SUCCESS;
}

// Execute a CREATE INDEX statement
ExecuteResult execute_create_index(Statement *statement, Database *db)
{
  // First check if the table exists
  int table_idx = catalog_find_table(&db->catalog, statement->table_name);
  if (table_idx == -1)
  {
    printf("Error: Table '%s' not found.\n", statement->table_name);
    return EXECUTE_UNRECOGNIZED_STATEMENT;
  }

  // Add the index to the catalog
  if (!catalog_add_index(&db->catalog, statement->table_name,
                         statement->index_name, statement->where_column, false))
  {
    return EXECUTE_UNRECOGNIZED_STATEMENT;
  }

  // Load the current table definition
  TableDef *table_def = &db->catalog.tables[table_idx];

  // Get the index definition
  int index_idx = catalog_find_index(&db->catalog, statement->table_name, statement->index_name);
  if (index_idx == -1)
  {
    printf("Error: Failed to create index.\n");
    return EXECUTE_UNRECOGNIZED_STATEMENT;
  }

  IndexDef *index_def = &table_def->indexes[index_idx];

  // Create the index (build it by scanning the table)
  Table *table = db->active_table;
  if (!table || strcmp(table_def->name, statement->table_name) != 0)
  {
    // We need to temporarily open the table
    char table_path[512];
    snprintf(table_path, sizeof(table_path), "Database/%s/Tables/%s.tbl",
             db->name, statement->table_name);

    table = db_open(table_path);
    if (!table)
    {
      printf("Error: Failed to open table '%s'.\n", statement->table_name);
      return EXECUTE_UNRECOGNIZED_STATEMENT;
    }
  }

  bool result = create_secondary_index(table, table_def, index_def);

  // If this wasn't the active table, close it
  if (table != db->active_table)
  {
    db_close(table);
  }

  // Save the updated catalog
  catalog_save(&db->catalog, db->name);

  if (result)
  {
    printf("Index '%s' created on table '%s' for column '%s'.\n",
           statement->index_name, statement->table_name, statement->where_column);
    return EXECUTE_SUCCESS;
  }
  else
  {
    printf("Error: Failed to create index.\n");
    return EXECUTE_UNRECOGNIZED_STATEMENT;
  }
}

ExecuteResult execute_filtered_select(Statement *statement, Table *table)
{
  TableDef *table_def = catalog_get_active_table(&statement->db->catalog);
  if (!table_def)
  {
    return EXECUTE_UNRECOGNIZED_STATEMENT;
  }

  // Find column index for the where condition
  int where_column_idx = -1;
  for (uint32_t i = 0; i < table_def->num_columns; i++)
  {
    if (strcasecmp(table_def->columns[i].name, statement->where_column) == 0)
    {
      where_column_idx = i;
      break;
    }
  }

  if (where_column_idx == -1)
  {
    printf("Error: Column '%s' not found in table\n", statement->where_column);
    // Free allocated memory before returning
    free_columns_to_select(statement);
    return EXECUTE_UNRECOGNIZED_STATEMENT;
  }

  int row_count = 0;
  bool rows_found = false;
  bool show_query_plan = true; // Set to true to enable query plan logging

  // Special case: if filtering by ID, use the more efficient btree search
  if (strcasecmp(statement->where_column, "id") == 0 || where_column_idx == 0)
  {
    // Log query execution plan
    if (show_query_plan)
    {
      printf("QUERY PLAN: Using primary key B-tree index on column 'id'\n");
    }

    int id_value = atoi(statement->where_value);
    Cursor *cursor = table_find(table, id_value);

    if (!cursor->end_of_table)
    {
      DynamicRow row;
      dynamic_row_init(&row, table_def);

      deserialize_dynamic_row(cursor_value(cursor), table_def, &row);

      // Choose output format
      if (statement->db->output_format == OUTPUT_FORMAT_JSON)
      {
        start_json_result();
        printf("    ");
        format_row_as_json(&row, table_def, statement->columns_to_select,
                           statement->num_columns_to_select);
        end_json_result(1);
      }
      else
      {
        // Table format (existing implementation)
        // Print column names as header
        printf("| ");
        if (statement->num_columns_to_select > 0)
        {
          // Print only selected columns
          for (uint32_t i = 0; i < statement->num_columns_to_select; i++)
          {
            printf("%s | ", statement->columns_to_select[i]);
          }
        }
        else
        {
          // Print all column names
          for (uint32_t i = 0; i < table_def->num_columns; i++)
          {
            printf("%s | ", table_def->columns[i].name);
          }
        }
        printf("\n");

        // Print separator line
        for (uint32_t i = 0; i < (statement->num_columns_to_select > 0
                                  ? statement->num_columns_to_select
                                  : table_def->num_columns);
         i++)
        {
          printf("|-%s-", "----------");
        }
        printf("|\n");

        // Print row data
        printf("| ");

        if (statement->num_columns_to_select > 0)
        {
          // Print only selected columns
          for (uint32_t i = 0; i < statement->num_columns_to_select; i++)
          {
            // Find column index by name
            int column_idx = -1;
            for (uint32_t j = 0; j < table_def->num_columns; j++)
            {
              if (strcasecmp(table_def->columns[j].name,
                           statement->columns_to_select[i]) == 0)
              {
                column_idx = j;
                break;
              }
            }

            if (column_idx != -1)
            {
              print_dynamic_column(&row, table_def, column_idx);
            }
            else
            {
              printf("N/A");
            }
            printf(" | ");
          }
        }
        else
        {
          // Print all columns
          for (uint32_t i = 0; i < table_def->num_columns; i++)
          {
            print_dynamic_column(&row, table_def, i);
            printf(" | ");
          }
        }
        printf("\n");
        rows_found = true;
      }

      dynamic_row_free(&row);
    }
    free(cursor);
  }
  else
  {
    // For other columns, do a full table scan
    Cursor *cursor = table_start(table);
    DynamicRow row;
    dynamic_row_init(&row, table_def);

    // Choose output format
    if (statement->db->output_format == OUTPUT_FORMAT_JSON)
    {
      start_json_result();
      bool first_match = true;

      while (!(cursor->end_of_table))
      {
        void *value = cursor_value(cursor);
        deserialize_dynamic_row(value, table_def, &row);

        // Check if row matches condition
        bool row_matches = false;

        switch (table_def->columns[where_column_idx].type)
        {
          case COLUMN_TYPE_INT:
          {
            int col_value = dynamic_row_get_int(&row, table_def, where_column_idx);
            int where_value = atoi(statement->where_value);
            row_matches = (col_value == where_value);
            break;
          }
          case COLUMN_TYPE_STRING:
          {
            char *col_value = dynamic_row_get_string(&row, table_def, where_column_idx);
            row_matches = (strcasecmp(col_value, statement->where_value) == 0);
            break;
          }
          case COLUMN_TYPE_FLOAT:
          {
            float col_value = dynamic_row_get_float(&row, table_def, where_column_idx);
            float where_value = atof(statement->where_value);
            row_matches = (fabs(col_value - where_value) < 0.0001);
            break;
          }
          case COLUMN_TYPE_BOOLEAN:
          {
            bool col_value = dynamic_row_get_boolean(&row, table_def, where_column_idx);
            bool where_value = (strcasecmp(statement->where_value, "true") == 0 ||
                            strcmp(statement->where_value, "1") == 0);
            row_matches = (col_value == where_value);
            break;
          }
          default:
            row_matches = false;
            break;
        }

        if (row_matches)
        {
          rows_found = true;
          row_count++;

          if (!first_match)
          {
            printf(",\n    ");
          }
          else
          {
            printf("    ");
            first_match = false;
          }

          format_row_as_json(&row, table_def, statement->columns_to_select,
                         statement->num_columns_to_select);
        }

        cursor_advance(cursor);
      }

      end_json_result(row_count);
    }
    else
    {
      // TABLE FORMAT OUTPUT
      // Print column headers
      printf("| ");
      if (statement->num_columns_to_select > 0)
      {
        // Print only selected columns
        for (uint32_t i = 0; i < statement->num_columns_to_select; i++)
        {
          printf("%s | ", statement->columns_to_select[i]);
        }
      }
      else
      {
        // Print all column names
        for (uint32_t i = 0; i < table_def->num_columns; i++)
        {
          printf("%s | ", table_def->columns[i].name);
        }
      }
      printf("\n");

      // Print separator line
      printf("|");
      for (uint32_t i = 0; i < (statement->num_columns_to_select > 0
                              ? statement->num_columns_to_select
                              : table_def->num_columns);
           i++)
      {
        printf("------------|");
      }
      printf("\n");

      // Scan the table for matching rows
      while (!(cursor->end_of_table))
      {
        void *value = cursor_value(cursor);
        deserialize_dynamic_row(value, table_def, &row);

        // Check if row matches condition
        bool row_matches = false;

        switch (table_def->columns[where_column_idx].type)
        {
          case COLUMN_TYPE_INT:
          {
            int col_value = dynamic_row_get_int(&row, table_def, where_column_idx);
            int where_value = atoi(statement->where_value);
            row_matches = (col_value == where_value);
            break;
          }
          case COLUMN_TYPE_STRING:
          {
            char *col_value = dynamic_row_get_string(&row, table_def, where_column_idx);
            row_matches = (strcasecmp(col_value, statement->where_value) == 0);
            break;
          }
          case COLUMN_TYPE_FLOAT:
          {
            float col_value = dynamic_row_get_float(&row, table_def, where_column_idx);
            float where_value = atof(statement->where_value);
            row_matches = (fabs(col_value - where_value) < 0.0001);
            break;
          }
          case COLUMN_TYPE_BOOLEAN:
          {
            bool col_value = dynamic_row_get_boolean(&row, table_def, where_column_idx);
            bool where_value = (strcasecmp(statement->where_value, "true") == 0 ||
                                strcmp(statement->where_value, "1") == 0);
            row_matches = (col_value == where_value);
            break;
          }
          default:
            row_matches = false;
            break;
        }

        if (row_matches)
        {
          rows_found = true;
          row_count++;

          // Print row data
          printf("| ");

          if (statement->num_columns_to_select > 0)
          {
            // Print only selected columns
            for (uint32_t i = 0; i < statement->num_columns_to_select; i++)
            {
              // Find column index by name
              int column_idx = -1;
              for (uint32_t j = 0; j < table_def->num_columns; j++)
              {
                if (strcasecmp(table_def->columns[j].name, statement->columns_to_select[i]) == 0)
                {
                  column_idx = j;
                  break;
                }
              }

              if (column_idx != -1)
              {
                print_dynamic_column(&row, table_def, column_idx);
              }
              else
              {
                printf("N/A");
              }
              printf(" | ");
            }
          }
          else
          {
            // Print all columns
            for (uint32_t i = 0; i < table_def->num_columns; i++)
            {
              print_dynamic_column(&row, table_def, i);
              printf(" | ");
            }
          }
          printf("\n");
        }

        cursor_advance(cursor);
      }

      if (!rows_found)
      {
        printf("No matching records found.\n");
      }
    }

    dynamic_row_free(&row);
    free(cursor);
  }

  // Free allocated memory for columns
  free_columns_to_select(statement);
  return EXECUTE_SUCCESS;
}

ExecuteResult execute_show_tables(Statement *statement, Database *db)
{
  (void)statement; // Mark parameter as used

  printf("Tables in database %s:\n", db->name);

  if (db->catalog.num_tables == 0)
  {
    printf("  No tables found.\n");
  }
  else
  {
    for (uint32_t i = 0; i < db->catalog.num_tables; i++)
    {
      printf("  %s%s\n", db->catalog.tables[i].name,
             (i == db->catalog.active_table && db->active_table != NULL)
                 ? " (active)"
                 : "");
    }
  }

  return EXECUTE_SUCCESS;
}

PrepareResult prepare_show_indexes(Input_Buffer *buf, Statement *statement)
{
  statement->type = STATEMENT_SHOW_INDEXES;

  // Parse: SHOW INDEXES FROM table_name
  char *token = strtok(buf->buffer, " \t"); // Skip "SHOW"
  token = strtok(NULL, " \t");              // Skip "INDEXES"
  token = strtok(NULL, " \t");              // Skip "FROM"
  token = strtok(NULL, " \t");              // Get table name

  if (!token)
  {
    return PREPARE_SYNTAX_ERROR;
  }

  strncpy(statement->table_name, token, MAX_TABLE_NAME - 1);
  statement->table_name[MAX_TABLE_NAME - 1] = '\0';

  return PREPARE_SUCCESS;
}

ExecuteResult execute_show_indexes(Statement *statement, Database *db)
{
  // Find the table
  int table_idx = catalog_find_table(&db->catalog, statement->table_name);
  if (table_idx == -1)
  {
    printf("Error: Table '%s' not found.\n", statement->table_name);
    return EXECUTE_TABLE_NOT_FOUND;
  }

  TableDef *table_def = &db->catalog.tables[table_idx];

  printf("Indexes for table '%s':\n", table_def->name);
  printf("--------------------\n");

  if (table_def->num_indexes == 0)
  {
    printf("  No indexes found.\n");
  }
  else
  {
    printf("  %-20s | %-20s | %-10s\n", "NAME", "COLUMN", "UNIQUE");
    printf("  %-20s | %-20s | %-10s\n", "--------------------", "--------------------", "----------");

    for (uint32_t i = 0; i < table_def->num_indexes; i++)
    {
      IndexDef *index = &table_def->indexes[i];
      printf("  %-20s | %-20s | %-10s\n",
             index->name,
             index->column_name,
             index->is_unique ? "YES" : "NO");
    }
  }

  return EXECUTE_SUCCESS;
}