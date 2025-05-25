#include "../vendor/cJSON/cJSON.h"
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

// Format a row as JSON string 
char* format_row_as_json_string(DynamicRow* row, TableDef* table_def, 
                               char** columns_to_select, uint32_t num_columns_to_select) {
    // Use a string buffer 
    char* buffer = NULL;
    size_t buffer_size = 0;
    FILE* memstream = open_memstream(&buffer, &buffer_size);
    
    if (!memstream) {
        return NULL;
    }
    
    fprintf(memstream, "{");
    
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
                fprintf(memstream, ", ");
            }
            is_first = false;
            
            fprintf(memstream, "\"%s\": ", table_def->columns[col_idx].name);
            
            // Format column value
            ColumnDef* col = &table_def->columns[col_idx];
            
            switch (col->type) {
                case COLUMN_TYPE_INT:
                    fprintf(memstream, "%d", dynamic_row_get_int(row, table_def, col_idx));
                    break;
                
                case COLUMN_TYPE_FLOAT:
                    fprintf(memstream, "%.2f", dynamic_row_get_float(row, table_def, col_idx));
                    break;
                
                case COLUMN_TYPE_BOOLEAN:
                    fprintf(memstream, "%s", dynamic_row_get_boolean(row, table_def, col_idx) ? "true" : "false");
                    break;
                
                case COLUMN_TYPE_STRING: {
                    char* str = dynamic_row_get_string(row, table_def, col_idx);
                    if (str) {
                        char* escaped = json_escape_string(str);
                        fprintf(memstream, "\"%s\"", escaped ? escaped : str);
                        free(escaped);
                    } else {
                        fprintf(memstream, "null");
                    }
                    break;
                }
                
                default:
                    fprintf(memstream, "null");
                    break;
            }
        }
    } else {
        // Output all columns (similar to above but for all columns)
        for (uint32_t i = 0; i < table_def->num_columns; i++) {
            if (!is_first) {
                fprintf(memstream, ", ");
            }
            is_first = false;
            
            fprintf(memstream, "\"%s\": ", table_def->columns[i].name);
            format_column_value_as_json(row, table_def, i);
        }
    }
    
    fprintf(memstream, "}");
    fclose(memstream);
    
    return buffer;
}

// Create a complete JSON result for query results
char* create_json_result_string(DynamicRow** rows, int row_count, TableDef* table_def,
                              char** columns_to_select, uint32_t num_columns_to_select) {
    char* buffer = NULL;
    size_t buffer_size = 0;
    FILE* memstream = open_memstream(&buffer, &buffer_size);
    
    if (!memstream) {
        return NULL;
    }
    
    fprintf(memstream, "{\n  \"results\": [");
    
    for (int i = 0; i < row_count; i++) {
        if (i > 0) {
            fprintf(memstream, ",");
        }
        fprintf(memstream, "\n    ");
        
        char* row_json = format_row_as_json_string(rows[i], table_def, 
                                                 columns_to_select, num_columns_to_select);
        fprintf(memstream, "%s", row_json);
        free(row_json);
    }
    
    fprintf(memstream, "\n  ],\n  \"count\": %d\n}", row_count);
    fclose(memstream);
    
    return buffer;
}

// Create a JSON error response
char* json_create_error_response(const char *message) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "status", "error");
    cJSON_AddStringToObject(root, "message", message);
    
    char *response = cJSON_Print(root);
    cJSON_Delete(root);
    
    return response;
}

// Create a JSON success response
char* json_create_success_response(const char *data) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "status", "success");
    cJSON_AddStringToObject(root, "data", data);
    
    char *response = cJSON_Print(root);
    cJSON_Delete(root);
    
    return response;
}

// Create a JSON result set response (for query results)
char* json_create_result_response(DynamicRow** rows, int row_count, TableDef* table_def,
                                char** columns_to_select, uint32_t num_columns_to_select) {
    // First format the results using our existing function
    char* results_json = create_json_result_string(rows, row_count, table_def, 
                                                 columns_to_select, num_columns_to_select);
    
    // Now wrap it in our standard response format
    cJSON *root = cJSON_Parse(results_json);
    free(results_json);
    
    if (!root) {
        // This shouldn't happen if our JSON formatter is working correctly
        return json_create_error_response("Error formatting results");
    }
    
    // Add status field
    cJSON_AddStringToObject(root, "status", "success");
    
    char *response = cJSON_Print(root);
    cJSON_Delete(root);
    
    return response;
}

