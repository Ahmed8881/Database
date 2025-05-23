#include "../include/acl.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <openssl/sha.h>
#include <openssl/evp.h>


static void hash_password(const char* password, char* hash_out, size_t hash_size) {
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len;
    
    EVP_DigestInit_ex(ctx, EVP_sha256(), NULL);
    EVP_DigestUpdate(ctx, password, strlen(password));
    EVP_DigestFinal_ex(ctx, hash, &hash_len);
    EVP_MD_CTX_free(ctx);
    
    // Convert binary hash to hex string
    for (size_t i = 0; i < hash_len && i*2+1 < hash_size; i++) {
        sprintf(hash_out + (i * 2), "%02x", hash[i]);
    }
    hash_out[hash_size-1] = '\0';
}

void acl_init(ACL* acl) {
    acl->num_users = 0;
    acl->num_user_roles = 0;
    acl->num_active_sessions = 0;
    
    // Initialize current_user to empty string
    acl->current_user[0] = '\0';
}

bool acl_save(ACL* acl, const char* db_name) {
    char filename[512];
    snprintf(filename, sizeof(filename), "Database/%s/%s.acl", db_name, db_name);
    
    FILE* file = fopen(filename, "wb");
    if (!file) {
        return false;
    }
    
    // Write number of users
    fwrite(&acl->num_users, sizeof(uint32_t), 1, file);
    
    // Write users
    for (uint32_t i = 0; i < acl->num_users; i++) {
        User* user = &acl->users[i];
        fwrite(user->username, MAX_USERNAME_SIZE, 1, file);
        fwrite(user->password_hash, MAX_PASSWORD_SIZE, 1, file);
        fwrite(&user->is_active, sizeof(bool), 1, file);
    }
    
    // Write number of user roles
    fwrite(&acl->num_user_roles, sizeof(uint32_t), 1, file);
    
    // Write user roles
    for (uint32_t i = 0; i < acl->num_user_roles; i++) {
        UserRole* role = &acl->user_roles[i];
        fwrite(role->username, MAX_USERNAME_SIZE, 1, file);
        fwrite(&role->role, sizeof(RoleType), 1, file);
    }
    
    fclose(file);
    return true;
}

bool acl_load(ACL* acl, const char* db_name) {
    char filename[512];
    snprintf(filename, sizeof(filename), "Database/%s/%s.acl", db_name, db_name);
    
    FILE* file = fopen(filename, "rb");
    if (!file) {
        // If file doesn't exist, initialize empty ACL
        acl_init(acl);
        return true;
    }
    
    // Read number of users
    if (fread(&acl->num_users, sizeof(uint32_t), 1, file) != 1) {
        fclose(file);
        return false;
    }
    
    // Read users
    for (uint32_t i = 0; i < acl->num_users; i++) {
        User* user = &acl->users[i];
        if (fread(user->username, MAX_USERNAME_SIZE, 1, file) != 1 ||
            fread(user->password_hash, MAX_PASSWORD_SIZE, 1, file) != 1 ||
            fread(&user->is_active, sizeof(bool), 1, file) != 1) {
            fclose(file);
            return false;
        }
    }
    
    // Read number of user roles
    if (fread(&acl->num_user_roles, sizeof(uint32_t), 1, file) != 1) {
        fclose(file);
        return false;
    }
    
    // Read user roles
    for (uint32_t i = 0; i < acl->num_user_roles; i++) {
        UserRole* role = &acl->user_roles[i];
        if (fread(role->username, MAX_USERNAME_SIZE, 1, file) != 1 ||
            fread(&role->role, sizeof(RoleType), 1, file) != 1) {
            fclose(file);
            return false;
        }
    }
    
    fclose(file);
    return true;
}

