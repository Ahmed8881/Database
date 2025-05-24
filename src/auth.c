#include "../include/auth.h"
#include "../include/utils.h"  // Add this include for my_strdup
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#ifdef __linux__
#include <strings.h>  // For strcasecmp on Linux
#endif

#define INITIAL_USER_CAPACITY 10

void auth_init(UserManager* manager) {
    manager->users = malloc(INITIAL_USER_CAPACITY * sizeof(User));
    if (manager->users) {
        manager->count = 0;
        manager->capacity = INITIAL_USER_CAPACITY;
        manager->current_user_index = -1;
        
        // Create default admin user if no users exist
        auth_create_user(manager, "admin", "jhaz", ROLE_ADMIN);
    }
}

bool auth_create_user(UserManager* manager, const char* username, 
                     const char* password, UserRole role) {
    // Check if username already exists
    for (uint32_t i = 0; i < manager->count; i++) {
        if (strcmp(manager->users[i].username, username) == 0) {
            return false; // Username already exists
        }
    }
    
    // Resize if needed
    if (manager->count >= manager->capacity) {
        uint32_t new_capacity = manager->capacity * 2;
        User* new_users = realloc(manager->users, new_capacity * sizeof(User));
        if (!new_users) {
            return false; // Out of memory
        }
        manager->users = new_users;
        manager->capacity = new_capacity;
    }
    
    // Add the new user
    User* new_user = &manager->users[manager->count];
    strncpy(new_user->username, username, sizeof(new_user->username) - 1);
    new_user->username[sizeof(new_user->username) - 1] = '\0';
    
    // Hash and store the password
    uint64_t hash_value = hash_password(password);
    snprintf(new_user->password_hash, sizeof(new_user->password_hash), "%llu", (unsigned long long)hash_value);
    
    new_user->role = role;
    new_user->active = true;
    
    manager->count++;
    return true;
}

bool auth_login(UserManager* manager, const char* username, const char* password) {
    uint64_t password_hash = hash_password(password);
    char hash_str[128];
    snprintf(hash_str, sizeof(hash_str), "%llu", (unsigned long long)password_hash);
    
    for (uint32_t i = 0; i < manager->count; i++) {
        if (strcmp(manager->users[i].username, username) == 0 && 
            strcmp(manager->users[i].password_hash, hash_str) == 0 &&
            manager->users[i].active) {
            manager->current_user_index = i;
            return true;
        }
    }
    
    return false;
}

void auth_logout(UserManager* manager) {
    manager->current_user_index = -1;
}

bool auth_is_logged_in(UserManager* manager) {
    return manager->current_user_index >= 0 && 
           manager->current_user_index < (int)manager->count;
}

UserRole auth_get_current_role(UserManager* manager) {
    if (!auth_is_logged_in(manager)) {
        return ROLE_USER; // Default to lowest privileges if not logged in
    }
    return manager->users[manager->current_user_index].role;
}

const char* auth_get_current_username(UserManager* manager) {
    if (!auth_is_logged_in(manager)) {
        return "guest";
    }
    return manager->users[manager->current_user_index].username;
}

bool auth_check_permission(UserManager* manager, const char* operation) {
    if (!auth_is_logged_in(manager)) {
        return false;
    }
    
    UserRole role = manager->users[manager->current_user_index].role;
    
    // Admin has full access
    if (role == ROLE_ADMIN) {
        return true;
    }
    
    // Developer has read-write access but cannot drop
    if (role == ROLE_DEVELOPER) {
        if (strcasecmp(operation, "DROP") == 0) {
            return false;
        }
        return true;
    }
    
    // User has read-only access
    if (role == ROLE_USER) {
        if (strcasecmp(operation, "SELECT") == 0 || 
            strcasecmp(operation, "SHOW") == 0) {
            return true;
        }
        return false;
    }
    
    return false;
}

bool auth_save_users(UserManager* manager, const char* db_name) {
    char filename[512];
    snprintf(filename, sizeof(filename), "Database/%s/users.auth", db_name);
    
    FILE* file = fopen(filename, "wb");
    if (!file) {
        return false;
    }
    
    // Write user count
    fwrite(&manager->count, sizeof(uint32_t), 1, file);
    
    // Write each user
    for (uint32_t i = 0; i < manager->count; i++) {
        fwrite(&manager->users[i], sizeof(User), 1, file);
    }
    
    fclose(file);
    return true;
}

bool auth_load_users(UserManager* manager, const char* db_name) {
    char filename[512];
    snprintf(filename, sizeof(filename), "Database/%s/users.auth", db_name);
    
    FILE* file = fopen(filename, "rb");
    if (!file) {
        // If file doesn't exist, initialize with default admin
        auth_init(manager);
        return true;
    }
    
    // Read user count
    uint32_t count;
    if (fread(&count, sizeof(uint32_t), 1, file) != 1) {
        fclose(file);
        return false;
    }
    
    // Allocate memory
    manager->users = malloc(count * sizeof(User));
    if (!manager->users) {
        fclose(file);
        return false;
    }
    
    // Read users
    if (fread(manager->users, sizeof(User), count, file) != count) {
        free(manager->users);
        fclose(file);
        return false;
    }
    
    manager->count = count;
    manager->capacity = count;
    manager->current_user_index = -1;
    
    fclose(file);
    return true;
}

