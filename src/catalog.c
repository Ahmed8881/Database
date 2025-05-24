#include "../include/catalog.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void catalog_init(Catalog *catalog)
{
    catalog->num_tables = 0;
    catalog->active_table = 0;
    catalog->database_name[0] = '\0'; // Initialize database name as empty
}

bool catalog_add_table(Catalog *catalog, const char *name, ColumnDef *columns, uint32_t num_columns)
{
    if (catalog->num_tables >= MAX_TABLES)
    {
        return false;
    }

    int idx = catalog_find_table(catalog, name);
    if (idx != -1)
    {
        // Table with this name already exists
        return false;
    }

    uint32_t table_idx = catalog->num_tables;
    TableDef *table = &catalog->tables[table_idx];

    strncpy(table->name, name, MAX_TABLE_NAME);
    table->name[MAX_TABLE_NAME - 1] = '\0';

    table->num_columns = num_columns;
    for (uint32_t i = 0; i < num_columns; i++)
    {
        table->columns[i] = columns[i];

        // Verify column sizes are reasonable
        if (table->columns[i].type == COLUMN_TYPE_STRING && table->columns[i].size == 0)
        {
            printf("WARNING: Column %s has size 0, setting to default 255\n", table->columns[i].name);
            table->columns[i].size = 255;
        }

        if (table->columns[i].type == COLUMN_TYPE_BLOB && table->columns[i].size == 0)
        {
            printf("WARNING: BLOB column %s has size 0, setting to default 1024\n", table->columns[i].name);
            table->columns[i].size = 1024;
        }
    }

    // Fix the potential buffer overflow warning - use a fixed buffer size
    char filename_buffer[512]; // Use a larger buffer
    snprintf(filename_buffer, sizeof(filename_buffer), "Database/%s/Tables/%s.tbl",
             catalog->database_name, name);

    // Copy safely to the destination with truncation check
    if (strlen(filename_buffer) >= sizeof(table->filename))
    {
        printf("Warning: Path too long, truncating: %s\n", filename_buffer);
    }
    strncpy(table->filename, filename_buffer, sizeof(table->filename) - 1);
    table->filename[sizeof(table->filename) - 1] = '\0';

    catalog->num_tables++;
    return true;
}

int catalog_find_table(Catalog *catalog, const char *name)
{
    for (uint32_t i = 0; i < catalog->num_tables; i++)
    {
        if (strcmp(catalog->tables[i].name, name) == 0)
        {
            return i;
        }
    }
    return -1;
}

bool catalog_set_active_table(Catalog *catalog, const char *name)
{
    int idx = catalog_find_table(catalog, name);
    if (idx == -1)
    {
        return false;
    }

    catalog->active_table = idx;
    return true;
}

TableDef *catalog_get_active_table(Catalog *catalog)
{
    if (catalog->num_tables == 0)
    {
        return NULL;
    }
    return &catalog->tables[catalog->active_table];
}

bool catalog_save(Catalog *catalog, const char *db_name)
{
    char filename[512];
    snprintf(filename, sizeof(filename), "Database/%s/%s.catalog", db_name, db_name);

    FILE *file = fopen(filename, "wb");
    if (!file)
    {
        return false;
    }

    // Write number of tables
    fwrite(&catalog->num_tables, sizeof(uint32_t), 1, file);

    // Write active table index
    fwrite(&catalog->active_table, sizeof(uint32_t), 1, file);

    // Write database name
    fwrite(catalog->database_name, sizeof(catalog->database_name), 1, file);

    // Write each table definition
    for (uint32_t i = 0; i < catalog->num_tables; i++)
    {
        TableDef *table = &catalog->tables[i];

        // Write table name
        fwrite(table->name, sizeof(char), MAX_TABLE_NAME, file);

        // Write number of columns
        fwrite(&table->num_columns, sizeof(uint32_t), 1, file);

        // Write each column
        for (uint32_t j = 0; j < table->num_columns; j++)
        {
            ColumnDef *col = &table->columns[j];
            fwrite(col->name, sizeof(char), MAX_COLUMN_NAME, file);
            fwrite(&col->type, sizeof(ColumnType), 1, file);
            fwrite(&col->size, sizeof(uint32_t), 1, file);
        }

        // Write root page number
        fwrite(&table->root_page_num, sizeof(uint32_t), 1, file);

        // Write filename
        fwrite(table->filename, sizeof(char), 256, file);

        // ADD THIS: Write index information
        fwrite(&table->num_indexes, sizeof(uint32_t), 1, file);
        for (uint32_t j = 0; j < table->num_indexes; j++)
        {
            IndexDef *index = &table->indexes[j];
            fwrite(index->name, sizeof(char), MAX_INDEX_NAME, file);
            fwrite(index->column_name, sizeof(char), MAX_COLUMN_NAME, file);
            fwrite(&index->type, sizeof(IndexType), 1, file);
            fwrite(&index->root_page_num, sizeof(uint32_t), 1, file);
            fwrite(index->filename, sizeof(char), 256, file);
            fwrite(&index->is_unique, sizeof(bool), 1, file);
        }
    }

    fclose(file);
    return true;
}

