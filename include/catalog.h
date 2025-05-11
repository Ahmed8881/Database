#ifndef CATALOG_H
#define CATALOG_H

#include <stdint.h>
#include <stdbool.h>
#include "schema.h"

typedef struct {
    TableDef tables[MAX_TABLES];
    uint32_t num_tables;
    uint32_t active_table;  // Index of the currently active table
    char database_name[256]; // Name of the database this catalog belongs to
} Catalog;

// Initialize catalog
void catalog_init(Catalog* catalog);

// Add a table definition to the catalog
bool catalog_add_table(Catalog* catalog, const char* name, ColumnDef* columns, uint32_t num_columns);

// Find a table by name
int catalog_find_table(Catalog* catalog, const char* name);

// Set active table by name
bool catalog_set_active_table(Catalog* catalog, const char* name);

// Get active table definition
TableDef* catalog_get_active_table(Catalog* catalog);

// Save catalog to disk
bool catalog_save(Catalog* catalog, const char* db_name);

// Load catalog from disk
bool catalog_load(Catalog* catalog, const char* db_name);

// Load catalog from a specific path
bool catalog_load_from_path(Catalog* catalog, const char* path);

#endif