bool acl_add_user(ACL* acl, const char* username, const char* password) {
    // Check if user already exists
    for (uint32_t i = 0; i < acl->num_users; i++) {
        if (strcmp(acl->users[i].username, username) == 0) {
            return false;
        }
    }
    
    // Check if we have room
    if (acl->num_users >= MAX_USERS) {
        return false;
    }
    
    // Add the user
    User* new_user = &acl->users[acl->num_users];
    strncpy(new_user->username, username, MAX_USERNAME_SIZE - 1);
    new_user->username[MAX_USERNAME_SIZE - 1] = '\0';
    
    // Hash and store password
    hash_password(password, new_user->password_hash, MAX_PASSWORD_SIZE);
    new_user->is_active = true;
    
    acl->num_users++;
    return true;
}

bool acl_delete_user(ACL* acl, const char* username) {
    // Find the user
    int user_idx = -1;
    for (uint32_t i = 0; i < acl->num_users; i++) {
        if (strcmp(acl->users[i].username, username) == 0) {
            user_idx = i;
            break;
        }
    }
    
    if (user_idx == -1) {
        return false;
    }
    
    // Remove user by shifting
    for (uint32_t i = user_idx; i < acl->num_users - 1; i++) {
        acl->users[i] = acl->users[i + 1];
    }
    acl->num_users--;
    
    // Remove any associated roles
    for (uint32_t i = 0; i < acl->num_user_roles; i++) {
        if (strcmp(acl->user_roles[i].username, username) == 0) {
            // Shift remaining roles
            for (uint32_t j = i; j < acl->num_user_roles - 1; j++) {
                acl->user_roles[j] = acl->user_roles[j + 1];
            }
            acl->num_user_roles--;
            i--; // Check this index again
        }
    }
    
    return true;
}

bool acl_assign_role(ACL* acl, const char* username, RoleType role) {
    // Check if trying to assign admin role when an admin already exists
    if (role == ROLE_ADMIN) {
        // Don't restrict the default admin user
        if (strcmp(username, "admin") != 0) {
            // Check if there's already an admin user
            for (uint32_t i = 0; i < acl->num_user_roles; i++) {
                if (acl->user_roles[i].role == ROLE_ADMIN) {
                    // There's already an admin user
                    printf("Error: Cannot create admin user '%s'. Only one admin user allowed.\n", username);
                    return false;
                }
            }
        }
    }
    
    // Find the user's current role entry (if any)
    int slot = -1;
    for (uint32_t i = 0; i < acl->num_user_roles; i++) {
        if (strcmp(acl->user_roles[i].username, username) == 0) {
            slot = i;
            break;
        }
    }
    
    // If role entry not found, create a new one
    if (slot == -1) {
        if (acl->num_user_roles >= MAX_USERS) {
            // No room for new role mapping
            return false;
        }
        slot = acl->num_user_roles++;
    }
    
    // Assign the role
    strncpy(acl->user_roles[slot].username, username, MAX_USERNAME_SIZE - 1);
    acl->user_roles[slot].username[MAX_USERNAME_SIZE - 1] = '\0';
    acl->user_roles[slot].role = role;
    
    return true;
}

bool acl_remove_role(ACL* acl, const char* username) {
    // Find role
    int role_idx = -1;
    for (uint32_t i = 0; i < acl->num_user_roles; i++) {
        if (strcmp(acl->user_roles[i].username, username) == 0) {
            role_idx = i;
            break;
        }
    }
    
    if (role_idx == -1) {
        return false;
    }
    
    // Remove role by shifting
    for (uint32_t i = role_idx; i < acl->num_user_roles - 1; i++) {
        acl->user_roles[i] = acl->user_roles[i + 1];
    }
    acl->num_user_roles--;
    
    return true;
}

