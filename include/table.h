#ifndef TABLE_H
#define TABLE_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pager.h"

#define COLUMN_USERNAME_SIZE 32
#define COLUMN_EMAIL_SIZE 255

#pragma pack(push, 1) // due to alignment issues or additional padding in the structure caused by compiler-specific alignment rules.
// basically allows you to specify the alignment boundary for the members of a structure, which can help in reducing the size of the structure by eliminating padding bytes
typedef struct
{
  uint32_t id;
  char username[COLUMN_USERNAME_SIZE + 1];
  char email[COLUMN_EMAIL_SIZE + 1];
} Row;
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

typedef struct
{
  uint32_t root_page_num;
  Pager *pager;
} Table;

Table *new_table();
void free_table(Table *table);
void *row_slot(Table *table, uint32_t row_num);
Table *db_open(const char *file_name);

void db_close(Table *table);

#endif