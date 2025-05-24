#include "../include/secondary_index.h"
#include "../include/db_types.h"
#include "../include/catalog.h"
#include "../include/schema.h"
#include "../include/table.h"
#include "../include/btree.h"
#include "../include/cursor.h"
#include "../include/pager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Add index to catalog
bool catalog_add_index(Catalog *catalog, const char *table_name,
                       const char *index_name, const char *column_name,
                       bool is_unique)
{
    // Find the table
    int table_idx = catalog_find_table(catalog, table_name);
    if (table_idx == -1)
    {
        printf("Error: Table '%s' not found.\n", table_name);
        return false;
    }

    TableDef *table = &catalog->tables[table_idx];

    // Check if we've reached the maximum indexes
    if (table->num_indexes >= MAX_INDEXES_PER_TABLE)
    {
        printf("Error: Maximum number of indexes reached for table '%s'.\n", table_name);
        return false;
    }

    // Check if index name already exists
    for (uint32_t i = 0; i < table->num_indexes; i++)
    {
        if (strcmp(table->indexes[i].name, index_name) == 0)
        {
            printf("Error: Index '%s' already exists on table '%s'.\n", index_name, table_name);
            return false;
        }
    }

    // Add the index
    IndexDef *index = &table->indexes[table->num_indexes];
    memset(index, 0, sizeof(IndexDef));

    strncpy(index->name, index_name, MAX_INDEX_NAME - 1);
    index->name[MAX_INDEX_NAME - 1] = '\0';

    strncpy(index->column_name, column_name, MAX_COLUMN_NAME - 1);
    index->column_name[MAX_COLUMN_NAME - 1] = '\0';

    index->type = INDEX_TYPE_BTREE; // Use the enum value
    index->is_unique = is_unique;

    // Create the index filename using a temporary buffer
    char filename_buffer[512];
    snprintf(filename_buffer, sizeof(filename_buffer),
             "Database/%s/Tables/%s_%s.idx",
             catalog->database_name, table_name, index_name);
             
    // Copy to index->filename
    strncpy(index->filename, filename_buffer, sizeof(index->filename) - 1);
    index->filename[sizeof(index->filename) - 1] = '\0';

    // Increment the count
    table->num_indexes++;

    return true;
}

// Find index by name in a specific table
int catalog_find_index(Catalog *catalog, const char *table_name, const char *index_name)
{
    int table_idx = catalog_find_table(catalog, table_name);
    if (table_idx == -1)
    {
        return -1;
    }

    TableDef *table = &catalog->tables[table_idx];

    for (uint32_t i = 0; i < table->num_indexes; i++)
    {
        if (strcmp(table->indexes[i].name, index_name) == 0)
        {
            return i;
        }
    }

    return -1; // Index not found
}

// Find index by column name in a specific table
int catalog_find_index_by_column(Catalog *catalog, const char *table_name, const char *column_name)
{
    int table_idx = catalog_find_table(catalog, table_name);
    if (table_idx == -1)
    {
        return -1;
    }

    TableDef *table = &catalog->tables[table_idx];

    for (uint32_t i = 0; i < table->num_indexes; i++)
    {
        if (strcmp(table->indexes[i].column_name, column_name) == 0)
        {
            return i;
        }
    }

    return -1; // Index not found
}

// Create a secondary index by scanning the table
bool create_secondary_index(Table *table, TableDef *table_def, IndexDef *index_def)
{
    // Find the column index
    int column_idx = -1;
    for (uint32_t i = 0; i < table_def->num_columns; i++)
    {
        if (strcmp(table_def->columns[i].name, index_def->column_name) == 0)
        {
            column_idx = i;
            break;
        }
    }

    if (column_idx == -1)
    {
        printf("Error: Column '%s' not found.\n", index_def->column_name);
        return false;
    }

    // Open or create the index file
    Table *index_table = db_open(index_def->filename);
    if (!index_table)
    {
        printf("Error: Failed to create index file '%s'.\n", index_def->filename);
        return false;
    }

    // Scan the table and build the index
    Cursor *cursor = table_start(table);
    DynamicRow row;
    dynamic_row_init(&row, table_def);

    printf("Building index '%s' on column '%s'...\n",
           index_def->name, index_def->column_name);

    uint32_t records_indexed = 0;

    while (!cursor->end_of_table)
    {
        void *row_data = cursor_value(cursor);
        deserialize_dynamic_row(row_data, table_def, &row);

        // Get the primary key (row ID)
        uint32_t row_id = dynamic_row_get_int(&row, table_def, 0);

        // Get the column value and its size
        uint32_t key_size;
        void *key_data = get_column_value(&row, table_def, column_idx, &key_size);

        if (key_data)
        {
            // Hash the key to get a numeric index key
            uint32_t hash_key = hash_key_for_value(key_data, key_size);

            // Insert into the index
            secondary_index_insert(index_table, hash_key, row_id, key_data, key_size);
            records_indexed++;
        }

        cursor_advance(cursor);
    }

    // Save the root page number
    index_def->root_page_num = index_table->root_page_num;

    // Close the index
    db_close(index_table);

    dynamic_row_free(&row);
    free(cursor);

    printf("Index created with %u records.\n", records_indexed);

    return true;
}

