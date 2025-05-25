#ifndef JSON_FORMATTER_H
#define JSON_FORMATTER_H

#include "table.h"
#include "schema.h"
#include <stdint.h>

char* json_escape_string(const char* str);
void format_row_as_json(DynamicRow* row, TableDef* table_def, 
                       char** columns_to_select, uint32_t num_columns_to_select);
void format_column_value_as_json(DynamicRow* row, TableDef* table_def, uint32_t col_idx);
void start_json_result();
void end_json_result(int count);

#endif