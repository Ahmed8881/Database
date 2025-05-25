#include "include/command_processor.h"
#include "include/input_handling.h"
#include "include/table.h"
#include "include/btree.h"
#include "include/cursor.h"
#include "include/pager.h"
#include "include/database.h"
#include "include/catalog.h"
#include "include/auth.h"
#include <string.h>
#include <strings.h> // Add this for strncasecmp

int main(int argc, char *argv[]) {
    (void)argc; // Mark as used to avoid warning
    (void)argv; // Mark as used to avoid warning
    
    // Start with no active database
    Database* db = NULL;
    
    // Flag to indicate if we've shown the welcome message
    bool welcome_shown = false;
    
    Input_Buffer *input_buf = newInputBuffer();
    while (1) {
        // Show welcome message only once
        if (!welcome_shown) {
            printf("====================================================\n");
            printf("    Welcome to JHAZ Database System\n");
            printf("----------------------------------------------------\n");
            printf("Please login with: LOGIN username password\n");
            // printf("Default admin credentials: admin / jhaz\n");
            printf("====================================================\n\n");
            welcome_shown = true;
        }
        
        // Display prompt with authentication status
        if (db) {
            if (db_is_authenticated(db)) {
                printf("%s:%s> ", db->name, auth_get_current_username(&db->user_manager));
            } else {
                printf("%s> ", db->name);
            }
        } else {
            printf("db > ");
        }
        
        read_input(input_buf);
        
        // Trim any trailing newlines or whitespace
        char *trimmed_input = input_buf->buffer;
        size_t len = strlen(trimmed_input);
        while (len > 0 && (trimmed_input[len-1] == '\n' || trimmed_input[len-1] == '\r' || 
                         trimmed_input[len-1] == ' ' || trimmed_input[len-1] == '\t')) {
            trimmed_input[--len] = '\0';
        }
        
        // Skip empty lines
        if (len == 0) {
            continue;
        }
        
        // Process the single command
        if (trimmed_input[0] == '.') {
            // If no database is open, only allow certain meta commands
            if (!db && strcmp(trimmed_input, ".exit") != 0) {
                printf("Error: No database is currently open.\n");
                printf("Create or open a database first with 'CREATE DATABASE name' or 'USE DATABASE name'\n");
                continue;
            }
            
            // Check authentication for meta commands except exit
            if (strcmp(trimmed_input, ".exit") != 0 && !db_is_authenticated(db)) {
                printf("Error: Authentication required. Please login first.\n");
                printf("Use 'LOGIN username 'password'' to authenticate.\n");
                continue;
            }
            
            switch (do_meta_command(input_buf, db)) {
                case META_COMMAND_SUCCESS:
                    continue;
                case META_COMMAND_TXN_BEGIN:
                    if (!db) {
                        printf("Error: No database is currently open.\n");
                    } else {
                        db_begin_transaction(db);
                    }
                    continue;
                case META_COMMAND_TXN_COMMIT:
                    if (!db) {
                        printf("Error: No database is currently open.\n");
                    } else {
                        db_commit_transaction(db);
                    }
                    continue;
                case META_COMMAND_TXN_ROLLBACK:
                    if (!db) {
                        printf("Error: No database is currently open.\n");
                    } else {
                        db_rollback_transaction(db);
                    }
                    continue;
                case META_COMMAND_TXN_STATUS:
                    if (!db) {
                        printf("Error: No database is currently open.\n");
                    } else if (db->active_txn_id == 0) {
                        printf("No active transaction.\n");
                    } else {
                        printf("Current transaction: %u\n", db->active_txn_id);
                        txn_print_status(&db->txn_manager, db->active_txn_id);
                    }
                    continue;
                case META_COMMAND_UNRECOGNIZED_COMMAND:
                    printf("Unrecognized command %s\n", trimmed_input);
                    continue;
            }
        } else {
            Statement statement;
            memset(&statement, 0, sizeof(Statement));  // Initialize all fields
            
            // Always allow authentication commands
            if (strncasecmp(trimmed_input, "login", 5) == 0 ||
                strncasecmp(trimmed_input, "logout", 6) == 0) {
                
                // Check if we need to create a temporary database for auth
                if (!db) {
                    // Create an in-memory database just for auth
                    db = malloc(sizeof(Database));
                    if (!db) {
                        printf("Error: Failed to allocate memory.\n");
                        continue;
                    }
                    memset(db, 0, sizeof(Database));
                    strcpy(db->name, "temp");
                    auth_init(&db->user_manager);
                }
                
                switch (prepare_statement(input_buf, &statement)) {
                    case PREPARE_SUCCESS:
                        statement.db = db;
                        break;
                    default:
                        printf("Syntax error. Could not parse statement.\n");
                        continue;
                }
                
                switch (execute_statement(&statement, db)) {
                    case EXECUTE_SUCCESS:
                        // Success handled by the execute function
                        break;
                    case EXECUTE_AUTH_FAILED:
                        // Auth failure handled by the execute function
                        break;
                    default:
                        printf("Error during authentication.\n");
                        break;
                }
                continue;
            }
            
            // Check for database creation or use commands before requiring authentication
            if (strncasecmp(trimmed_input, "create database", 15) == 0 ||
                strncasecmp(trimmed_input, "use database", 12) == 0) {
                
                // Always require authentication for database operations
                if (!db || !db_is_authenticated(db)) {
                    printf("Error: Authentication required. Please login first.\n");
                    printf("Use 'LOGIN username password' to authenticate.\n");
                    continue;
                }
                
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
            
            // For user creation commands
            if (strncasecmp(trimmed_input, "create user", 11) == 0) {
                if (!db) {
                    printf("Error: No database is currently open.\n");
                    printf("Create or open a database first with 'CREATE DATABASE name' or 'USE DATABASE name'\n");
                    continue;
                }
                
                if (!db_is_authenticated(db)) {
                    printf("Error: Authentication required. Please login first.\n");
                    printf("Use 'LOGIN username password' to authenticate.\n");
                    continue;
                }
                
                // Check user creation permission first
                if (!db_check_permission(db, "CREATE_USER")) {
                    printf("Error: Permission denied. Only administrators can create users.\n");
                    printf("You don't have sufficient privileges. Please ask an admin for assistance.\n");
                    continue;
                }
                
                switch (prepare_statement(input_buf, &statement)) {
                    case PREPARE_SUCCESS:
                        statement.db = db;
                        break;
                    default:
                        printf("Syntax error. Could not parse statement.\n");
                        printf("Correct syntax: CREATE USER username PASSWORD password ROLE role\n");
                        printf("Roles: ADMIN, DEVELOPER, USER\n");
                        continue;
                }
                
                ExecuteResult result = execute_statement(&statement, db);
                if (result == EXECUTE_SUCCESS) {
                    printf("User '%s' created successfully.\n", statement.auth_username);
                } else if (result == EXECUTE_PERMISSION_DENIED) {
                    // Already handled in execute_statement
                } else {
                    printf("Error creating user.\n");
                }
                continue;
            }
            
            // For all other commands, require an active database
            if (!db) {
                printf("Error: No database is currently open.\n");
                printf("Create or open a database first with 'CREATE DATABASE name' or 'USE DATABASE name'\n");
                continue;
            }
            
            // Require authentication for all other operations
            if (!db_is_authenticated(db)) {
                printf("Error: Authentication required. Please login first.\n");
                printf("Use 'LOGIN username password' to authenticate.\n");
                continue;
            }
            
            switch (prepare_statement(input_buf, &statement)) {
                case PREPARE_SUCCESS:
                    // Add database reference to statement
                    statement.db = db;
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
                        trimmed_input);
                    continue;
            }
            
            ExecuteResult result = execute_statement(&statement, db);
            switch (result) {
                case EXECUTE_SUCCESS:
                    printf("Executed.\n");
                    break;
                case EXECUTE_DUPLICATE_KEY:
                    // Already handled in execute_insert
                    break;
                case EXECUTE_TABLE_FULL:
                    printf("Error: Table full.\n");
                    break;
                case EXECUTE_PERMISSION_DENIED:
                    // Already handled in execute_statement
                    break;
                case EXECUTE_UNRECOGNIZED_STATEMENT:
                    printf("Unrecognized statement at '%s'.\n", trimmed_input);
                    break;
            }
        }
    }
}