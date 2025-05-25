#ifndef JSON_FORMATTER_H
#define JSON_FORMATTER_H

#include "table.h"
#include "schema.h"
#include <stdint.h>
#include "../vendor/cJSON/cJSON.h"
#include "command_processor.h"

typedef ClientConnection ClientConnection;
typedef DatabaseServer DatabaseServer;

char* json_escape_string(const char* str);
void format_row_as_json(DynamicRow* row, TableDef* table_def, 
                       char** columns_to_select, uint32_t num_columns_to_select);
void format_column_value_as_json(DynamicRow* row, TableDef* table_def, uint32_t col_idx);
void start_json_result();
void end_json_result(int count);
char* format_row_as_json_string(DynamicRow* row, TableDef* table_def, 
                               char** columns_to_select, uint32_t num_columns_to_select);
char* create_json_result_string(DynamicRow** rows, int row_count, TableDef* table_def,
                              char** columns_to_select, uint32_t num_columns_to_select);

// New functions for network JSON handling
char* json_create_error_response(const char* message);
char* json_create_success_response(const char* data);
char* json_create_result_response(DynamicRow** rows, int row_count, TableDef* table_def,
                                char** columns_to_select, uint32_t num_columns_to_select);
char* json_create_detailed_success(const char* message, int affected_rows);
bool json_parse_command(const char* json_str, void* output);

// Parse JSON into database statements
bool json_parse_sql_command(const char* json_str, char* sql_buffer, size_t buffer_size);
bool json_parse_transaction_command(const char* json_str, char* txn_buffer, size_t buffer_size);

// Parse JSON directly into Statement structure
bool json_parse_statement(const char* json_str, Statement* statement);

// Statement-specific JSON parsers
bool json_parse_select_statement(cJSON* root, Statement* statement);
bool json_parse_insert_statement(cJSON* root, Statement* statement);
bool json_parse_update_statement(cJSON* root, Statement* statement);
bool json_parse_delete_statement(cJSON* root, Statement* statement);
bool json_parse_create_table_statement(cJSON* root, Statement* statement);
bool json_parse_create_index_statement(cJSON* root, Statement* statement);
bool json_parse_use_database_statement(cJSON* root, Statement* statement);
bool json_parse_show_tables_statement(cJSON* root, Statement* statement);
bool json_parse_show_indexes_statement(cJSON* root, Statement* statement);
bool json_parse_meta_command(const char* json_str, ClientConnection* conn, DatabaseServer* server);

// Helper functions for parsing
bool json_parse_where_clause(cJSON* where, Statement* statement);
bool json_parse_columns(cJSON* columns, Statement* statement);

#endif // JSON_FORMATTER_H