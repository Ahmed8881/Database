#ifndef DATABASE_H
#define DATABASE_H

#include "catalog.h"
#include "table.h"
#include "pager.h"
#include "schema.h"

typedef struct {
    char name[256];         // Database name
    Catalog catalog;        // Catalog of tables
    Table* active_table;    // Currently active table
} Database;

// Create a database directory structure
Database* db_create_database(const char* name);

// Open or create a database
Database* db_open_database(const char* name);

// Create a new table in the database
bool db_create_table(Database* db, const char* name, ColumnDef* columns, uint32_t num_columns);

// Open a specific table in the database
bool db_use_table(Database* db, const char* table_name);

// Close the database
void db_close_database(Database* db);

#endif