// Create a more detailed success response
char* json_create_detailed_success(const char* message, int affected_rows) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "status", "success");
    cJSON_AddStringToObject(root, "message", message);
    cJSON_AddNumberToObject(root, "affected_rows", affected_rows);
    
    char *response = cJSON_Print(root);
    cJSON_Delete(root);
    
    return response;
}

// Parse a JSON command
bool json_parse_command(const char *json_str, void *output) {
    cJSON *root = cJSON_Parse(json_str);
    if (!root) {
        return false;
    }
    
    // Extract command type
    cJSON *command_type = cJSON_GetObjectItem(root, "command");
    if (!command_type || !cJSON_IsString(command_type)) {
        cJSON_Delete(root);
        return false;
    }
    
    // Process the command based on its type
    // Note: This is a stub - the actual implementation would depend on your command structure
    
    cJSON_Delete(root);
    return true;
}

// Parse a SQL command from JSON
bool json_parse_sql_command(const char* json_str, char* sql_buffer, size_t buffer_size) {
    cJSON *root = cJSON_Parse(json_str);
    if (!root) {
        return false;
    }
    
    // Check if it's a query command
    cJSON *command = cJSON_GetObjectItem(root, "command");
    if (!command || !cJSON_IsString(command) || 
        strcmp(command->valuestring, "query") != 0) {
        cJSON_Delete(root);
        return false;
    }
    
    // Extract SQL text
    cJSON *sql = cJSON_GetObjectItem(root, "sql");
    if (!sql || !cJSON_IsString(sql)) {
        cJSON_Delete(root);
        return false;
    }
    
    // Copy the SQL
    strncpy(sql_buffer, sql->valuestring, buffer_size - 1);
    sql_buffer[buffer_size - 1] = '\0';
    
    cJSON_Delete(root);
    return true;
}

// Parse a meta command from JSON
bool json_parse_meta_command(const char* json_str, char* meta_buffer, size_t buffer_size) {
    cJSON *root = cJSON_Parse(json_str);
    if (!root) {
        return false;
    }
    
    // Check if it's a meta command
    cJSON *command = cJSON_GetObjectItem(root, "command");
    if (!command || !cJSON_IsString(command) || 
        strcmp(command->valuestring, "meta") != 0) {
        cJSON_Delete(root);
        return false;
    }
    
    // Extract meta command
    cJSON *meta = cJSON_GetObjectItem(root, "meta_command");
    if (!meta || !cJSON_IsString(meta)) {
        cJSON_Delete(root);
        return false;
    }
    
    // Copy the meta command (add "." prefix)
    snprintf(meta_buffer, buffer_size, ".%s", meta->valuestring);
    
    cJSON_Delete(root);
    return true;
}

// Parse a transaction command from JSON
bool json_parse_transaction_command(const char* json_str, char* txn_buffer, size_t buffer_size) {
    cJSON *root = cJSON_Parse(json_str);
    if (!root) {
        return false;
    }
    
    // Check if it's a transaction command
    cJSON *command = cJSON_GetObjectItem(root, "command");
    if (!command || !cJSON_IsString(command) || 
        strcmp(command->valuestring, "transaction") != 0) {
        cJSON_Delete(root);
        return false;
    }
    
    // Extract transaction command
    cJSON *txn_cmd = cJSON_GetObjectItem(root, "transaction_command");
    if (!txn_cmd || !cJSON_IsString(txn_cmd)) {
        cJSON_Delete(root);
        return false;
    }
    
    // Valid transaction commands
    if (strcmp(txn_cmd->valuestring, "begin") != 0 && 
        strcmp(txn_cmd->valuestring, "commit") != 0 && 
        strcmp(txn_cmd->valuestring, "rollback") != 0) {
        cJSON_Delete(root);
        return false;
    }
    
    // Copy the transaction command
    strncpy(txn_buffer, txn_cmd->valuestring, buffer_size - 1);
    txn_buffer[buffer_size - 1] = '\0';
    
    cJSON_Delete(root);
    return true;
}

