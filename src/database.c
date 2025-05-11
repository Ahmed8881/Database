#include "../include/database.h"
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

// Helper function to create directory if it doesn't exist
static bool ensure_directory_exists(const char* path) {
    struct stat st = {0};
    if (stat(path, &st) == -1) {
        // Directory doesn't exist, create it
        #ifdef _WIN32
        // Windows
        if (mkdir(path) == -1) {
            return false;
        }
        #else
        // Unix/Linux/MacOS
        if (mkdir(path, 0755) == -1) {
            // Check if error is because directory already exists
            if (errno != EEXIST) {
                return false;
            }
        }
        #endif
    }
    return true;
}

// Helper function to migrate table files to the correct location
static bool migrate_table_if_needed(const char* old_path, const char* new_path) {
    // Check if old file exists
    struct stat st;
    if (stat(old_path, &st) == 0) {
        // Old file exists, rename it to new path
        if (rename(old_path, new_path) != 0) {
            printf("Warning: Could not migrate table from %s to %s\n", old_path, new_path);
            return false;
        }
        printf("Migrated table: %s -> %s\n", old_path, new_path);
        return true;
    }
    return false;
}

// Fix potential buffer overflow in snprintf
static char tables_dir_buffer[512]; // Global buffer to ensure enough space

Database* db_create_database(const char* name) {
    // Ensure base Database directory exists
    if (!ensure_directory_exists("Database")) {
        printf("Error: Failed to create base Database directory\n");
        return NULL;
    }
    
    char database_dir[512];
    snprintf(database_dir, sizeof(database_dir), "Database/%s", name);
    
    // Create main database directory
    if (!ensure_directory_exists(database_dir)) {
        printf("Error: Failed to create database directory: %s\n", database_dir);
        return NULL;
    }
    
    // Create Tables directory with safe string construction
    char tables_dir[512];
    size_t required_size = strlen(database_dir) + 8; // "/Tables" + null terminator
    if (required_size < sizeof(tables_dir)) {
        snprintf(tables_dir, sizeof(tables_dir), "%s/Tables", database_dir);
        if (!ensure_directory_exists(tables_dir)) {
            printf("Error: Failed to create Tables directory: %s\n", tables_dir);
            return NULL;
        }
    } else {
        printf("Error: Database path too long: %zu bytes needed, %zu available\n", 
               required_size, sizeof(tables_dir));
        return NULL;
    }
    
    // Open or create the database
    return db_open_database(name);
}

Database* db_open_database(const char* name) {
    char database_dir[512];
    snprintf(database_dir, sizeof(database_dir), "Database/%s", name);
    
    // Check if the database directory exists
    struct stat st = {0};
    if (stat(database_dir, &st) == -1) {
        printf("Error: Database '%s' does not exist.\n", name);
        return NULL;
    }
    
    // Make sure the Tables directory exists
    snprintf(tables_dir_buffer, sizeof(tables_dir_buffer), "Database/%s/Tables", name);
    if (!ensure_directory_exists(tables_dir_buffer)) {
        printf("Error: Failed to create Tables directory: %s\n", tables_dir_buffer);
        return NULL;
    }
    
    Database* db = malloc(sizeof(Database));
    if (!db) {
        return NULL;
    }
    
    strncpy(db->name, name, sizeof(db->name));
    db->name[sizeof(db->name) - 1] = '\0';
    
    // Load or initialize catalog
    char catalog_path[512];
    snprintf(catalog_path, sizeof(catalog_path), "Database/%s/%s.catalog", name, name);
    
    if (!catalog_load_from_path(&db->catalog, catalog_path)) {
        free(db);
        return NULL;
    }
    
    // IMPORTANT: Make sure the catalog has the database name
    strncpy(db->catalog.database_name, name, sizeof(db->catalog.database_name) - 1);
    db->catalog.database_name[sizeof(db->catalog.database_name) - 1] = '\0';
    
    // Check for tables in wrong location and migrate them
    for (uint32_t i = 0; i < db->catalog.num_tables; i++) {
        TableDef* table = &db->catalog.tables[i];
        
        // Construct correct path
        char correct_path[512];
        snprintf(correct_path, sizeof(correct_path), "Database/%s/Tables/%s.tbl", 
                 name, table->name);
        
        // If the path is not correct
        if (strcmp(table->filename, correct_path) != 0) {
            // Try to migrate from old path to new path
            migrate_table_if_needed(table->filename, correct_path);
            
            // Update path in catalog
            strncpy(table->filename, correct_path, sizeof(table->filename) - 1);
        }
    }
    
    // Save catalog with corrected paths
    catalog_save(&db->catalog, db->name);
    
    // If there's an active table, open it
    db->active_table = NULL;
    if (db->catalog.num_tables > 0) {
        TableDef* table_def = catalog_get_active_table(&db->catalog);
        if (table_def) {
            db->active_table = db_open(table_def->filename);
            if (db->active_table) {
                db->active_table->root_page_num = table_def->root_page_num;
            }
        }
    }
    
    return db;
}