bool acl_authenticate(ACL* acl, const char* username, const char* password) {
    // Find user
    for (uint32_t i = 0; i < acl->num_users; i++) {
        User* user = &acl->users[i];
        if (strcmp(user->username, username) == 0) {
            // Check if active
            if (!user->is_active) {
                return false;
            }
            
            // Check password
            char hashed_input[MAX_PASSWORD_SIZE];
            hash_password(password, hashed_input, MAX_PASSWORD_SIZE);
            
            // For debugging
            #ifdef DEBUG
            printf("DEBUG: Provided username: %s, password: %s\n", username, password);
            printf("DEBUG: Stored hash: %s\n", user->password_hash);
            printf("DEBUG: Computed hash: %s\n", hashed_input);
            #endif
            
            if (strcmp(user->password_hash, hashed_input) == 0) {
                // Set current user
                strncpy(acl->current_user, username, MAX_USERNAME_SIZE - 1);
                acl->current_user[MAX_USERNAME_SIZE - 1] = '\0';
                
                // Check if user is already in active sessions
                for (uint32_t j = 0; j < acl->num_active_sessions; j++) {
                    if (strcmp(acl->active_sessions[j].username, username) == 0) {
                        // User is already logged in, just update the session time
                        acl->active_sessions[j].login_time = time(NULL);
                        return true;
                    }
                }
                
                // Add user to active sessions if there's space
                if (acl->num_active_sessions < MAX_ACTIVE_SESSIONS) {
                    UserSession* session = &acl->active_sessions[acl->num_active_sessions];
                    strncpy(session->username, username, MAX_USERNAME_SIZE - 1);
                    session->username[MAX_USERNAME_SIZE - 1] = '\0';
                    session->login_time = time(NULL);
                    session->is_active = true;
                    acl->num_active_sessions++;
                    return true;
                } else {
                    printf("Warning: Maximum number of active sessions reached.\n");
                    return false;
                }
            }
            
            return false;
        }
    }
    
    return false;
}

// New function to check if a user is active
bool acl_is_user_active(ACL* acl, const char* username) {
    for (uint32_t i = 0; i < acl->num_active_sessions; i++) {
        if (strcmp(acl->active_sessions[i].username, username) == 0 &&
            acl->active_sessions[i].is_active) {
            return true;
        }
    }
    return false;
}

// New function to logout a specific user
bool acl_logout_user(ACL* acl, const char* username) {
    for (uint32_t i = 0; i < acl->num_active_sessions; i++) {
        if (strcmp(acl->active_sessions[i].username, username) == 0) {
            // Remove this session by shifting the rest down
            for (uint32_t j = i; j < acl->num_active_sessions - 1; j++) {
                acl->active_sessions[j] = acl->active_sessions[j + 1];
            }
            acl->num_active_sessions--;
            printf("User '%s' logged out.\n", username);
            return true;
        }
    }
    printf("User '%s' is not currently logged in.\n", username);
    return false;
}

// New function to list all active users
void acl_list_active_users(ACL* acl) {
    if (acl->num_active_sessions == 0) {
        printf("No active users.\n");
        return;
    }
    
    printf("Active users (%u):\n", acl->num_active_sessions);
    for (uint32_t i = 0; i < acl->num_active_sessions; i++) {
        UserSession* session = &acl->active_sessions[i];
        
        // Format login time
        char time_buf[64];
        struct tm* tm_info = localtime(&session->login_time);
        strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);
        
        printf("  - %s (logged in at %s)\n", session->username, time_buf);
    }
}

RoleType acl_get_user_role(const ACL* acl, const char* username) {
    // Find user's role
    for (uint32_t i = 0; i < acl->num_user_roles; i++) {
        if (strcmp(acl->user_roles[i].username, username) == 0) {
            return acl->user_roles[i].role;
        }
    }
    
    // Default to lowest privileges
    return ROLE_USER;
}

bool acl_has_permission(ACL* acl, const char* username, CommandType cmd_type) {
    RoleType role = acl_get_user_role(acl, username);
    
    switch (role) {
        case ROLE_ADMIN:
            // Admin has all permissions
            return true;
            
        case ROLE_DEVELOPER:
            // Developer can create tables and write data, but can't delete or perform admin operations
            return (cmd_type == CMD_READ || cmd_type == CMD_WRITE || cmd_type == CMD_CREATE);
            
        case ROLE_USER:
            // User can only read
            return cmd_type == CMD_READ;
            
        default:
            return false;
    }
}

