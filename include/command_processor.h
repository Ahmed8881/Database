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
<<<<<<< HEAD
  // Add these new result types
  EXECUTE_ERROR,
  EXECUTE_TABLE_NOT_FOUND,
  EXECUTE_TABLE_OPEN_ERROR,
  EXECUTE_INDEX_ERROR,
  EXECUTE_PERMISSION_DENIED, // Add this missing enum value
  EXECUTE_AUTH_FAILED,       // Add another useful enum value
  // Any other existing values...
  EXECUTE_UNRECOGNIZED_STATEMENT
=======
  EXECUTE_UNRECOGNIZED_STATEMENT,
  EXECUTE_DUPLICATE_KEY,
  EXECUTE_FAILED  // Add this new value
>>>>>>> bf47354 (Implement Access Control List (ACL) functionality with user authentication)
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
<<<<<<< HEAD
  STATEMENT_CREATE_INDEX,
  STATEMENT_SHOW_INDEXES,
  // Add new statement types for authentication
  STATEMENT_LOGIN,
  STATEMENT_LOGOUT,
  STATEMENT_CREATE_USER
=======
  STATEMENT_CREATE_USER,
  STATEMENT_DROP_USER,
  STATEMENT_GRANT_ROLE,
  STATEMENT_REVOKE_ROLE, 
  STATEMENT_LOGIN,
  STATEMENT_LOGOUT,
  STATEMENT_ENABLE_AUTH,
  STATEMENT_DISABLE_AUTH
>>>>>>> bf47354 (Implement Access Control List (ACL) functionality with user authentication)
} StatementType;

typedef struct
{
  StatementType type;
  Row row_to_insert;
  uint32_t id_to_select;
  uint32_t id_to_delete;
<<<<<<< HEAD

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
  bool has_where_clause;

  // Fields for index operations
  char index_name[MAX_INDEX_NAME];
  bool use_index; // Flag to indicate if an index should be used for queries

  // Authentication fields
  char auth_username[64];
  char auth_password[64];
  UserRole auth_role;
=======
  uint32_t id_to_update;
  
  // For table operations
  char table_name[MAX_TABLE_NAME];
  ColumnDef columns[MAX_COLUMNS];
  uint32_t num_columns;
  
  // For database operations
  char database_name[256];
  
  // For update operations
  char column_to_update[MAX_COLUMN_NAME];
  char update_value[COLUMN_EMAIL_SIZE];
  
  // For dynamic value handling
  char** values;
  uint32_t num_values;
  
  // Reference to database
  Database* db;
  
  // For user management
  struct {
    char username[MAX_USERNAME_SIZE];
    char password[MAX_PASSWORD_SIZE];
    RoleType role;
    bool role_specified;  // Add this field to track if a role was specified
    char role_str[32];    // Add a string field to store the role name
  } user;
>>>>>>> bf47354 (Implement Access Control List (ACL) functionality with user authentication)
} Statement;

// Function to free allocated column selections
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

<<<<<<< HEAD
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

#endif // COMMAND_PROCESSOR_H
=======
// ACL functions
bool check_permission(Database* db, Statement* statement);
PrepareResult prepare_create_user(Input_Buffer *buf, Statement *statement);
PrepareResult prepare_drop_user(Input_Buffer *buf, Statement *statement);
PrepareResult prepare_grant_role(Input_Buffer *buf, Statement *statement);
PrepareResult prepare_revoke_role(Input_Buffer *buf, Statement *statement);
PrepareResult prepare_login(Input_Buffer *buf, Statement *statement);
PrepareResult prepare_logout(Input_Buffer *buf, Statement *statement);

ExecuteResult execute_create_user(Statement *statement, Database *db);
ExecuteResult execute_drop_user(Statement *statement, Database *db);
ExecuteResult execute_grant_role(Statement *statement, Database *db);
ExecuteResult execute_revoke_role(Statement *statement, Database *db);
ExecuteResult execute_login(Statement *statement, Database *db);
ExecuteResult execute_logout(Statement *statement, Database *db);
ExecuteResult execute_enable_auth(Statement *statement, Database *db);
ExecuteResult execute_disable_auth(Statement *statement, Database *db);

// Utility functions
void print_constants();

// Add this function declaration to support user role checking
RoleType acl_get_user_role(const ACL* acl, const char* username);

#endif
>>>>>>> bf47354 (Implement Access Control List (ACL) functionality with user authentication)
