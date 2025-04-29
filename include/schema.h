#ifndef SCHEMA_H
#define SCHEMA_H

#include <stdint.h>

#define MAX_TABLE_NAME 64
#define MAX_COLUMN_NAME 64
#define MAX_TABLES 32
#define MAX_COLUMNS 16

typedef enum {
    COLUMN_TYPE_INT,
    COLUMN_TYPE_STRING,
    COLUMN_TYPE_FLOAT,
    COLUMN_TYPE_BOOLEAN,
    COLUMN_TYPE_DATE,
    COLUMN_TYPE_TIME,
    COLUMN_TYPE_TIMESTAMP,
    COLUMN_TYPE_BLOB
    // Add more types as needed
} ColumnType;

typedef struct {
    char name[MAX_COLUMN_NAME];
    ColumnType type;
    uint32_t size;  // Relevant for strings
} ColumnDef;

typedef struct {
    char name[MAX_TABLE_NAME];
    uint32_t num_columns;
    ColumnDef columns[MAX_COLUMNS];
    uint32_t root_page_num;  // Root page number for this table
    char filename[256];      // Data file for this table
} TableDef;

#endif // SCHEMA_H
