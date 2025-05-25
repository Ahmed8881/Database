#ifndef DB_TYPES_H
#define DB_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Common constants
#define MAX_TABLE_NAME 64
#define MAX_COLUMN_NAME 64
#define MAX_INDEX_NAME 64
#define MAX_TABLES 32
#define MAX_COLUMNS 16
#define MAX_INDEXES_PER_TABLE 16
#define MAX_COLUMN_SIZE 256

// Forward declarations for other types
typedef struct Pager Pager;
typedef struct Table Table;
typedef struct Catalog Catalog;
typedef struct Cursor Cursor;
typedef struct TableDef TableDef;
typedef struct DynamicRow DynamicRow;

// JSON Response types
typedef struct {
  char *data;    // JSON String
  size_t length; // Length of the JSON string
  bool success;  // Indicates if the operation was successful
} JsonResponse;

// Define IndexType enum
typedef enum {
  INDEX_TYPE_BTREE = 0,
  // Future index types could be added here
} IndexType;

// Full definition of IndexDef (not just forward declaration)
typedef struct IndexDef {
  char name[MAX_INDEX_NAME];
  char column_name[MAX_COLUMN_NAME];
  IndexType type;
  uint32_t root_page_num;
  char filename[256];
  bool is_unique;
} IndexDef;

#endif // DB_TYPES_H