bool catalog_load(Catalog *catalog, const char *db_name)
{
    char filename[512];
    snprintf(filename, sizeof(filename), "Database/%s/%s.catalog", db_name, db_name);

    FILE *file = fopen(filename, "rb");
    if (!file)
    {
        // If catalog file doesn't exist, initialize an empty catalog
        catalog_init(catalog);
        // Store database name
        strncpy(catalog->database_name, db_name, sizeof(catalog->database_name) - 1);
        catalog->database_name[sizeof(catalog->database_name) - 1] = '\0';
        return true;
    }

    // Read number of tables
    fread(&catalog->num_tables, sizeof(uint32_t), 1, file);

    // Read active table index
    fread(&catalog->active_table, sizeof(uint32_t), 1, file);

    // Read database name
    fread(catalog->database_name, sizeof(catalog->database_name), 1, file);

    // Read each table definition
    for (uint32_t i = 0; i < catalog->num_tables; i++)
    {
        TableDef *table = &catalog->tables[i];

        // Read table name
        fread(table->name, sizeof(char), MAX_TABLE_NAME, file);

        // Read number of columns
        fread(&table->num_columns, sizeof(uint32_t), 1, file);

        // Read column definitions
        for (uint32_t j = 0; j < table->num_columns; j++)
        {
            ColumnDef *col = &table->columns[j];

            // Read column name
            fread(col->name, sizeof(char), MAX_COLUMN_NAME, file);

            // Read column type
            fread(&col->type, sizeof(ColumnType), 1, file);

            // Read column size
            fread(&col->size, sizeof(uint32_t), 1, file);
        }

        // Read root page number
        fread(&table->root_page_num, sizeof(uint32_t), 1, file);

        // Read filename
        fread(table->filename, sizeof(char), 256, file);

        // ADD THIS: Read index information
        fread(&table->num_indexes, sizeof(uint32_t), 1, file);
        for (uint32_t j = 0; j < table->num_indexes; j++)
        {
            IndexDef *index = &table->indexes[j];
            fread(index->name, sizeof(char), MAX_INDEX_NAME, file);
            fread(index->column_name, sizeof(char), MAX_COLUMN_NAME, file);
            fread(&index->type, sizeof(IndexType), 1, file);
            fread(&index->root_page_num, sizeof(uint32_t), 1, file);
            fread(index->filename, sizeof(char), 256, file);
            fread(&index->is_unique, sizeof(bool), 1, file);
        }
    }

    fclose(file);
    return true;
}

bool catalog_load_from_path(Catalog *catalog, const char *path)
{
    FILE *file = fopen(path, "rb");
    if (!file)
    {
        // If catalog file doesn't exist, initialize an empty catalog
        catalog_init(catalog);

        // Extract database name from path (format: Database/<db_name>/<db_name>.catalog)
        const char *db_start = strstr(path, "Database/");
        if (db_start)
        {
            db_start += 9; // Skip "Database/"
            const char *db_end = strchr(db_start, '/');
            if (db_end)
            {
                size_t db_name_len = db_end - db_start;
                if (db_name_len < sizeof(catalog->database_name))
                {
                    strncpy(catalog->database_name, db_start, db_name_len);
                    catalog->database_name[db_name_len] = '\0';
                }
            }
        }

        return true;
    }

    // Read number of tables
    fread(&catalog->num_tables, sizeof(uint32_t), 1, file);

    // Read active table index
    fread(&catalog->active_table, sizeof(uint32_t), 1, file);

    // Read database name
    fread(catalog->database_name, sizeof(catalog->database_name), 1, file);

    // Read each table definition
    for (uint32_t i = 0; i < catalog->num_tables; i++)
    {
        TableDef *table = &catalog->tables[i];

        // Read table name
        fread(table->name, sizeof(char), MAX_TABLE_NAME, file);

        // Read number of columns
        fread(&table->num_columns, sizeof(uint32_t), 1, file);

        // Read column definitions
        for (uint32_t j = 0; j < table->num_columns; j++)
        {
            ColumnDef *col = &table->columns[j];

            // Read column name
            fread(col->name, sizeof(char), MAX_COLUMN_NAME, file);

            // Read column type
            fread(&col->type, sizeof(ColumnType), 1, file);

            // Read column size
            fread(&col->size, sizeof(uint32_t), 1, file);
        }

        // Read root page number
        fread(&table->root_page_num, sizeof(uint32_t), 1, file);

        // Read filename
        fread(table->filename, sizeof(char), 256, file);

        // ADD THIS: Read index information
        fread(&table->num_indexes, sizeof(uint32_t), 1, file);
        for (uint32_t j = 0; j < table->num_indexes; j++)
        {
            IndexDef *index = &table->indexes[j];
            fread(index->name, sizeof(char), MAX_INDEX_NAME, file);
            fread(index->column_name, sizeof(char), MAX_COLUMN_NAME, file);
            fread(&index->type, sizeof(IndexType), 1, file);
            fread(&index->root_page_num, sizeof(uint32_t), 1, file);
            fread(index->filename, sizeof(char), 256, file);
            fread(&index->is_unique, sizeof(bool), 1, file);
        }
    }

    fclose(file);
    return true;
}