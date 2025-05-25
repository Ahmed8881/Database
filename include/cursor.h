#ifndef CURSOR_H
#define CURSOR_H

#include "db_types.h" // Include common type definitions
#include <stdint.h>
#include <stdbool.h>

struct Cursor
{
   Table *table;
   uint32_t page_num;
   uint32_t cell_num;
   bool end_of_table;
};

Cursor *table_start(Table *table);
// remove table_end
// instead we use method that searches for the given key defined in btree.h
void *cursor_value(Cursor *cursor);
void cursor_advance(Cursor *cursor);
#endif // CURSOR_H