bool acl_is_admin(const ACL* acl, const char* username) {
    return acl_get_user_role(acl, username) == ROLE_ADMIN;
}

// Implementation of acl_login function
bool acl_login(ACL* acl, const char* username, const char* password) {
    // Find user
    int user_idx = -1;
    for (uint32_t i = 0; i < acl->num_users; i++) {
        if (strcmp(acl->users[i].username, username) == 0) {
            user_idx = i;
            break;
        }
    }
    
    if (user_idx == -1) {
        return false;  // User not found
    }
    
    User* user = &acl->users[user_idx];
    
    // Check if user is active
    if (!user->is_active) {
        return false;
    }
    
    // Check password
    char hashed_input[MAX_PASSWORD_SIZE];
    hash_password(password, hashed_input, MAX_PASSWORD_SIZE);
    
    if (strcmp(user->password_hash, hashed_input) == 0) {
        // Set as current user
        strncpy(acl->current_user, username, MAX_USERNAME_SIZE - 1);
        acl->current_user[MAX_USERNAME_SIZE - 1] = '\0';
        
        // Add or update session
        bool found = false;
        for (uint32_t i = 0; i < acl->num_active_sessions; i++) {
            if (strcmp(acl->active_sessions[i].username, username) == 0) {
                // Update existing session
                acl->active_sessions[i].login_time = time(NULL);
                acl->active_sessions[i].is_active = true;
                found = true;
                break;
            }
        }
        
        if (!found && acl->num_active_sessions < MAX_ACTIVE_SESSIONS) {
            // Add new session
            UserSession* session = &acl->active_sessions[acl->num_active_sessions];
            strncpy(session->username, username, MAX_USERNAME_SIZE - 1);
            session->username[MAX_USERNAME_SIZE - 1] = '\0';
            session->login_time = time(NULL);
            session->is_active = true;
            acl->num_active_sessions++;
        }
        
        return true;
    }
    
    return false;
}

// Function to create the initial admin user for a new database
void acl_create_admin(ACL* acl, const char* username, const char* password) {
    // Find an empty slot for the new user
    int slot = -1;
    for (int i = 0; i < MAX_USERS; i++) {
        if (!acl->users[i].is_active) {
            slot = i;
            break;
        }
    }
    
    if (slot == -1) {
        // No empty slots available
        return;
    }
    
    // Initialize the admin user
    acl->users[slot].is_active = true;
    strncpy(acl->users[slot].username, username, MAX_USERNAME_SIZE - 1);
    acl->users[slot].username[MAX_USERNAME_SIZE - 1] = '\0';
    
    // Set password hash (hash the password)
    hash_password(password, acl->users[slot].password_hash, MAX_PASSWORD_SIZE);
    
    // Add a role mapping for this user
    int role_slot = -1;
    for (int i = 0; i < MAX_USERS; i++) {
        if (acl->user_roles[i].username[0] == '\0' || 
            strcmp(acl->user_roles[i].username, username) == 0) {
            role_slot = i;
            break;
        }
    }
    
    if (role_slot != -1) {
        strncpy(acl->user_roles[role_slot].username, username, MAX_USERNAME_SIZE - 1);
        acl->user_roles[role_slot].username[MAX_USERNAME_SIZE - 1] = '\0';
        acl->user_roles[role_slot].role = ROLE_ADMIN;
        
        if (acl->num_user_roles <= role_slot) {
            acl->num_user_roles = role_slot + 1;
        }
    }
    
    // Update user count if needed
    if (acl->num_users <= slot) {
        acl->num_users = slot + 1;
    }
}

// Add a simplified logout function
void acl_logout(ACL* acl) {
    // Clear the current user's name
    acl->current_user[0] = '\0';
    
    // Keep the session tracking intact for reconnection
}