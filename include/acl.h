#ifndef ACL_H
#define ACL_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h> // For time_t

#define MAX_USERS 100
#define MAX_USERNAME_SIZE 64
#define MAX_PASSWORD_SIZE 256
#define MAX_ACTIVE_SESSIONS 10  // Maximum number of concurrent active sessions

// Role types
typedef enum {
    ROLE_ADMIN,     // Full access
    ROLE_DEVELOPER, // Read-write, cannot drop
    ROLE_USER       // Read-only
} RoleType;

// Command types for permission checking
typedef enum {
    CMD_READ,       // SELECT, SHOW
    CMD_WRITE,      // INSERT, UPDATE
    CMD_CREATE,     // CREATE TABLE
    CMD_DROP,       // DROP TABLE, DROP DATABASE
    CMD_DELETE,     // DELETE operations
    CMD_GRANT,      // Grant permissions
    CMD_REVOKE      // Revoke permissions
} CommandType;

// User definition
typedef struct {
    char username[MAX_USERNAME_SIZE];
    char password_hash[MAX_PASSWORD_SIZE];
    bool is_active;
} User;

// User-Role mapping
typedef struct {
    char username[MAX_USERNAME_SIZE];
    RoleType role;
} UserRole;

// User session tracking
typedef struct {
    char username[MAX_USERNAME_SIZE];
    time_t login_time;
    bool is_active;
} UserSession;

// ACL container
typedef struct {
    User users[MAX_USERS];
    uint32_t num_users;
    UserRole user_roles[MAX_USERS]; // One role per user
    uint32_t num_user_roles;
    
    // Multiple active sessions instead of single current_user
    UserSession active_sessions[MAX_ACTIVE_SESSIONS];
    uint32_t num_active_sessions;
    char current_user[MAX_USERNAME_SIZE];  // Store the currently logged-in user
} ACL;

// ACL Functions
void acl_init(ACL* acl);
bool acl_save(ACL* acl, const char* db_name);
bool acl_load(ACL* acl, const char* db_name);
bool acl_add_user(ACL* acl, const char* username, const char* password);
bool acl_delete_user(ACL* acl, const char* username);
bool acl_assign_role(ACL* acl, const char* username, RoleType role);
bool acl_remove_role(ACL* acl, const char* username);
bool acl_authenticate(ACL* acl, const char* username, const char* password);
bool acl_login(ACL* acl, const char* username, const char* password);
void acl_logout(ACL* acl);
bool acl_logout_user(ACL* acl, const char* username);
bool acl_has_permission(ACL* acl, const char* username, CommandType cmd_type);
bool acl_is_admin(const ACL* acl, const char* username);
bool acl_is_user_active(ACL* acl, const char* username);
void acl_list_active_users(ACL* acl);
void acl_create_admin(ACL* acl, const char* username, const char* password);

#define USERNAME_MAX_LENGTH 32  // Standard username length limit

#endif // ACL_H