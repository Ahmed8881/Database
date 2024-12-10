#ifndef TABLE_H
#define TABLE_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COLUMN_USERNAME_SIZE 32
#define COLUMN_EMAIL_SIZE 255

typedef struct
{
  uint32_t id;
  char username[COLUMN_USERNAME_SIZE + 1];
  char email[COLUMN_EMAIL_SIZE + 1];
} Row;

#define size_of_attribute(Struct, Attribute) sizeof(((Struct *)0)->Attribute)

extern const uint32_t ID_SIZE;
extern const uint32_t USERNAME_SIZE;
extern const uint32_t EMAIL_SIZE;
extern const uint32_t ID_OFFSET;
extern const uint32_t USERNAME_OFFSET;
extern const uint32_t EMAIL_OFFSET;
extern const uint32_t ROW_SIZE;

extern const uint32_t PAGE_SIZE;
#define TABLE_MAX_PAGES 100
extern const uint32_t ROWS_PER_PAGE;
extern const uint32_t TABLE_MAX_ROWS;

void serialize_row(Row *source, void *destination);
void deserialize_row(void *source, Row *destination);
void print_row(Row *row);

// struc for pages
typedef struct
{
  int file_descriptor;          // basically number return by os when file is opened that if read or write to file
  uint32_t file_length;         // length of file
  void *pages[TABLE_MAX_PAGES]; // array of pointrs where each pointer refers to a page and which takes data from disk as needed
} Pager;

typedef struct
{
  uint32_t num_rows;
  // void *pages[TABLE_MAX_PAGES];
  Pager *pager;
} Table;

Table *new_table();
void free_table(Table *table);
void *row_slot(Table *table, uint32_t row_num);
Table *db_open(const char *file_name);
Pager *pager_open(const char *file_name);
void *get_page(Pager *pager, uint32_t page_num);
void pager_flush(Pager *pager, uint32_t page_num, uint32_t size);
void db_close(Table *table);

#endif