void auth_cleanup(UserManager* manager) {
    if (manager->users) {
        free(manager->users);
        manager->users = NULL;
    }
    manager->count = 0;
    manager->capacity = 0;
    manager->current_user_index = -1;
}

uint64_t hash_password(const char* password) {
    // Simple hash function for demonstration
    // In production, use a proper cryptographic hash function
    uint64_t hash = 5381;
    int c;
    
    while ((c = *password++)) {
        hash = ((hash << 5) + hash) + c; // hash * 33 + c
    }
    
    return hash;
}

// Add function to get all active users
uint32_t auth_get_active_user_count(UserManager* manager) {
    uint32_t active_count = 0;
    for (uint32_t i = 0; i < manager->count; i++) {
        if (manager->users[i].active) {
            active_count++;
        }
    }
    return active_count;
}

// Add function to check if a specific username is logged in
bool auth_is_user_logged_in(UserManager* manager, const char* username) {
    for (uint32_t i = 0; i < manager->count; i++) {
        if (strcmp(manager->users[i].username, username) == 0 && 
            manager->users[i].active) {
            return true;
        }
    }
    return false;
}

// Add function to get a list of active users
char** auth_get_active_users(UserManager* manager, uint32_t* count) {
    // First count active users
    uint32_t active_count = auth_get_active_user_count(manager);
    
    if (active_count == 0) {
        *count = 0;
        return NULL;
    }
    
    // Allocate array of string pointers
    char** active_users = malloc(active_count * sizeof(char*));
    if (!active_users) {
        *count = 0;
        return NULL;
    }
    
    // Fill the array with active usernames
    uint32_t index = 0;
    for (uint32_t i = 0; i < manager->count && index < active_count; i++) {
        if (manager->users[i].active) {
            active_users[index] = my_strdup(manager->users[i].username);
            index++;
        }
    }
    
    *count = active_count;
    return active_users;
}

// Add function to free the active users list
void auth_free_active_users(char** active_users, uint32_t count) {
    if (active_users) {
        for (uint32_t i = 0; i < count; i++) {
            free(active_users[i]);
        }
        free(active_users);
    }
}

#include "../include/command_processor.h"
#include "../include/database.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

// Either remove this function if not needed or mark it as unused
static char* trim_string(char* str) __attribute__((unused));
static char* trim_string(char* str) {
    if (!str) return NULL;
    
    // Trim leading spaces
    while(isspace((unsigned char)*str)) str++;
    
    if(*str == 0) return str;  // All spaces
    
    // Trim trailing spaces
    char* end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    
    // Write new null terminator
    *(end+1) = 0;
    
    return str;
}

PrepareResult prepare_login(Input_Buffer *buf, Statement *statement) {
    statement->type = STATEMENT_LOGIN;
    
    // Parse login command: LOGIN username password
    char username[64] = {0};
    char password[64] = {0};
    
    // Use a more flexible approach that handles case differences
    char *input = buf->buffer;
    char *token;
    
    // Skip the "login" part (already verified by caller with strncasecmp)
    token = strtok(input, " \t");
    
    // Get username
    token = strtok(NULL, " \t");
    if (!token) {
        printf("Syntax error. Expected: LOGIN <username> <password>\n");
        return PREPARE_SYNTAX_ERROR;
    }
    strncpy(username, token, sizeof(username) - 1);
    
    // Get password
    token = strtok(NULL, " \t");
    if (!token) {
        printf("Syntax error. Expected: LOGIN <username> <password>\n");
        return PREPARE_SYNTAX_ERROR;
    }
    strncpy(password, token, sizeof(password) - 1);
    
    // Check for extra tokens (should be none)
    token = strtok(NULL, " \t");
    if (token) {
        printf("Syntax error. Expected: LOGIN <username> <password>\n");
        return PREPARE_SYNTAX_ERROR;
    }
    
    // Store credentials in the statement
    strncpy(statement->auth_username, username, sizeof(statement->auth_username) - 1);
    strncpy(statement->auth_password, password, sizeof(statement->auth_password) - 1);
    
    return PREPARE_SUCCESS;
}

PrepareResult prepare_logout(Input_Buffer *buf, Statement *statement) {
    statement->type = STATEMENT_LOGOUT;
    
    // No parameters needed for logout
    (void)buf;  // Mark as used to avoid compiler warning
    
    return PREPARE_SUCCESS;
}