/**
 * Parse a JSON command directly into a Statement structure
 * @param json_str The JSON string to parse
 * @param statement Pointer to the Statement structure to populate
 * @return true if parsing succeeded, false otherwise
 */
bool json_parse_statement(const char* json_str, Statement* statement) {
    if (!json_str || !statement) {
        return false;
    }
    
    // Initialize statement to avoid undefined behavior
    memset(statement, 0, sizeof(Statement));
    
    cJSON *root = cJSON_Parse(json_str);
    if (!root) {
        return false;
    }
    
    // Extract command type
    cJSON *command = cJSON_GetObjectItem(root, "command");
    if (!command || !cJSON_IsString(command)) {
        cJSON_Delete(root);
        return false;
    }
    
    bool success = false;
    
    // Determine statement type based on command
    if (strcmp(command->valuestring, "select") == 0) {
        statement->type = STATEMENT_SELECT;
        success = json_parse_select_statement(root, statement);
    } 
    else if (strcmp(command->valuestring, "insert") == 0) {
        statement->type = STATEMENT_INSERT;
        success = json_parse_insert_statement(root, statement);
    }
    else if (strcmp(command->valuestring, "update") == 0) {
        statement->type = STATEMENT_UPDATE;
        success = json_parse_update_statement(root, statement);
    }
    else if (strcmp(command->valuestring, "delete") == 0) {
        statement->type = STATEMENT_DELETE;
        success = json_parse_delete_statement(root, statement);
    }
    else if (strcmp(command->valuestring, "create_table") == 0) {
        statement->type = STATEMENT_CREATE_TABLE;
        success = json_parse_create_table_statement(root, statement);
    }
    else if (strcmp(command->valuestring, "create_index") == 0) {
        statement->type = STATEMENT_CREATE_INDEX;
        success = json_parse_create_index_statement(root, statement);
    }
    else if (strcmp(command->valuestring, "use_database") == 0) {
        statement->type = STATEMENT_USE_DATABASE;
        success = json_parse_use_database_statement(root, statement);
    }
    else if (strcmp(command->valuestring, "use_table") == 0) {
        statement->type = STATEMENT_USE_TABLE;
        success = json_parse_use_table_statement(root, statement);
    }
    else if (strcmp(command->valuestring, "show_tables") == 0) {
        statement->type = STATEMENT_SHOW_TABLES;
        success = json_parse_show_tables_statement(root, statement);
    }
    else if (strcmp(command->valuestring, "show_indexes") == 0) {
        statement->type = STATEMENT_SHOW_INDEXES;
        success = json_parse_show_indexes_statement(root, statement);
    }
    else {
        // Unknown command type
        cJSON_Delete(root);
        return false;
    }
    
    cJSON_Delete(root);
    return success;
}

/**
 * Parse a SELECT statement from JSON
 */
bool json_parse_select_statement(cJSON* root, Statement* statement) {
    // Extract table name
    cJSON* table = cJSON_GetObjectItem(root, "table");
    if (!table || !cJSON_IsString(table)) {
        return false;
    }
    strncpy(statement->table_name, table->valuestring, MAX_TABLE_NAME - 1);
    statement->table_name[MAX_TABLE_NAME - 1] = '\0';
    
    // Extract column names
    cJSON* columns = cJSON_GetObjectItem(root, "columns");
    if (columns && cJSON_IsArray(columns)) {
        int num_columns = cJSON_GetArraySize(columns);
        statement->columns_to_select = malloc(sizeof(char*) * num_columns);
        if (!statement->columns_to_select) {
            return false;
        }
        
        statement->num_columns_to_select = num_columns;
        
        for (int i = 0; i < num_columns; i++) {
            cJSON* column = cJSON_GetArrayItem(columns, i);
            if (cJSON_IsString(column)) {
                statement->columns_to_select[i] = strdup(column->valuestring);
                if (!statement->columns_to_select[i]) {
                    // Clean up on failure
                    for (int j = 0; j < i; j++) {
                        free(statement->columns_to_select[j]);
                    }
                    free(statement->columns_to_select);
                    statement->columns_to_select = NULL;
                    statement->num_columns_to_select = 0;
                    return false;
                }
            } else {
                // Clean up on failure
                for (int j = 0; j < i; j++) {
                    free(statement->columns_to_select[j]);
                }
                free(statement->columns_to_select);
                statement->columns_to_select = NULL;
                statement->num_columns_to_select = 0;
                return false;
            }
        }
    } else {
        // If no columns specified, select all columns
        statement->columns_to_select = NULL;
        statement->num_columns_to_select = 0;
    }
    
    // Extract WHERE condition if present
    return json_parse_where_clause(cJSON_GetObjectItem(root, "where"), statement);
}

