#ifndef COMMAND_PROCESSOR_H
#define COMMAND_PROCESSOR_H

#include "db_types.h"
#include "input_handling.h"
#include "secondary_index.h"
#include "table.h"
#include "catalog.h"
#include "database.h"

typedef enum
{
  META_COMMAND_SUCCESS,
  META_COMMAND_UNRECOGNIZED_COMMAND,
  META_COMMAND_TXN_BEGIN,
  META_COMMAND_TXN_COMMIT,
  META_COMMAND_TXN_ROLLBACK,
  META_COMMAND_TXN_STATUS
} MetaCommandResult;

typedef enum
{
  PREPARE_SUCCESS,
  PREPARE_NEGATIVE_ID,
  PREPARE_STRING_TOO_LONG,
  PREPARE_UNRECOGNIZED_STATEMENT,
  PREPARE_SYNTAX_ERROR
} PrepareResult;

typedef enum
{
  EXECUTE_SUCCESS,
  EXECUTE_DUPLICATE_KEY,
  EXECUTE_TABLE_FULL,
  // Add these new result types
  EXECUTE_ERROR,
  EXECUTE_TABLE_NOT_FOUND,
  EXECUTE_TABLE_OPEN_ERROR,
  EXECUTE_INDEX_ERROR,
  EXECUTE_PERMISSION_DENIED, // Add this missing enum value
  EXECUTE_AUTH_FAILED,       // Add another useful enum value
  // Any other existing values...
  EXECUTE_UNRECOGNIZED_STATEMENT
} ExecuteResult;

typedef enum
{
  STATEMENT_INSERT,
  STATEMENT_SELECT,
  STATEMENT_SELECT_BY_ID,
  STATEMENT_UPDATE,
  STATEMENT_DELETE,
  STATEMENT_CREATE_TABLE,
  STATEMENT_USE_TABLE,
  STATEMENT_SHOW_TABLES,
  STATEMENT_CREATE_DATABASE,
  STATEMENT_USE_DATABASE,
  STATEMENT_CREATE_INDEX,
  STATEMENT_SHOW_INDEXES,
  // Add new statement types for authentication
  STATEMENT_LOGIN,
  STATEMENT_LOGOUT,
  STATEMENT_CREATE_USER
} StatementType;

typedef enum
{
  WHERE_OP_EQUAL,
  WHERE_OP_GREATER,
  WHERE_OP_LESS,
  WHERE_OP_GREATER_EQUAL,
  WHERE_OP_LESS_EQUAL,
  WHERE_OP_NOT_EQUAL,
} WhereOperator;
typedef struct
{
  StatementType type;
  Row row_to_insert;
  uint32_t id_to_select;
  uint32_t id_to_update;
  uint32_t id_to_delete;

  // Fields for update operation
  char column_to_update[MAX_COLUMN_NAME];
  char update_value[COLUMN_EMAIL_SIZE]; // Using email size as it's larger

  // New fields for table operations
  char table_name[MAX_TABLE_NAME];
  ColumnDef columns[MAX_COLUMNS];
  uint32_t num_columns;

  // New fields for variable-column insert values
  char **values;
  uint32_t num_values;

  // Fields for database operations
  char database_name[256];

  // Reference to the database - needed for schema lookup
  Database *db;

  // Field for selecting specific column and improved where clause
  char **columns_to_select;
  uint32_t num_columns_to_select;
  char where_column[MAX_COLUMN_NAME];
  char where_value[COLUMN_EMAIL_SIZE];
  WhereOperator where_operator; 
  bool has_where_clause;

  // Fields for index operations
  char index_name[MAX_INDEX_NAME];
  bool use_index; // Flag to indicate if an index should be used for queries

  // Authentication fields
  char auth_username[64];
  char auth_password[64];
  UserRole auth_role;
} Statement;

void free_columns_to_select(Statement *statement);
// Meta command function
MetaCommandResult do_meta_command(Input_Buffer *buf, Database *db);

// Prepare statement functions
PrepareResult prepare_statement(Input_Buffer *buf, Statement *statement);
PrepareResult prepare_insert(Input_Buffer *buf, Statement *statement);
PrepareResult prepare_select(Input_Buffer *buf, Statement *statement);
PrepareResult prepare_create_table(Input_Buffer *buf, Statement *statement);
PrepareResult prepare_use_table(Input_Buffer *buf, Statement *statement);
PrepareResult prepare_show_tables(Input_Buffer *buf, Statement *statement);

// Database statement functions
PrepareResult prepare_database_statement(Input_Buffer *buf, Statement *statement);

// Execute statement functions
ExecuteResult execute_statement(Statement *statement, Database *db);
ExecuteResult execute_insert(Statement *statement, Table *table);
ExecuteResult execute_select(Statement *statement, Table *table);
ExecuteResult execute_filtered_select(Statement *statement, Table *table);
ExecuteResult execute_select_by_id(Statement *statement, Table *table);
ExecuteResult execute_update(Statement *statement, Table *table);
ExecuteResult execute_delete(Statement *statement, Table *table);
ExecuteResult execute_create_table(Statement *statement, Database *db);
ExecuteResult execute_use_table(Statement *statement, Database *db);
ExecuteResult execute_show_tables(Statement *statement, Database *db);
ExecuteResult execute_database_statement(Statement *statement, Database **db);
// Index operation functions
PrepareResult prepare_create_index(Input_Buffer *buf, Statement *statement);
ExecuteResult execute_create_index(Statement *statement, Database *db);

// Add these declarations
PrepareResult prepare_show_indexes(Input_Buffer *buf, Statement *statement);
ExecuteResult execute_show_indexes(Statement *statement, Database *db);
// Utility functions
void print_constants();

// Add new prepare functions for auth commands
PrepareResult prepare_login(Input_Buffer *buf, Statement *statement);
PrepareResult prepare_logout(Input_Buffer *buf, Statement *statement);
PrepareResult prepare_create_user(Input_Buffer *buf, Statement *statement);

// Add new execute functions for auth commands
ExecuteResult execute_login(Statement *statement, Database *db);
ExecuteResult execute_logout(Statement *statement, Database *db);
ExecuteResult execute_create_user(Statement *statement, Database *db);

// Process a command string for the server, writing output to response_buf
// db_ptr: pointer to the current Database* (may be updated, e.g. on CREATE/USE DATABASE)
// input_buf: reusable Input_Buffer for parsing
// response_buf: output buffer for response (should be zeroed before call)
// response_bufsize: size of response_buf
void process_command_for_server(const char *input, int input_size, Database **db_ptr, Input_Buffer *input_buf, char *response_buf, size_t response_bufsize);

#endif // COMMAND_PROCESSOR_H