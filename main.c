#include "include/command_processor.h"
#include "include/input_handling.h"
#include "include/table.h"
#include "include/btree.h"
#include "include/cursor.h"
#include "include/pager.h"
#include "include/database.h"
#include "include/catalog.h"
#include <string.h>
#include <strings.h>

int main(int argc, char *argv[]) {
    // Start with no active database
    Database* db = NULL;
    
    Input_Buffer *input_buf = newInputBuffer();
    while (1) {
        read_input(input_buf);
        if (input_buf->buffer[0] == '.') {
            // If no database is open, only allow certain meta commands
            if (!db && strcmp(input_buf->buffer, ".exit") != 0) {
                printf("Error: No database is currently open.\n");
                printf("Create or open a database first with 'CREATE DATABASE name' or 'USE DATABASE name'\n");
                continue;
            }
            
            switch (do_meta_command(input_buf, db)) {
                case META_COMMAND_SUCCESS:
                    continue;
                case META_COMMAND_UNRECOGNIZED_COMMAND:
                    printf("Unrecognized command %s\n", input_buf->buffer);
                    continue;
            }
        } else {
            Statement statement;
            
            // Check for database creation or use commands before requiring an active database
            if (strncasecmp(input_buf->buffer, "create database", 15) == 0 ||
                strncasecmp(input_buf->buffer, "use database", 12) == 0) {
                
                switch (prepare_database_statement(input_buf, &statement)) {
                    case PREPARE_SUCCESS:
                        break;
                    case PREPARE_SYNTAX_ERROR:
                        printf("Syntax error. Could not parse statement.\n");
                        continue;
                    default:
                        printf("Unknown error during database operation.\n");
                        continue;
                }
                
                switch (execute_database_statement(&statement, &db)) {
                    case EXECUTE_SUCCESS:
                        printf("Executed.\n");
                        continue;
                    case EXECUTE_UNRECOGNIZED_STATEMENT:
                        printf("Error during database operation.\n");
                        continue;
                    default:
                        printf("Unknown error during database operation.\n");
                        continue;
                }
            }
            
            // For all other commands, require an active database
            if (!db) {
                printf("Error: No database is currently open.\n");
                printf("Create or open a database first with 'CREATE DATABASE name' or 'USE DATABASE name'\n");
                continue;
            }
            
            switch (prepare_statement(input_buf, &statement)) {
                case PREPARE_SUCCESS:
                    break;
                case PREPARE_NEGATIVE_ID:
                    printf("ID must be positive.\n");
                    continue;
                case PREPARE_STRING_TOO_LONG:
                    printf("String is too long.\n");
                    continue;
                case PREPARE_SYNTAX_ERROR:
                    printf("Syntax error. Could not parse statement.\n");
                    continue;
                case PREPARE_UNRECOGNIZED_STATEMENT:
                    printf("Unrecognized keyword at the start of '%s'.\n",
                        input_buf->buffer);
                    continue;
            }
            
            switch (execute_statement(&statement, db)) {
                case EXECUTE_SUCCESS:
                    printf("Executed.\n");
                    break;
                case EXECUTE_DUPLICATE_KEY:
                    printf("Error: Duplicate key.\n");
                    break;
                case EXECUTE_TABLE_FULL:
                    printf("Error: Table full.\n");
                    break;
                case EXECUTE_UNRECOGNIZED_STATEMENT:
                    printf("Unrecognized statement at '%s'.\n", input_buf->buffer);
                    break;
            }
        }
    }
}