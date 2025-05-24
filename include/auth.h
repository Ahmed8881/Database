#ifndef AUTH_H
#define AUTH_H

#include <stdbool.h>
#include <stdint.h>

// User roles with different permission levels
typedef enum {
    ROLE_ADMIN,     // Full access
    ROLE_DEVELOPER, // Read-write access (no drop)
    ROLE_USER       // Read-only access
} UserRole;

typedef struct {
    char username[64];
    char password_hash[128]; // Store password hash, not plain text
    UserRole role;
    bool active;
} User;

typedef struct {
    User* users;
    uint32_t count;
    uint32_t capacity;
    int current_user_index; // -1 if no user is logged in
} UserManager;

// Initialize the user management system
void auth_init(UserManager* manager);

// Create a new user
bool auth_create_user(UserManager* manager, const char* username, 
                     const char* password, UserRole role);

// Authenticate a user
bool auth_login(UserManager* manager, const char* username, const char* password);

// Log out the current user
void auth_logout(UserManager* manager);

// Check if a user is logged in
bool auth_is_logged_in(UserManager* manager);

// Get the current user's role
UserRole auth_get_current_role(UserManager* manager);

// Get the current username
const char* auth_get_current_username(UserManager* manager);

// Check if the current user has permission for an operation
bool auth_check_permission(UserManager* manager, const char* operation);

// Save users to a file
bool auth_save_users(UserManager* manager, const char* db_name);

// Load users from a file
bool auth_load_users(UserManager* manager, const char* db_name);

// Free resources
void auth_cleanup(UserManager* manager);

// Simple string hash function for passwords
uint64_t hash_password(const char* password);

// Transfer authentication state between databases
bool auth_transfer_state(UserManager* source, UserManager* target);

#endif // AUTH_H
