#ifndef SCHEMA_H
#define SCHEMA_H

#include "db_types.h" // This now has the full IndexDef definition
#include <stdint.h>

// Constants are now in db_types.h
// No need to redefine them here

typedef enum
{
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

typedef struct
{
  char name[MAX_COLUMN_NAME];
  ColumnType type;
  uint32_t size; // Relevant for strings
} ColumnDef;

struct TableDef
{
  char name[MAX_TABLE_NAME];
  uint32_t num_columns;
  ColumnDef columns[MAX_COLUMNS];
  uint32_t root_page_num; // Root page number for this table
  char filename[256];     // Data file for this table

  // Add secondary index support
  uint32_t num_indexes;
  IndexDef indexes[MAX_INDEXES_PER_TABLE]; // IndexDef is now fully defined in db_types.h
};

#endif // SCHEMA_H