/**
 * Parse an INSERT statement from JSON
 */
bool json_parse_insert_statement(cJSON* root, Statement* statement) {
    // Extract table name
    cJSON* table = cJSON_GetObjectItem(root, "table");
    if (!table || !cJSON_IsString(table)) {
        return false;
    }
    strncpy(statement->table_name, table->valuestring, MAX_TABLE_NAME - 1);
    statement->table_name[MAX_TABLE_NAME - 1] = '\0';
    
    // Extract values array
    cJSON* values = cJSON_GetObjectItem(root, "values");
    if (!values || !cJSON_IsArray(values)) {
        return false;
    }
    
    int num_values = cJSON_GetArraySize(values);
    if (num_values == 0) {
        return false;
    }
    
    statement->values = malloc(sizeof(char*) * num_values);
    if (!statement->values) {
        return false;
    }
    
    statement->num_values = num_values;
    
    for (int i = 0; i < num_values; i++) {
        cJSON* value = cJSON_GetArrayItem(values, i);
        char buffer[MAX_COLUMN_SIZE];
        
        if (cJSON_IsString(value)) {
            // String values need to be quoted in the statement
            snprintf(buffer, sizeof(buffer), "\"%s\"", value->valuestring);
        } 
        else if (cJSON_IsNumber(value)) {
            snprintf(buffer, sizeof(buffer), "%g", value->valuedouble);
        } 
        else if (cJSON_IsBool(value)) {
            strcpy(buffer, cJSON_IsTrue(value) ? "true" : "false");
        } 
        else if (cJSON_IsNull(value)) {
            strcpy(buffer, "NULL");
        } 
        else {
            // Unsupported value type
            for (int j = 0; j < i; j++) {
                free(statement->values[j]);
            }
            free(statement->values);
            statement->values = NULL;
            statement->num_values = 0;
            return false;
        }
        
        statement->values[i] = strdup(buffer);
        if (!statement->values[i]) {
            // Clean up on failure
            for (int j = 0; j < i; j++) {
                free(statement->values[j]);
            }
            free(statement->values);
            statement->values = NULL;
            statement->num_values = 0;
            return false;
        }
    }
    
    return true;
}

/**
 * Parse a WHERE clause from JSON into a Statement
 */
bool json_parse_where_clause(cJSON* where, Statement* statement) {
    if (!where) {
        statement->has_where_clause = false;
        return true;
    }
    
    cJSON* column = cJSON_GetObjectItem(where, "column");
    cJSON* operator = cJSON_GetObjectItem(where, "operator");
    cJSON* value = cJSON_GetObjectItem(where, "value");
    
    if (!column || !cJSON_IsString(column) || 
        !operator || !cJSON_IsString(operator) || 
        !value) {
        return false;
    }
    
    strncpy(statement->where_column, column->valuestring, MAX_COLUMN_NAME - 1);
    statement->where_column[MAX_COLUMN_NAME - 1] = '\0';
    
    // Set the operator
    if (strcmp(operator->valuestring, "=") == 0) {
        statement->where_operator = WHERE_OP_EQUAL;
    } else if (strcmp(operator->valuestring, ">") == 0) {
        statement->where_operator = WHERE_OP_GREATER;
    } else if (strcmp(operator->valuestring, "<") == 0) {
        statement->where_operator = WHERE_OP_LESS;
    } else if (strcmp(operator->valuestring, ">=") == 0) {
        statement->where_operator = WHERE_OP_GREATER_EQUAL;
    } else if (strcmp(operator->valuestring, "<=") == 0) {
        statement->where_operator = WHERE_OP_LESS_EQUAL;
    } else if (strcmp(operator->valuestring, "!=") == 0) {
        statement->where_operator = WHERE_OP_NOT_EQUAL;
    } else {
        return false;
    }
    
    // Extract value based on type
    if (cJSON_IsString(value)) {
        snprintf(statement->where_value, MAX_COLUMN_SIZE, "%s", value->valuestring);
    } else if (cJSON_IsNumber(value)) {
        snprintf(statement->where_value, MAX_COLUMN_SIZE, "%g", value->valuedouble);
    } else if (cJSON_IsBool(value)) {
        strcpy(statement->where_value, cJSON_IsTrue(value) ? "true" : "false");
    } else {
        return false;
    }
    
    statement->has_where_clause = true;
    return true;
}

