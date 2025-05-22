#ifndef COMMAND_PROCESSOR_H
#define COMMAND_PROCESSOR_H

#include "input_handling.h"
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
  EXECUTE_TABLE_FULL,
  EXECUTE_UNRECOGNIZED_STATEMENT,
  EXECUTE_DUPLICATE_KEY
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
  STATEMENT_USE_DATABASE
} StatementType;

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
  char** values;
  uint32_t num_values;
  
  // Fields for database operations
  char database_name[256];
  
  // Reference to the database - needed for schema lookup
  Database *db;
  
  // Field for selecting specific column and improved where clause
  char** columns_to_select;
  uint32_t num_columns_to_select;
  char where_column[MAX_COLUMN_NAME];
  char where_value[COLUMN_EMAIL_SIZE];
  bool has_where_clause;
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
ExecuteResult execute_create_table(Statement* statement, Database* db);
ExecuteResult execute_use_table(Statement* statement, Database* db);
ExecuteResult execute_show_tables(Statement* statement, Database* db);
ExecuteResult execute_database_statement(Statement *statement, Database **db);

// Utility functions
void print_constants();

#endif