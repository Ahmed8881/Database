#ifndef DATABASE_H
#define DATABASE_H

#include "catalog.h"
#include "table.h"
#include "pager.h"
#include "schema.h"
#include "transaction.h"
<<<<<<< HEAD
#include "auth.h"  // Add this include

#define MAX_OPEN_INDEXES 16

typedef enum
{
    OUTPUT_FORMAT_TABLE,
    OUTPUT_FORMAT_JSON
} OutputFormat;

typedef struct
{
    Table *tables[MAX_OPEN_INDEXES];
    uint32_t count;
} OpenIndexes;

typedef struct
{
    char name[256];                 // Database name
    Catalog catalog;                // Catalog of tables
    Table *active_table;            // Currently active table
    TransactionManager txn_manager; // Transaction manager
    uint32_t active_txn_id;         // Currently active transaction
    OutputFormat output_format;     // Output format setting
    char active_table_name[MAX_TABLE_NAME];
    char table_directory[512];
    OpenIndexes active_indexes;     // Add this field
    UserManager user_manager;       // Add user management
=======
#include "acl.h"  // Include the new header

typedef struct {
    char name[256];         // Database name
    Catalog catalog;        // Catalog of tables
    Table* active_table;    // Currently active table
    TransactionManager txn_manager;  // Transaction manager
    uint32_t active_txn_id;          // Currently active transaction
    ACL acl;               // Access control list
    bool auth_required;    // Whether authentication is required
>>>>>>> bf47354 (Implement Access Control List (ACL) functionality with user authentication)
} Database;

// Create a database directory structure
Database *db_create_database(const char *name);

// Open or create a database
Database *db_open_database(const char *name);

// Create a new table in the database
bool db_create_table(Database *db, const char *name, ColumnDef *columns, uint32_t num_columns);

// Open a specific table in the database
bool db_use_table(Database *db, const char *table_name);

// Initialize transactions for the database
void db_init_transactions(Database *db, uint32_t capacity);

// Begin a new transaction
uint32_t db_begin_transaction(Database *db);

// Commit the current transaction
bool db_commit_transaction(Database *db);

// Rollback the current transaction
bool db_rollback_transaction(Database *db);

// Set the active transaction
bool db_set_active_transaction(Database *db, uint32_t txn_id);

// Enable transactions for the database
void db_enable_transactions(Database *db);

// Disable transactions for the database
void db_disable_transactions(Database *db);

// Enable authentication for the database
void db_enable_auth(Database* db);

// Disable authentication for the database
void db_disable_auth(Database* db);

// Close the database
void db_close_database(Database *db);

// Add new function prototypes for authentication
bool db_login(Database *db, const char *username, const char *password);
void db_logout(Database *db);
bool db_create_user(Database *db, const char *username, const char *password, UserRole role);
bool db_is_authenticated(Database *db);
bool db_check_permission(Database *db, const char *operation);

#endif