/**
 * Parse an UPDATE statement from JSON
 */
bool json_parse_update_statement(cJSON* root, Statement* statement) {
    // Extract table name
    cJSON* table = cJSON_GetObjectItem(root, "table");
    if (!table || !cJSON_IsString(table)) {
        return false;
    }
    strncpy(statement->table_name, table->valuestring, MAX_TABLE_NAME - 1);
    
    // Extract column to update
    cJSON* column = cJSON_GetObjectItem(root, "column");
    if (!column || !cJSON_IsString(column)) {
        return false;
    }
    strncpy(statement->column_to_update, column->valuestring, MAX_COLUMN_NAME - 1);
    
    // Extract update value
    cJSON* value = cJSON_GetObjectItem(root, "value");
    if (!value) {
        return false;
    }
    
    if (cJSON_IsString(value)) {
        snprintf(statement->update_value, COLUMN_EMAIL_SIZE, "%s", value->valuestring);
    } else if (cJSON_IsNumber(value)) {
        snprintf(statement->update_value, COLUMN_EMAIL_SIZE, "%g", value->valuedouble);
    } else if (cJSON_IsBool(value)) {
        strcpy(statement->update_value, cJSON_IsTrue(value) ? "true" : "false");
    } else {
        return false;
    }
    
    // Extract WHERE clause
    return json_parse_where_clause(cJSON_GetObjectItem(root, "where"), statement);
}

/**
 * Parse a DELETE statement from JSON
 */
bool json_parse_delete_statement(cJSON* root, Statement* statement) {
    // Extract table name
    cJSON* table = cJSON_GetObjectItem(root, "table");
    if (!table || !cJSON_IsString(table)) {
        return false;
    }
    strncpy(statement->table_name, table->valuestring, MAX_TABLE_NAME - 1);
    
    // Extract WHERE clause
    return json_parse_where_clause(cJSON_GetObjectItem(root, "where"), statement);
}

/**
 * Parse a CREATE TABLE statement from JSON
 */
bool json_parse_create_table_statement(cJSON* root, Statement* statement) {
    // Extract table name
    cJSON* table = cJSON_GetObjectItem(root, "table");
    if (!table || !cJSON_IsString(table)) {
        return false;
    }
    strncpy(statement->table_name, table->valuestring, MAX_TABLE_NAME - 1);
    
    // Extract columns definition
    cJSON* columns = cJSON_GetObjectItem(root, "columns");
    if (!columns || !cJSON_IsArray(columns)) {
        return false;
    }
    
    int num_columns = cJSON_GetArraySize(columns);
    if (num_columns == 0 || num_columns > MAX_COLUMNS) {
        return false;
    }
    
    statement->num_columns = num_columns;
    
    for (int i = 0; i < num_columns; i++) {
        cJSON* column = cJSON_GetArrayItem(columns, i);
        
        cJSON* name = cJSON_GetObjectItem(column, "name");
        cJSON* type = cJSON_GetObjectItem(column, "type");
        cJSON* size = cJSON_GetObjectItem(column, "size");
        
        if (!name || !cJSON_IsString(name) || !type || !cJSON_IsString(type)) {
            return false;
        }
        
        strncpy(statement->columns[i].name, name->valuestring, MAX_COLUMN_NAME - 1);
        
        if (strcmp(type->valuestring, "INT") == 0) {
            statement->columns[i].type = COLUMN_TYPE_INT;
        } else if (strcmp(type->valuestring, "STRING") == 0) {
            statement->columns[i].type = COLUMN_TYPE_STRING;
            if (size && cJSON_IsNumber(size)) {
                statement->columns[i].size = (uint32_t)size->valueint;
            } else {
                statement->columns[i].size = 255; // Default string size
            }
        } else if (strcmp(type->valuestring, "FLOAT") == 0) {
            statement->columns[i].type = COLUMN_TYPE_FLOAT;
        } else if (strcmp(type->valuestring, "BOOLEAN") == 0) {
            statement->columns[i].type = COLUMN_TYPE_BOOLEAN;
        } else if (strcmp(type->valuestring, "DATE") == 0) {
            statement->columns[i].type = COLUMN_TYPE_DATE;
        } else if (strcmp(type->valuestring, "TIME") == 0) {
            statement->columns[i].type = COLUMN_TYPE_TIME;
        } else if (strcmp(type->valuestring, "TIMESTAMP") == 0) {
            statement->columns[i].type = COLUMN_TYPE_TIMESTAMP;
        } else if (strcmp(type->valuestring, "BLOB") == 0) {
            statement->columns[i].type = COLUMN_TYPE_BLOB;
        } else {
            return false;
        }
    }
    
    return true;
}