bool db_create_table(Database* db, const char* name, ColumnDef* columns, uint32_t num_columns) {
    // Make sure the Tables directory exists
    snprintf(tables_dir_buffer, sizeof(tables_dir_buffer), "Database/%s/Tables", db->name);
    if (!ensure_directory_exists(tables_dir_buffer)) {
        printf("Error: Failed to create Tables directory: %s\n", tables_dir_buffer);
        return false;
    }
    
    // Add table to catalog
    if (!catalog_add_table(&db->catalog, name, columns, num_columns)) {
        return false;
    }
    
    // Set the new table as active
    catalog_set_active_table(&db->catalog, name);
    TableDef* table_def = catalog_get_active_table(&db->catalog);
    
    // Close current active table if any
    if (db->active_table) {
        table_def->root_page_num = db->active_table->root_page_num;
        db_close(db->active_table);
        db->active_table = NULL;
    }
    
    // Open new table
    db->active_table = db_open(table_def->filename);
    table_def->root_page_num = db->active_table->root_page_num;
    
    // Save updated catalog
    catalog_save(&db->catalog, db->name);
    
    return true;
}

bool db_use_table(Database* db, const char* table_name) {
    // Find and set active table in catalog
    if (!catalog_set_active_table(&db->catalog, table_name)) {
        return false;
    }
    
    TableDef* table_def = catalog_get_active_table(&db->catalog);
    
    // Close current active table if any
    if (db->active_table) {
        // Save current table's root page number before closing
        TableDef* current_table_def = catalog_get_active_table(&db->catalog);
        current_table_def->root_page_num = db->active_table->root_page_num;
        
        db_close(db->active_table);
        db->active_table = NULL;
    }
    
    // Ensure the path in the table definition is correct
    char correct_path[512];
    snprintf(correct_path, sizeof(correct_path), "Database/%s/Tables/%s.tbl", 
             db->name, table_name);
    
    // Update the path if needed
    if (strcmp(table_def->filename, correct_path) != 0) {
        strncpy(table_def->filename, correct_path, sizeof(table_def->filename) - 1);
    }
    
    // Open new active table
    db->active_table = db_open(table_def->filename);
    
    // Update root page number from catalog
    db->active_table->root_page_num = table_def->root_page_num;
    
    // Save updated catalog
    catalog_save(&db->catalog, db->name);
    
    return true;
}

void db_close_database(Database* db) {
    if (db == NULL) {
        return;
    }
    
    // Save current active table's root page number
    if (db->active_table) {
        TableDef* table_def = catalog_get_active_table(&db->catalog);
        if (table_def) {
            table_def->root_page_num = db->active_table->root_page_num;
        }
        
        // Close active table
        db_close(db->active_table);
    }
    
    // Save catalog before closing
    catalog_save(&db->catalog, db->name);
    
    free(db);
}

bool catalog_save_to_database(Catalog* catalog, const char* db_name) {
    (void)catalog; // Mark as used to avoid warning
    
    char catalog_path[512];
    snprintf(catalog_path, sizeof(catalog_path), "Database/%s/%s.catalog", db_name, db_name);
    
    FILE* file = fopen(catalog_path, "wb");
    if (!file) {
        return false;
    }
    
    // Write the catalog data here
    // Implement this function or return false
    fclose(file);
    return false; // Not implemented
}