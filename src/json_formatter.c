#include "../include/json_formatter.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#ifdef _WIN32
// Windows specific
#include <string.h>
#define strcasecmp _stricmp
#else
// Unix/Linux specific
#include <strings.h>  // For strcasecmp
#endif

// Helper function to escape JSON strings
char* json_escape_string(const char* str) {
    if (!str) return NULL;
    
    size_t str_len = strlen(str);
    // Allocate enough space for a fully escaped string (worst case: each char needs escaping)
    char* escaped = malloc(str_len * 2 + 1);
    
    if (!escaped) return NULL;
    
    size_t j = 0;
    for (size_t i = 0; i < str_len; i++) {
        switch (str[i]) {
            case '\\': escaped[j++] = '\\'; escaped[j++] = '\\'; break;
            case '"': escaped[j++] = '\\'; escaped[j++] = '"'; break;
            case '\b': escaped[j++] = '\\'; escaped[j++] = 'b'; break;
            case '\f': escaped[j++] = '\\'; escaped[j++] = 'f'; break;
            case '\n': escaped[j++] = '\\'; escaped[j++] = 'n'; break;
            case '\r': escaped[j++] = '\\'; escaped[j++] = 'r'; break;
            case '\t': escaped[j++] = '\\'; escaped[j++] = 't'; break;
            default:
                // Handle control characters
                if (iscntrl(str[i])) {
                    j += sprintf(&escaped[j], "\\u%04x", (unsigned char)str[i]);
                } else {
                    escaped[j++] = str[i];
                }
                break;
        }
    }
    
    escaped[j] = '\0';
    return escaped;
}

// Format a single row as JSON object
void format_row_as_json(DynamicRow* row, TableDef* table_def, 
                        char** columns_to_select, uint32_t num_columns_to_select) {
    printf("{");
    
    bool is_first = true;
    
    // If specific columns are selected, only output those
    if (num_columns_to_select > 0) {
        for (uint32_t i = 0; i < num_columns_to_select; i++) {
            // Find the column index by name
            int col_idx = -1;
            for (uint32_t j = 0; j < table_def->num_columns; j++) {
                if (strcasecmp(table_def->columns[j].name, columns_to_select[i]) == 0) {
                    col_idx = j;
                    break;
                }
            }
            
            if (col_idx == -1) continue; // Skip if column not found
            
            if (!is_first) {
                printf(", ");
            }
            is_first = false;
            
            printf("\"%s\": ", table_def->columns[col_idx].name);
            format_column_value_as_json(row, table_def, col_idx);
        }
    } else {
        // Output all columns
        for (uint32_t i = 0; i < table_def->num_columns; i++) {
            if (!is_first) {
                printf(", ");
            }
            is_first = false;
            
            printf("\"%s\": ", table_def->columns[i].name);
            format_column_value_as_json(row, table_def, i);
        }
    }
    
    printf("}");
}

// Format a single column value according to its type
void format_column_value_as_json(DynamicRow* row, TableDef* table_def, uint32_t col_idx) {
    ColumnDef* col = &table_def->columns[col_idx];
    
    switch (col->type) {
        case COLUMN_TYPE_INT:
            printf("%d", dynamic_row_get_int(row, table_def, col_idx));
            break;
            
        case COLUMN_TYPE_FLOAT:
            printf("%.2f", dynamic_row_get_float(row, table_def, col_idx));
            break;
            
        case COLUMN_TYPE_BOOLEAN:
            printf("%s", dynamic_row_get_boolean(row, table_def, col_idx) ? "true" : "false");
            break;
            
        case COLUMN_TYPE_DATE:
            printf("\"%d\"", dynamic_row_get_date(row, table_def, col_idx));
            break;
            
        case COLUMN_TYPE_TIME:
            printf("\"%d\"", dynamic_row_get_time(row, table_def, col_idx));
            break;
            
        case COLUMN_TYPE_TIMESTAMP:
            printf("\"%lld\"", (long long)dynamic_row_get_timestamp(row, table_def, col_idx));
            break;
            
        case COLUMN_TYPE_STRING: {
            char* str = dynamic_row_get_string(row, table_def, col_idx);
            if (str) {
                char* escaped = json_escape_string(str);
                printf("\"%s\"", escaped ? escaped : str);
                free(escaped);
            } else {
                printf("null");
            }
            break;
        }
        
        case COLUMN_TYPE_BLOB: {
            uint32_t size;
            void* blob = dynamic_row_get_blob(row, table_def, col_idx, &size);
            (void)blob; // Avoid unused variable warning
            printf("\"<BLOB(%u bytes)>\"", size);
            break;
        }
        
        default:
            printf("null");
            break;
    }
}

// Start a JSON array response
void start_json_result() {
    printf("{\n  \"results\": [\n");
}

// End a JSON array response
void end_json_result(int count) {
    printf("\n  ],\n  \"count\": %d\n}", count);
}