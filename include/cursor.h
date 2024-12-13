#ifndef CURSOR_H
#define CURSOR_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "table.h"

// Cursor struct
typedef struct
{
   Table *table;
   uint32_t page_num;
   uint32_t cell_num;
   bool end_of_table;
} Cursor;

Cursor *table_start(Table *table);
// remove table_end
// instead we use method that searches for the given key defined in btree.h
void *cursor_value(Cursor *cursor);
void cursor_advance(Cursor *cursor);
#endif // CURSOR_H