PrepareResult prepare_create_user(Input_Buffer *buf, Statement *statement) {
    statement->type = STATEMENT_CREATE_USER;
    
    // Parse: CREATE USER username PASSWORD password ROLE role
    char username[64] = {0};
    char password[64] = {0};
    char role_str[20] = {0};
    
    // Use a more flexible parsing approach
    char *input = buf->buffer;
    char *token = strtok(input, " \t");  // Skip "CREATE"
    token = strtok(NULL, " \t");  // Skip "USER"
    
    // Get username
    token = strtok(NULL, " \t");
    if (!token) return PREPARE_SYNTAX_ERROR;
    strncpy(username, token, sizeof(username) - 1);
    
    // Skip to PASSWORD keyword
    token = strtok(NULL, " \t");
    if (!token || strcasecmp(token, "PASSWORD") != 0) {
        return PREPARE_SYNTAX_ERROR;
    }
    
    // Get password
    token = strtok(NULL, " \t");
    if (!token) return PREPARE_SYNTAX_ERROR;
    strncpy(password, token, sizeof(password) - 1);
    
    // Skip to ROLE keyword
    token = strtok(NULL, " \t");
    if (!token || strcasecmp(token, "ROLE") != 0) {
        return PREPARE_SYNTAX_ERROR;
    }
    
    // Get role
    token = strtok(NULL, " \t");
    if (!token) return PREPARE_SYNTAX_ERROR;
    strncpy(role_str, token, sizeof(role_str) - 1);
    
    // Store in statement
    strncpy(statement->auth_username, username, sizeof(statement->auth_username) - 1);
    strncpy(statement->auth_password, password, sizeof(statement->auth_password) - 1);
    
    // Parse role
    if (strcasecmp(role_str, "ADMIN") == 0) {
        statement->auth_role = ROLE_ADMIN;
    } else if (strcasecmp(role_str, "EDITOR") == 0 || strcasecmp(role_str, "DEVELOPER") == 0) {
        statement->auth_role = ROLE_DEVELOPER; // Changed from ROLE_EDITOR to ROLE_DEVELOPER
    } else if (strcasecmp(role_str, "VIEWER") == 0 || strcasecmp(role_str, "USER") == 0) {
        statement->auth_role = ROLE_USER; // Changed from ROLE_VIEWER to ROLE_USER
    } else {
        printf("Invalid role. Expected: ADMIN, DEVELOPER, or USER\n");
        return PREPARE_SYNTAX_ERROR;
    }
    
    return PREPARE_SUCCESS;
}

ExecuteResult execute_login(Statement *statement, Database *db) {
    // Check if the credentials are valid
    bool login_success = db_login(db, statement->auth_username, statement->auth_password);
    
    if (login_success) {
        printf("Login successful. Welcome, %s!\n", statement->auth_username);
        return EXECUTE_SUCCESS;
    } else {
        printf("Login failed. Invalid username or password.\n");
        return EXECUTE_AUTH_FAILED;
    }
}

ExecuteResult execute_logout(Statement *statement, Database *db) {
    (void)statement;  // Unused parameter
    
    if (db_is_authenticated(db)) {
        printf("Logged out successfully. Goodbye, %s!\n", auth_get_current_username(&db->user_manager));
        db_logout(db);
        return EXECUTE_SUCCESS;
    } else {
        printf("No user is currently logged in.\n");
        return EXECUTE_SUCCESS;
    }
}

ExecuteResult execute_create_user(Statement *statement, Database *db) {
    // Check if the current user has permission to create users
    if (db_is_authenticated(db) && auth_get_current_role(&db->user_manager) != ROLE_ADMIN) {
        printf("Error: Only administrators can create new users.\n");
        return EXECUTE_PERMISSION_DENIED;
    }
    
    // Create the new user
    bool success = db_create_user(db, statement->auth_username, 
                                 statement->auth_password, 
                                 statement->auth_role);
    
    if (success) {
        printf("User '%s' created successfully with role '%s'.\n", 
               statement->auth_username, 
               statement->auth_role == ROLE_ADMIN ? "ADMIN" :
               statement->auth_role == ROLE_DEVELOPER ? "DEVELOPER" : "USER");
        return EXECUTE_SUCCESS;
    } else {
        printf("Failed to create user. Username may already exist.\n");
        return EXECUTE_ERROR;
    }
}

// New function to transfer auth state between databases
bool auth_transfer_state(UserManager* source, UserManager* target) {
    if (!auth_is_logged_in(source)) {
        return false; // No user logged in to transfer
    }
    
    const char* username = auth_get_current_username(source);
    
    // Find matching user in target database
    for (uint32_t i = 0; i < target->count; i++) {
        if (strcmp(target->users[i].username, username) == 0) {
            target->current_user_index = i;
            return true;
        }
    }
    
    // User doesn't exist in target database
    // Could potentially create the user here, but for now just return false
    return false;
}
