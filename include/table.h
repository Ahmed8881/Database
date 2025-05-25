#ifndef TABLE_H
#define TABLE_H

#include "db_types.h" // Include common type definitions
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pager.h"
#include "schema.h"

// Remove the forward declaration since we're including schema.h
// struct TableDef;
// typedef struct TableDef TableDef;

#define COLUMN_USERNAME_SIZE 32
#define COLUMN_EMAIL_SIZE 255

#pragma pack(push, 1)
// DEPRECATED: This fixed structure should be replaced with DynamicRow
// Only kept for backward compatibility
typedef struct
{
  uint32_t id;
  char username[COLUMN_USERNAME_SIZE + 1];
  char email[COLUMN_EMAIL_SIZE + 1];
} Row;

struct DynamicRow
{
  void *data;
  uint32_t data_size;
};
#pragma pack(pop)

#define size_of_attribute(Struct, Attribute) sizeof(((Struct *)0)->Attribute)

extern const uint32_t ID_SIZE;
extern const uint32_t USERNAME_SIZE;
extern const uint32_t EMAIL_SIZE;
extern const uint32_t ID_OFFSET;
extern const uint32_t USERNAME_OFFSET;
extern const uint32_t EMAIL_OFFSET;
extern const uint32_t ROW_SIZE;

extern const uint32_t PAGE_SIZE;

extern const uint32_t ROWS_PER_PAGE;
extern const uint32_t TABLE_MAX_ROWS;

void serialize_row(Row *source, void *destination);
void deserialize_row(void *source, Row *destination);
void print_row(Row *row);

struct Table
{
  Pager *pager;
  uint32_t root_page_num;
};

Table *new_table();
void free_table(Table *table);
void *row_slot(Table *table, uint32_t row_num);
Table *db_open(const char *file_name);

void db_close(Table *table);
// Functions to work with dynamic rows
void dynamic_row_init(DynamicRow *row, TableDef *table_def);
void dynamic_row_set_int(DynamicRow *row, TableDef *table_def, uint32_t col_idx, int32_t value);
void dynamic_row_set_string(DynamicRow *row, TableDef *table_def, uint32_t col_idx, const char *value);
void dynamic_row_set_float(DynamicRow *row, TableDef *table_def, uint32_t col_idx, float value);
void dynamic_row_set_boolean(DynamicRow *row, TableDef *table_def, uint32_t col_idx, bool value);
void dynamic_row_set_date(DynamicRow *row, TableDef *table_def, uint32_t col_idx, int32_t value);
void dynamic_row_set_time(DynamicRow *row, TableDef *table_def, uint32_t col_idx, int32_t value);
void dynamic_row_set_timestamp(DynamicRow *row, TableDef *table_def, uint32_t col_idx, int64_t value);
void dynamic_row_set_blob(DynamicRow *row, TableDef *table_def, uint32_t col_idx, const void *data, uint32_t size);

int32_t dynamic_row_get_int(DynamicRow *row, TableDef *table_def, uint32_t col_idx);
char *dynamic_row_get_string(DynamicRow *row, TableDef *table_def, uint32_t col_idx);
float dynamic_row_get_float(DynamicRow *row, TableDef *table_def, uint32_t col_idx);
bool dynamic_row_get_boolean(DynamicRow *row, TableDef *table_def, uint32_t col_idx);
int32_t dynamic_row_get_date(DynamicRow *row, TableDef *table_def, uint32_t col_idx);
int32_t dynamic_row_get_time(DynamicRow *row, TableDef *table_def, uint32_t col_idx);
int64_t dynamic_row_get_timestamp(DynamicRow *row, TableDef *table_def, uint32_t col_idx);
void *dynamic_row_get_blob(DynamicRow *row, TableDef *table_def, uint32_t col_idx, uint32_t *size);

void dynamic_row_free(DynamicRow *row);

// Update table functions to work with the new row structure
void serialize_dynamic_row(DynamicRow *source, TableDef *table_def, void *destination);
void deserialize_dynamic_row(void *source, TableDef *table_def, DynamicRow *destination);
void print_dynamic_row(DynamicRow *row, TableDef *table_def);
void print_dynamic_column(DynamicRow *row, TableDef *table_def, uint32_t col_idx, char *buf, size_t buf_size);
#endif