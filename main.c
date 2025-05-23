#include "include/command_processor.h"
#include "include/input_handling.h"
#include "include/table.h"
#include "include/btree.h"
#include "include/cursor.h"
#include "include/pager.h"
#include "include/database.h"
#include "include/catalog.h"
<<<<<<< HEAD
#include "include/auth.h"
#include <string.h>
#include <strings.h> // Add this for strncasecmp
=======
#include "include/acl.h"
#include <strings.h>  // For strncasecmp
#include <string.h>   // For other string operations

// Global authentication state (before any database is selected)
typedef struct {
    ACL acl;
    bool authenticated;
    bool auth_required;
} GlobalAuth;

// Initialize global authentication
void init_global_auth(GlobalAuth* auth) {
    acl_init(&auth->acl);
    auth->authenticated = false;
    auth->auth_required = true;
    
    // Create default admin user
    acl_create_admin(&auth->acl, "admin", "jhaz");
    
    // Set current user to empty (not logged in)
    auth->acl.current_user[0] = '\0';
}

// Global authentication login
bool global_auth_login(GlobalAuth* auth, const char* username, const char* password) {
    if (acl_login(&auth->acl, username, password)) {
        auth->authenticated = true;
        return true;
    }
    return false;
}

// Global authentication logout
void global_auth_logout(GlobalAuth* auth) {
    acl_logout(&auth->acl);
    auth->authenticated = false;
}
>>>>>>> bf47354 (Implement Access Control List (ACL) functionality with user authentication)