bool json_parse_create_index_statement(cJSON* root, Statement* statement){
    // Extract index name
    cJSON* index_name = cJSON_GetObjectItem(root, "index_name");
    if (!index_name || !cJSON_IsString(index_name)) {
        return false;
    }
    strncpy(statement->index_name, index_name->valuestring, MAX_INDEX_NAME - 1);
    
    // Extract table name
    cJSON* table = cJSON_GetObjectItem(root, "table");
    if (!table || !cJSON_IsString(table)) {
        return false;
    }
    strncpy(statement->table_name, table->valuestring, MAX_TABLE_NAME - 1);
    
    // Extract columns to index
    cJSON* columns = cJSON_GetObjectItem(root, "columns");
    if (!columns || !cJSON_IsArray(columns)) {
        return false;
    }
    
    int num_columns = cJSON_GetArraySize(columns);
    if (num_columns == 0 || num_columns > 1) {
        return false;
    }
    
    statement->num_columns = num_columns;
    
    for (int i = 0; i < num_columns; i++) {
        cJSON* column = cJSON_GetArrayItem(columns, i);
        if (!column || !cJSON_IsString(column)) {
            return false;
        }
        
        strncpy(statement->columns[i].name, column->valuestring, MAX_COLUMN_NAME - 1);
        statement->columns[i].name[MAX_COLUMN_NAME - 1] = '\0';
        
        // Default type for index columns is STRING
        statement->columns[i].type = COLUMN_TYPE_STRING;
        statement->columns[i].size = 255; // Default size for string columns
    }
    
    return true;
}


bool json_parse_use_database_statement(cJSON* root, Statement* statement){
    // TODO: Implement this
}
bool json_parse_show_tables_statement(cJSON* root, Statement* statement){
    // TODO: Implement this
}
bool json_parse_show_indexes_statement(cJSON* root, Statement* statement){
    //  TODO: Implement this
}


