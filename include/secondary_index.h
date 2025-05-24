#ifndef SECONDARY_INDEX_H
#define SECONDARY_INDEX_H

#include "db_types.h"
#include <stdint.h>
#include <stdbool.h>

// Structure for secondary index entries
typedef struct
{
    uint32_t row_id;    // The primary key of the indexed row
    uint32_t key_size;  // Size of the indexed key data
    uint8_t key_data[]; // Flexible array member for key data
} SecondaryIndexEntry;

// Function declarations
bool catalog_add_index(Catalog *catalog, const char *table_name,
                       const char *index_name, const char *column_name,
                       bool is_unique);

int catalog_find_index(Catalog *catalog, const char *table_name, const char *index_name);

int catalog_find_index_by_column(Catalog *catalog, const char *table_name, const char *column_name);

bool create_secondary_index(Table *table, TableDef *table_def, IndexDef *index_def);

bool secondary_index_insert(Table *index_table, uint32_t hash_key, uint32_t row_id,
                            void *key_data, uint32_t key_size);

Cursor *secondary_index_find(Table *index_table, uint32_t hash_key);

bool secondary_index_delete(Table *index_table, uint32_t hash_key, uint32_t row_id);

uint32_t hash_key_for_value(void *key, uint32_t key_size);

void *get_column_value(DynamicRow *row, TableDef *table_def, uint32_t column_idx, uint32_t *size);

#endif // SECONDARY_INDEX_H