// Insert a value into a secondary index
bool secondary_index_insert(Table *index_table, uint32_t hash_key, uint32_t row_id,
                            void *key_data, uint32_t key_size)
{
    // Create the secondary index entry
    uint32_t entry_size = sizeof(SecondaryIndexEntry) + key_size;
    SecondaryIndexEntry *entry = malloc(entry_size);
    if (!entry)
    {
        return false;
    }

    entry->row_id = row_id;
    entry->key_size = key_size;
    memcpy(entry->key_data, key_data, key_size);

    // Create a dynamic row for the B-tree
    DynamicRow index_row;
    index_row.data = entry;
    index_row.data_size = entry_size;

    // Create a dummy table definition for the index
    TableDef dummy_table_def;
    memset(&dummy_table_def, 0, sizeof(TableDef));

    // Find insertion point in the B-tree
    Cursor *cursor = table_find(index_table, hash_key);

    // Insert the entry
    leaf_node_insert(cursor, hash_key, &index_row, &dummy_table_def);

    free(cursor);
    free(entry);

    return true;
}

// Find rows using a secondary index
Cursor *secondary_index_find(Table *index_table, uint32_t hash_key)
{
    return table_find(index_table, hash_key);
}

// Delete a value from a secondary index
bool secondary_index_delete(Table *index_table, uint32_t hash_key, uint32_t row_id)
{
    Cursor *cursor = table_find(index_table, hash_key);

    if (cursor->end_of_table)
    {
        free(cursor);
        return false;
    }

    // Check if the row_id matches
    void *node = get_page(index_table->pager, cursor->page_num);
    void *value = leaf_node_value(node, cursor->cell_num);

    // Cast the value to our secondary index entry
    SecondaryIndexEntry *entry = (SecondaryIndexEntry *)value;

    // If this is the correct entry, delete it
    if (entry->row_id == row_id)
    {
        // We need to use the existing delete mechanism
        // This is similar to execute_delete in command_processor.c

        uint32_t num_cells = *leaf_node_num_cells(node);

        // Shift cells to overwrite the one being deleted
        if (cursor->cell_num < num_cells - 1)
        {
            void *cell_to_delete = leaf_node_cell(node, cursor->cell_num);
            void *next_cell = leaf_node_next_cell(node, cursor->cell_num);

            uint32_t bytes_to_move = 0;
            for (uint32_t i = cursor->cell_num + 1; i < num_cells; i++)
            {
                bytes_to_move += leaf_node_cell_size(node, i);
            }

            if (bytes_to_move > 0)
            {
                memmove(cell_to_delete, next_cell, bytes_to_move);
            }
        }

        // Decrement the number of cells
        (*leaf_node_num_cells(node))--;

        free(cursor);
        return true;
    }

    free(cursor);
    return false;
}

// Hash function for creating numeric keys from any data type
uint32_t hash_key_for_value(void *key, uint32_t key_size)
{
    uint32_t hash = 5381;
    uint8_t *bytes = (uint8_t *)key;

    for (uint32_t i = 0; i < key_size; i++)
    {
        hash = ((hash << 5) + hash) + bytes[i]; // hash * 33 + c
    }

    return hash;
}

// Get a column value and its size based on the column type
void *get_column_value(DynamicRow *row, TableDef *table_def, uint32_t column_idx, uint32_t *size)
{
    static int32_t int_value;
    static float float_value;

    ColumnType type = table_def->columns[column_idx].type;

    switch (type)
    {
    case COLUMN_TYPE_INT:
        int_value = dynamic_row_get_int(row, table_def, column_idx);
        *size = sizeof(int32_t);
        return &int_value;

    case COLUMN_TYPE_STRING:
        *size = strlen(dynamic_row_get_string(row, table_def, column_idx));
        return dynamic_row_get_string(row, table_def, column_idx);

    case COLUMN_TYPE_FLOAT:
        float_value = dynamic_row_get_float(row, table_def, column_idx);
        *size = sizeof(float);
        return &float_value;

        // Add support for other types as needed

    default:
        // Unsupported type
        *size = 0;
        return NULL;
    }
}