int main(int argc, char *argv[]) {
    (void)argc; // Mark as used to avoid warning
    (void)argv; // Mark as used to avoid warning
    
    // Initialize global authentication (before any database)
    GlobalAuth global_auth;
    init_global_auth(&global_auth);
    
    // Start with no active database
    Database* db = NULL;
    
<<<<<<< HEAD
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
=======
    // Connection state
    bool connected = false;
    
    // Display welcome message
    printf("Welcome to Build-Your-Own-Database System\n");
    printf("Please login with 'LOGIN username 'password''\n");
    printf("Default admin credentials: username='admin', password='jhaz'\n\n");
    
    Input_Buffer *input_buf = newInputBuffer();
    while (1) {
        // Show prompt with current state
        if (global_auth.authenticated) {
            if (db) {
                printf("%s:%s> ", db->name, global_auth.acl.current_user);
            } else {
                printf("no-db:%s> ", global_auth.acl.current_user);
>>>>>>> bf47354 (Implement Access Control List (ACL) functionality with user authentication)
            }
        } else {
            printf("db > ");
        }
        
<<<<<<< HEAD
=======
        // Skip the print_prompt in read_input by setting a flag
        input_buf->prompt_displayed = true;
>>>>>>> bf47354 (Implement Access Control List (ACL) functionality with user authentication)
        read_input(input_buf);
        input_buf->prompt_displayed = false;
        
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
        
        // Process exit command regardless of login state
        if (strcmp(trimmed_input, ".exit") == 0) {
            if (db) {
                db_close_database(db);
            }
            exit(EXIT_SUCCESS);
        }
        
        // Handle login command regardless of current state
        if (strncasecmp(trimmed_input, "login", 5) == 0) {
            Statement statement;
            memset(&statement, 0, sizeof(Statement));
            
<<<<<<< HEAD
            // Check authentication for meta commands except exit
            if (strcmp(trimmed_input, ".exit") != 0 && !db_is_authenticated(db)) {
                printf("Error: Authentication required. Please login first.\n");
                printf("Use 'LOGIN username 'password'' to authenticate.\n");
                continue;
            }
            
=======
            switch (prepare_login(input_buf, &statement)) {
                case PREPARE_SUCCESS:
                    // Try global login first (if no database is open)
                    if (!db) {
                        if (global_auth_login(&global_auth, statement.user.username, statement.user.password)) {
                            printf("Logged in as '%s'.\n", statement.user.username);
                        } else {
                            printf("Invalid username or password.\n");
                        }
                    } else {
                        // If database is open, try database-specific login
                        if (acl_login(&db->acl, statement.user.username, statement.user.password)) {
                            global_auth.authenticated = true;
                            // Copy username to global auth for consistency
                            strncpy(global_auth.acl.current_user, db->acl.current_user, MAX_USERNAME_SIZE - 1);
                            global_auth.acl.current_user[MAX_USERNAME_SIZE - 1] = '\0';
                            printf("Logged in as '%s'.\n", statement.user.username);
                        } else {
                            printf("Invalid username or password.\n");
                        }
                    }
                    continue;
                default:
                    printf("Syntax error in login command.\n");
                    printf("Usage: LOGIN username 'password'\n");
                    continue;
            }
        }
        
        // Handle logout command
        if (strncasecmp(trimmed_input, "logout", 6) == 0) {
            if (global_auth.authenticated) {
                if (db) {
                    // Clear current user in both global and db authentication
                    acl_logout(&db->acl);
                }
                global_auth_logout(&global_auth);
                printf("Logged out successfully.\n");
            } else {
                printf("Not currently logged in.\n");
            }
            continue;
        }
        
        // Check if authentication is required and user is authenticated
        if (!global_auth.authenticated) {
            printf("Error: Authentication required. Please login first.\n");
            printf("Use 'LOGIN username 'password'' to authenticate.\n");
            continue;
        }
        
        // Now that we've authenticated, handle database creation/connection commands
        if (strncasecmp(trimmed_input, "create database", 15) == 0 ||
            strncasecmp(trimmed_input, "use database", 12) == 0) {
            
            Statement statement;
            memset(&statement, 0, sizeof(Statement));
            
            switch (prepare_database_statement(input_buf, &statement)) {
                case PREPARE_SUCCESS:
                    switch (execute_database_statement(&statement, &db)) {
                        case EXECUTE_SUCCESS:
                            connected = true;
                            
                            // When creating or connecting to a database, synchronize the current user
                            // from global auth to the database's ACL system
                            if (global_auth.authenticated && db) {
                                strncpy(db->acl.current_user, global_auth.acl.current_user, MAX_USERNAME_SIZE - 1);
                                db->acl.current_user[MAX_USERNAME_SIZE - 1] = '\0';
                            }
                            
                            printf("Executed.\n");
                            continue;
                        default:
                            printf("Error during database operation.\n");
                            continue;
                    }
                    break;
                default:
                    printf("Syntax error. Could not parse statement.\n");
                    continue;
            }
            continue; // Skip the rest of the loop after database commands
        }
        
        // Handle user management without a database (using global auth)
        if (strncasecmp(trimmed_input, "create user", 11) == 0 && !db) {
            Statement statement;
            memset(&statement, 0, sizeof(Statement));
            
            switch (prepare_create_user(input_buf, &statement)) {
                case PREPARE_SUCCESS:
                    // Only admin can create users
                    if (!acl_is_admin(&global_auth.acl, global_auth.acl.current_user)) {
                        printf("Error: Only admin users can create new users.\n");
                        continue;
                    }
                    
                    if (acl_add_user(&global_auth.acl, statement.user.username, statement.user.password)) {
                        acl_assign_role(&global_auth.acl, statement.user.username, statement.user.role);
                        // printf("User '%s' created successfully with role: ", statement.user.username);
                        
                        // Print role name
                        switch (statement.user.role) {
                            case ROLE_ADMIN: printf("admin\n"); break;
                            case ROLE_DEVELOPER: printf("developer\n"); break;
                            case ROLE_USER: printf("user\n"); break;
                            default: printf("unknown\n"); break;
                        }
                    } else {
                        printf("Failed to create user. User may already exist.\n");
                    }
                    continue;
                default:
                    printf("Syntax error in create user command.\n");
                    printf("Usage: CREATE USER username WITH PASSWORD 'password' [ROLE 'role']\n");
                    continue;
            }
        }
        
        // If not connected to a database, prompt to create or connect
        if (!connected) {
            printf("Error: No database is currently open.\n");
            printf("Create or open a database first with 'CREATE DATABASE name' or 'USE DATABASE name'\n");
            continue;
        }
        
        // Now handle regular commands with the database
        if (trimmed_input[0] == '.') {
>>>>>>> bf47354 (Implement Access Control List (ACL) functionality with user authentication)
            switch (do_meta_command(input_buf, db)) {
                case META_COMMAND_SUCCESS:
                    continue;
                case META_COMMAND_TXN_BEGIN:
                    db_begin_transaction(db);
                    continue;
                case META_COMMAND_TXN_COMMIT:
                    db_commit_transaction(db);
                    continue;
                case META_COMMAND_TXN_ROLLBACK:
                    db_rollback_transaction(db);
                    continue;
                case META_COMMAND_TXN_STATUS:
                    if (db->active_txn_id == 0) {
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
            
<<<<<<< HEAD
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
                    printf("Use 'LOGIN username 'password'' to authenticate.\n");
                    continue;
                }
                
                switch (prepare_statement(input_buf, &statement)) {
                    case PREPARE_SUCCESS:
                        statement.db = db;
                        break;
                    default:
                        printf("Syntax error. Could not parse statement.\n");
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
                printf("Use 'LOGIN username 'password'' to authenticate.\n");
                continue;
            }
            
=======
            // Handle authentication commands
            if (strncasecmp(trimmed_input, "enable auth", 11) == 0) {
                statement.type = STATEMENT_ENABLE_AUTH;
                execute_enable_auth(&statement, db);
                printf("Executed.\n");
                continue;
            }
            
            if (strncasecmp(trimmed_input, "disable auth", 12) == 0) {
                statement.type = STATEMENT_DISABLE_AUTH;
                if (!acl_is_admin(&db->acl, db->acl.current_user)) {
                    printf("Error: Only admin users can disable authentication.\n");
                    continue;
                }
                execute_disable_auth(&statement, db);
                printf("Executed.\n");
                continue;
            }
            
            // Handle user management commands
            if (strncasecmp(trimmed_input, "create user", 11) == 0) {
                switch (prepare_create_user(input_buf, &statement)) {
                    case PREPARE_SUCCESS:
                        if (execute_create_user(&statement, db) == EXECUTE_SUCCESS) {
                            printf("User '%s' created successfully.\n", statement.user.username);
                        }
                        continue;
                    default:
                        printf("Syntax error in create user command.\n");
                        printf("Usage: CREATE USER username WITH PASSWORD 'password' [ROLE 'role']\n");
                        continue;
                }
            }
            
            // Prepare and execute regular SQL statements
>>>>>>> bf47354 (Implement Access Control List (ACL) functionality with user authentication)
            switch (prepare_statement(input_buf, &statement)) {
                case PREPARE_SUCCESS:
                    // Add database reference to statement
                    statement.db = db;
                    
                    // Check permissions based on statement type and user role
                    if (db->auth_required && !check_permission(db, &statement)) {
                        printf("Error: Permission denied for this operation.\n");
                        printf("You don't have sufficient privileges. Please ask an admin for assistance.\n");
                        continue;
                    }
                    
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
                case EXECUTE_FAILED:
                    printf("Error: Command execution failed.\n");
                    break;
            }
        }
    }
}