// Parse and execute a meta command from JSON
bool json_parse_meta_command(const char* json_str, ClientConnection* conn, DatabaseServer* server) {
    cJSON *root = cJSON_Parse(json_str);
    if (!root) {
        return false;
    }
    
    // Check if it's a meta command
    cJSON *command = cJSON_GetObjectItem(root, "command");
    if (!command || !cJSON_IsString(command) || 
        strcmp(command->valuestring, "meta") != 0) {
        cJSON_Delete(root);
        return false;
    }
    
    // Extract meta command
    cJSON *meta = cJSON_GetObjectItem(root, "meta_command");
    if (!meta || !cJSON_IsString(meta)) {
        cJSON_Delete(root);
        return false;
    }
    
    const char* meta_cmd = meta->valuestring;
    char* response = NULL;
    Database* db = server->db;
    
    // Handle exit command
    if (strcmp(meta_cmd, "exit") == 0) {
        response = json_create_success_response("Closing connection");
        connection_send_response(conn, response);
        free(response);
        
        // Mark connection for closing
        pthread_mutex_lock(&conn->lock);
        conn->connected = false;
        pthread_mutex_unlock(&conn->lock);
        
        cJSON_Delete(root);
        return true;
    }
    
    // Handle btree command
    else if (strncmp(meta_cmd, "btree", 5) == 0) {
        // Check if a specific table is requested
        char table_name[MAX_TABLE_NAME] = {0};
        cJSON* table = cJSON_GetObjectItem(root, "table");
        
        if (table && cJSON_IsString(table)) {
            strncpy(table_name, table->valuestring, MAX_TABLE_NAME-1);
        }
        
        // TODO: Capture the btree output to send back as a response
        // For now, just acknowledge receipt
        if (table_name[0] != '\0') {
            response = json_create_success_response("Btree command received for table");
        } else {
            response = json_create_success_response("Btree command received for active table");
        }
    }
    
    // Handle constants command
    else if (strcmp(meta_cmd, "constants") == 0) {
        response = json_create_success_response("Constants command received");
        // TODO: Capture constants output to send back
    }
    
    // Handle transaction commands
    else if (strncmp(meta_cmd, "txn", 3) == 0) {
        const char* txn_cmd = meta_cmd + 4; // Skip "txn "
        
        if (strcmp(txn_cmd, "begin") == 0) {
            // Begin a new transaction
            if (conn->transaction_id != 0) {
                // Already in a transaction
                response = json_create_error_response("Transaction already in progress");
            } else {
                conn->transaction_id = txn_begin(server->txn_manager);
                char msg[64];
                snprintf(msg, sizeof(msg), "Transaction %u started", conn->transaction_id);
                response = json_create_success_response(msg);
            }
        }
        else if (strcmp(txn_cmd, "commit") == 0) {
            // Commit the current transaction
            if (conn->transaction_id == 0) {
                response = json_create_error_response("No active transaction");
            } else {
                txn_commit(server->txn_manager, conn->transaction_id);
                char msg[64];
                snprintf(msg, sizeof(msg), "Transaction %u committed", conn->transaction_id);
                conn->transaction_id = 0;
                response = json_create_success_response(msg);
            }
        }
        else if (strcmp(txn_cmd, "rollback") == 0) {
            // Rollback the current transaction
            if (conn->transaction_id == 0) {
                response = json_create_error_response("No active transaction");
            } else {
                txn_rollback(server->txn_manager, conn->transaction_id);
                char msg[64];
                snprintf(msg, sizeof(msg), "Transaction %u rolled back", conn->transaction_id);
                conn->transaction_id = 0;
                response = json_create_success_response(msg);
            }
        }
        else if (strcmp(txn_cmd, "status") == 0) {
            // Show transaction status
            if (conn->transaction_id == 0) {
                response = json_create_success_response("No active transaction");
            } else {
                char msg[64];
                snprintf(msg, sizeof(msg), "Active transaction: %u", conn->transaction_id);
                response = json_create_success_response(msg);
                // TODO: Capture more detailed status info
            }
        }
        else if (strcmp(txn_cmd, "enable") == 0) {
            // Enable transactions
            if (db) {
                db_enable_transactions(db);
                response = json_create_success_response("Transactions enabled");
            } else {
                response = json_create_error_response("No database selected");
            }
        }
        else if (strcmp(txn_cmd, "disable") == 0) {
            // Disable transactions
            if (db) {
                db_disable_transactions(db);
                response = json_create_success_response("Transactions disabled");
            } else {
                response = json_create_error_response("No database selected");
            }
        }
        else {
            response = json_create_error_response("Unknown transaction command");
        }
    }
    
    // Handle format command
    else if (strncmp(meta_cmd, "format", 6) == 0) {
        cJSON* format = cJSON_GetObjectItem(root, "format_type");
        if (format && cJSON_IsString(format)) {
            const char* format_type = format->valuestring;
            
            if (db) {
                if (strcasecmp(format_type, "table") == 0) {
                    db->output_format = OUTPUT_FORMAT_TABLE;
                    response = json_create_success_response("Output format set to TABLE");
                }
                else if (strcasecmp(format_type, "json") == 0) {
                    db->output_format = OUTPUT_FORMAT_JSON;
                    response = json_create_success_response("Output format set to JSON");
                }
                else {
                    response = json_create_error_response("Unknown format type");
                }
            } else {
                response = json_create_error_response("No database selected");
            }
        } else {
            response = json_create_error_response("Format type not specified");
        }
    }
    
    // Unknown meta command
    else {
        char error_msg[128];
        snprintf(error_msg, sizeof(error_msg), "Unknown meta command: %s", meta_cmd);
        response = json_create_error_response(error_msg);
    }
    
    // Send the response
    if (response) {
        connection_send_response(conn, response);
        free(response);
    }
    
    cJSON_Delete(root);
    return true;
}

