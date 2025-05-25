#include "../include/table.h"
#include "../include/btree.h"
#include "../include/cursor.h"
#include "../include/pager.h"
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif
#include <stdarg.h>

// Function prototype for get_column_offset to prevent implicit declaration error
uint32_t get_column_offset(TableDef* table_def, uint32_t col_idx);

const uint32_t ID_SIZE = size_of_attribute(Row, id);
const uint32_t USERNAME_SIZE = size_of_attribute(Row, username);
const uint32_t EMAIL_SIZE = size_of_attribute(Row, email);
const uint32_t ID_OFFSET = 0;
const uint32_t USERNAME_OFFSET = ID_OFFSET + ID_SIZE;
const uint32_t EMAIL_OFFSET = USERNAME_OFFSET + USERNAME_SIZE;
const uint32_t ROW_SIZE = ID_SIZE + USERNAME_SIZE + EMAIL_SIZE;

const uint32_t PAGE_SIZE = 4096;
const uint32_t ROWS_PER_PAGE = PAGE_SIZE / ROW_SIZE;
const uint32_t TABLE_MAX_ROWS = ROWS_PER_PAGE * TABLE_MAX_PAGES;

void serialize_row(Row *source, void *destination)
{
  memcpy(destination + ID_OFFSET, &(source->id), ID_SIZE);
  memcpy(destination + USERNAME_OFFSET, &(source->username), USERNAME_SIZE);
  memcpy(destination + EMAIL_OFFSET, &(source->email), EMAIL_SIZE);
}

void deserialize_row(void *source, Row *destination)
{
  memcpy(&(destination->id), source + ID_OFFSET, ID_SIZE);
  memcpy(&(destination->username), source + USERNAME_OFFSET, USERNAME_SIZE);
  memcpy(&(destination->email), source + EMAIL_OFFSET, EMAIL_SIZE);
}

void print_row(Row *row)
{
  printf("(%d, %s, %s)\n", row->id, row->username, row->email);
}

void free_table(Table *table)
{
  for (uint32_t i = 0; i < TABLE_MAX_PAGES; i++)
  {
    free(table->pager->pages[i]);
  }
  free(table);
}

void *row_slot(Table *table, uint32_t row_num)
{
  uint32_t page_num = row_num / ROWS_PER_PAGE;
  void *page = get_page(table->pager, page_num);
  uint32_t row_offset = row_num % ROWS_PER_PAGE;
  uint32_t byte_offset = row_offset * ROW_SIZE;
  return page + byte_offset;
}

// initialze and open table
Table *db_open(const char *file_name)
{
  Pager *pager = pager_open(file_name);
  Table *table = malloc(sizeof(Table));
  table->pager = pager;
  table->root_page_num = 0;
  if (pager->num_pages == 0)
  {
    // New database file. Initialize page 0 as leaf node.
    void *root_node = get_page(pager, 0);
    initialize_leaf_node(root_node);
    set_node_root(root_node, true);
  }
  return table;
}

// Function to create all directories in a file path
static void create_path_for_file(const char* file_path) {
    char dir_path[512];
    strncpy(dir_path, file_path, sizeof(dir_path) - 1);
    dir_path[sizeof(dir_path) - 1] = '\0';
    
    // Find the last slash to get directory part
    char *last_slash = strrchr(dir_path, '/');
    if (last_slash == NULL) {
        // No directory part
        return;
    }
    
    *last_slash = '\0'; // Truncate at the last slash to get directory part
    
    // We'll build the path one component at a time
    char path_so_far[512] = {0};
    char *component = dir_path;
    
    while (*component) {
        // Find next path separator
        char *next_separator = strchr(component, '/');
        if (next_separator) {
            *next_separator = '\0'; // Temporarily terminate the string
        }
        
        // Add current component to the path so far
        if (path_so_far[0] != '\0') {
            strcat(path_so_far, "/");
        }
        strcat(path_so_far, component);
        
        // Create this directory if it doesn't exist
        struct stat st = {0};
        if (stat(path_so_far, &st) == -1) {
            #ifdef _WIN32
            mkdir(path_so_far);
            #else
            mkdir(path_so_far, 0755);
            #endif
            printf("Created directory: %s\n", path_so_far);
        }
        
        // Move to next component
        if (next_separator) {
            *next_separator = '/'; // Restore the path separator
            component = next_separator + 1;
        } else {
            break;
        }
    }
}

// function for openinng the file and initializes the pager object
Pager *pager_open(const char *file_name)
{
  // Create all necessary directories before opening file
  create_path_for_file(file_name);
  
  // open the file
  // O_RDWR: open the file for reading and writing
  // O_CREAT: create the file if it does not exist
  // S_IWUSR: user has permission to write
  // S_IRUSR: user has permission to read
  int fd = open(file_name, O_RDWR | O_CREAT, S_IWUSR | S_IRUSR);

  // os returns -1 if fails to open file
  if (fd == -1)
  {
    perror("Unable to open file"); // Print detailed error
    exit(EXIT_FAILURE);
  }
  // taking the file length means from 0 to end fo file
  off_t file_length = lseek(fd, 0, SEEK_END);
  Pager *pager = malloc(sizeof(Pager));
  pager->file_descriptor = fd;
  pager->file_length = file_length;
  pager->num_pages = (file_length / PAGE_SIZE);
  if (file_length % PAGE_SIZE != 0)
  {
    printf("Db file is not a whole number of pages. Corrupt file.\n");
    exit(EXIT_FAILURE);
  }
  for (uint32_t i = 0; i < TABLE_MAX_PAGES; i++)
  {
    pager->pages[i] = NULL;
  }

  return pager;
}
void *get_page(Pager *pager, uint32_t page_num)
{
  if (page_num > TABLE_MAX_PAGES)
  {
    printf("Tried to fetch page out of bounds. %d > %d\n", page_num,
           TABLE_MAX_PAGES);
    exit(EXIT_FAILURE);
  }
  if (pager->pages[page_num] == NULL)

  {
    void *page = malloc(PAGE_SIZE);
    uint32_t num_pages = pager->file_length / PAGE_SIZE;
    if (pager->file_length % PAGE_SIZE)
    {
      num_pages += 1;
    }
    if (page_num <= num_pages)
    {
      lseek(pager->file_descriptor, page_num * PAGE_SIZE, SEEK_SET);
      ssize_t bytes_read = read(pager->file_descriptor, page, PAGE_SIZE);
      if (bytes_read == -1)
      {
        printf("Error reading file: %d\n", errno);
        exit(EXIT_FAILURE);
      }
    }

    pager->pages[page_num] = page;
    if (page_num >= pager->num_pages)
    {
      pager->num_pages = page_num + 1;
    }
  }
  return pager->pages[page_num];
}
void pager_flush(Pager *pager, uint32_t page_num)
{
  if (pager->pages[page_num] == NULL)
  {
    printf("Tried to flush null page\n");
    exit(EXIT_FAILURE);
  }
  off_t offset = lseek(pager->file_descriptor, page_num * PAGE_SIZE, SEEK_SET);
  if (offset == -1)
  {
    printf("Error seeking: %d\n", errno);
    exit(EXIT_FAILURE);
  }
  ssize_t bytes_written =
      write(pager->file_descriptor, pager->pages[page_num], PAGE_SIZE);
  if (bytes_written == -1)
  {
    printf("Error writing: %d\n", errno);
    exit(EXIT_FAILURE);
  }
}
void db_close(Table *table)
{
  Pager *pager = table->pager;
  // uint32_t num_full_pages = table->num_rows / ROWS_PER_PAGE;
  for (uint32_t i = 0; i < pager->num_pages; i++)
  {
    if (pager->pages[i] == NULL)
    {
      continue;
    }
    pager_flush(pager, i);
    free(pager->pages[i]);
    pager->pages[i] = NULL;
  }
  int result = close(pager->file_descriptor);
  if (result == -1)
  {
    printf("Error closing db file.\n");
    exit(EXIT_FAILURE);
  }
  for (uint32_t i = 0; i < TABLE_MAX_PAGES; i++)
  {
    void *page = pager->pages[i];
    if (page)
    {
      free(page);
      pager->pages[i] = NULL;
    }
  }
  free(pager);
  free(table);
}
Cursor *table_start(Table *table)
{
  Cursor *cursor = table_find(table, 0);

  void *node = get_page(table->pager, cursor->page_num);
  uint32_t num_cells = *leaf_node_num_cells(node);
  cursor->end_of_table = (num_cells == 0);

  return cursor;
}

void *cursor_value(Cursor *cursor)
{
  uint32_t page_num = cursor->page_num;
  void *page = get_page(cursor->table->pager, page_num);
  return leaf_node_value(page, cursor->cell_num);
}

void cursor_advance(Cursor *cursor)
{
  uint32_t page_num = cursor->page_num;
  void *node = get_page(cursor->table->pager, page_num);
  cursor->cell_num += 1;
  if (cursor->cell_num >= *leaf_node_num_cells(node))
  {
    // Advance to next leaf node
    uint32_t next_page_num = *leaf_node_next_leaf(node);
    if (next_page_num == 0)
    {
      // this is rightmost leaf node
      cursor->end_of_table = true;
    }
    else
    {
      cursor->page_num = next_page_num;
      cursor->cell_num = 0;
    }
  }
}

void dynamic_row_init(DynamicRow* row, TableDef* table_def) {
    // Calculate the total size needed for the row
    uint32_t size = 0;

    #ifdef DEBUG
    printf("DEBUG: Initializing DynamicRow for table with %d columns\n", table_def->num_columns);
    #endif

    for (uint32_t i = 0; i < table_def->num_columns; i++) {
        ColumnDef* col = &table_def->columns[i];

        #ifdef DEBUG
        printf("DEBUG: Column %d (%s) of type %d with size %u\n", 
               i, col->name, col->type, col->size);
        #endif

        switch (col->type) {
            case COLUMN_TYPE_INT:
                size += sizeof(int32_t);
                break;
            case COLUMN_TYPE_FLOAT:
                size += sizeof(float);
                break;
            case COLUMN_TYPE_BOOLEAN:
                size += sizeof(uint8_t);
                break;
            case COLUMN_TYPE_DATE:
            case COLUMN_TYPE_TIME:
                size += sizeof(int32_t);
                break;
            case COLUMN_TYPE_TIMESTAMP:
                size += sizeof(int64_t);
                break;
            case COLUMN_TYPE_STRING:
                // Add 1 for null terminator
                size += col->size + 1;
                break;
            case COLUMN_TYPE_BLOB:
                // Size prefix + data
                size += col->size + sizeof(uint32_t);
                break;
        }
    }

    #ifdef DEBUG
    printf("DEBUG: Allocating %u bytes for row\n", size);
    #endif

    row->data = malloc(size);
    if (row->data == NULL) {
        fprintf(stderr, "ERROR: Failed to allocate %u bytes for row\n", size);
        exit(EXIT_FAILURE);
    }

    row->data_size = size;
    memset(row->data, 0, size); // Initialize to zeros
}

void dynamic_row_set_string(DynamicRow* row, TableDef* table_def, uint32_t col_idx, const char* value) {
    if (col_idx >= table_def->num_columns) {
        fprintf(stderr, "ERROR: Column index %u out of bounds (max %u)\n", 
               col_idx, 
               table_def->num_columns > 0 ? table_def->num_columns - 1 : 0);
        return;
    }

    ColumnDef* col = &table_def->columns[col_idx];
    if (col->type != COLUMN_TYPE_STRING) {
        fprintf(stderr, "ERROR: Column %s is not a string type (actual type: %d)\n", 
               col->name, col->type);
        return;
    }

    uint32_t offset = get_column_offset(table_def, col_idx);
    uint32_t max_str_size = col->size;

    // Check for null value
    if (!value) {
        // Handle NULL strings by setting first byte to 0
        ((char*)row->data)[offset] = '\0';
        #ifdef DEBUG
        printf("DEBUG: Storing NULL string at column %s (idx=%d, offset=%d)\n", 
               col->name, col_idx, offset);
        #endif
        return;
    }

    // Calculate safe copy length (leave room for null terminator)
    size_t value_len = strlen(value);
    size_t copy_len = (value_len < max_str_size) ? value_len : max_str_size - 1;

    // Make sure we don't exceed the buffer size
    if (offset + copy_len >= row->data_size) {
        fprintf(stderr, "ERROR: String would overflow row buffer: offset=%u, copy_len=%zu, buffer_size=%u\n",
               offset, copy_len, row->data_size);
        copy_len = row->data_size > offset + 1 ? row->data_size - offset - 1 : 0;
    }

    // Copy string data and ensure null termination
    if (copy_len > 0) {
        memcpy((char*)row->data + offset, value, copy_len);
    }
    ((char*)row->data)[offset + copy_len] = '\0';

    #ifdef DEBUG
    printf("DEBUG: Stored string '%s' (len=%zu, copied=%zu) at column %s (idx=%d, offset=%d, max_size=%d)\n", 
           value, value_len, copy_len, col->name, col_idx, offset, max_str_size);
    #endif
}

// Helper function to calculate column offset
uint32_t get_column_offset(TableDef* table_def, uint32_t col_idx) {
    if (col_idx >= table_def->num_columns) {
        fprintf(stderr, "ERROR: Column index %u out of bounds (max %u)\n", 
                col_idx, table_def->num_columns > 0 ? table_def->num_columns - 1 : 0);
        return 0;
    }

    uint32_t offset = 0;
    for (uint32_t i = 0; i < col_idx; i++) {
        ColumnDef* col = &table_def->columns[i];

        switch (col->type) {
            case COLUMN_TYPE_INT:
                offset += sizeof(int32_t);
                break;
            case COLUMN_TYPE_FLOAT:
                offset += sizeof(float);
                break;
            case COLUMN_TYPE_BOOLEAN:
                offset += sizeof(uint8_t);
                break;
            case COLUMN_TYPE_DATE:
            case COLUMN_TYPE_TIME:
                offset += sizeof(int32_t);
                break;
            case COLUMN_TYPE_TIMESTAMP:
                offset += sizeof(int64_t);
                break;
            case COLUMN_TYPE_STRING:
                offset += col->size + 1;  // Include null terminator
                break;
            case COLUMN_TYPE_BLOB:
                offset += col->size + sizeof(uint32_t);  // Size prefix + data
                break;
        }
    }

    #ifdef DEBUG
    printf("DEBUG: Offset for column %u (%s) is %u bytes\n", 
           col_idx, table_def->columns[col_idx].name, offset);
    #endif

    return offset;
}

void dynamic_row_set_int(DynamicRow* row, TableDef* table_def, uint32_t col_idx, int32_t value) {
    if (col_idx >= table_def->num_columns || table_def->columns[col_idx].type != COLUMN_TYPE_INT) {
        fprintf(stderr, "ERROR: Cannot set int value for column %u\n", col_idx);
        return;
    }

    uint32_t offset = get_column_offset(table_def, col_idx);
    memcpy((uint8_t*)row->data + offset, &value, sizeof(int32_t));

    #ifdef DEBUG
    printf("DEBUG: Set INT value %d at offset %u\n", value, offset);
    #endif
}

void dynamic_row_set_float(DynamicRow* row, TableDef* table_def, uint32_t col_idx, float value) {
    if (col_idx >= table_def->num_columns || table_def->columns[col_idx].type != COLUMN_TYPE_FLOAT) {
        fprintf(stderr, "ERROR: Cannot set float value for column %u\n", col_idx);
        return;
    }

    uint32_t offset = get_column_offset(table_def, col_idx);
    memcpy((uint8_t*)row->data + offset, &value, sizeof(float));

    #ifdef DEBUG
    printf("DEBUG: Set FLOAT value %f at offset %u\n", value, offset);
    #endif
}

void dynamic_row_set_boolean(DynamicRow* row, TableDef* table_def, uint32_t col_idx, bool value) {
    if (col_idx >= table_def->num_columns || table_def->columns[col_idx].type != COLUMN_TYPE_BOOLEAN) {
        fprintf(stderr, "ERROR: Cannot set boolean value for column %u\n", col_idx);
        return;
    }

    uint32_t offset = get_column_offset(table_def, col_idx);
    uint8_t bool_val = value ? 1 : 0;
    memcpy((uint8_t*)row->data + offset, &bool_val, sizeof(uint8_t));

    #ifdef DEBUG
    printf("DEBUG: Set BOOLEAN value %d at offset %u\n", bool_val, offset);
    #endif
}

void dynamic_row_set_date(DynamicRow* row, TableDef* table_def, uint32_t col_idx, int32_t value) {
    if (col_idx >= table_def->num_columns || table_def->columns[col_idx].type != COLUMN_TYPE_DATE) {
        fprintf(stderr, "ERROR: Cannot set date value for column %u\n", col_idx);
        return;
    }

    uint32_t offset = get_column_offset(table_def, col_idx);
    memcpy((uint8_t*)row->data + offset, &value, sizeof(int32_t));

    #ifdef DEBUG
    printf("DEBUG: Set DATE value %d at offset %u\n", value, offset);
    #endif
}

void dynamic_row_set_time(DynamicRow* row, TableDef* table_def, uint32_t col_idx, int32_t value) {
    if (col_idx >= table_def->num_columns || table_def->columns[col_idx].type != COLUMN_TYPE_TIME) {
        fprintf(stderr, "ERROR: Cannot set time value for column %u\n", col_idx);
        return;
    }

    uint32_t offset = get_column_offset(table_def, col_idx);
    memcpy((uint8_t*)row->data + offset, &value, sizeof(int32_t));

    #ifdef DEBUG
    printf("DEBUG: Set TIME value %d at offset %u\n", value, offset);
    #endif
}

void dynamic_row_set_timestamp(DynamicRow* row, TableDef* table_def, uint32_t col_idx, int64_t value) {
    if (col_idx >= table_def->num_columns || table_def->columns[col_idx].type != COLUMN_TYPE_TIMESTAMP) {
        fprintf(stderr, "ERROR: Cannot set timestamp value for column %u\n", col_idx);
        return;
    }

    uint32_t offset = get_column_offset(table_def, col_idx);
    memcpy((uint8_t*)row->data + offset, &value, sizeof(int64_t));

    #ifdef DEBUG
    printf("DEBUG: Set TIMESTAMP value %lld at offset %u\n", (long long)value, offset);
    #endif
}

char* dynamic_row_get_string(DynamicRow* row, TableDef* table_def, uint32_t col_idx) {
    if (col_idx >= table_def->num_columns) {
        fprintf(stderr, "ERROR: Invalid get_string call: idx=%d, num_cols=%d\n", 
               col_idx, table_def->num_columns);
        return NULL;
    }

    ColumnDef* col = &table_def->columns[col_idx];
    if (col->type != COLUMN_TYPE_STRING) {
        fprintf(stderr, "ERROR: Column %s is not a string type\n", col->name);
        return NULL;
    }

    uint32_t offset = get_column_offset(table_def, col_idx);

    // Safety check to make sure offset is within buffer
    if (offset >= row->data_size) {
        fprintf(stderr, "ERROR: String offset %u is beyond buffer size %u\n", offset, row->data_size);
        return NULL;
    }

    char* result = (char*)row->data + offset;

    // Verify string is null-terminated within the buffer
    bool found_null = false;
    for (uint32_t i = 0; i < col->size && offset + i < row->data_size; i++) {
        if (result[i] == '\0') {
            found_null = true;
            break;
        }
    }

    if (!found_null) {
        // If there's no null terminator, force one at the end of the allocated space
        uint32_t max_pos = (offset + col->size - 1 < row->data_size) ? 
                          col->size - 1 : row->data_size - offset - 1;
        result[max_pos] = '\0';
        printf("WARNING: Fixed missing null terminator in column %s\n", col->name);
    }
    
    #ifdef DEBUG
    printf("DEBUG: Retrieved string '%s' from column %s (idx=%d, offset=%d)\n", 
           result, col->name, col_idx, offset);
    #endif
    
    return result;
}

void dynamic_row_set_blob(DynamicRow* row, TableDef* table_def, uint32_t col_idx, const void* data, uint32_t size) {
    if (col_idx >= table_def->num_columns || table_def->columns[col_idx].type != COLUMN_TYPE_BLOB) {
        fprintf(stderr, "ERROR: Cannot set blob data for column %u\n", col_idx);
        return;
    }
    
    uint32_t offset = get_column_offset(table_def, col_idx);
    uint32_t max_size = table_def->columns[col_idx].size;
    
    // First store the actual size being used
    uint32_t actual_size = size > max_size ? max_size : size;
    memcpy((uint8_t*)row->data + offset, &actual_size, sizeof(uint32_t));
    
    // Then store the blob data
    memcpy((uint8_t*)row->data + offset + sizeof(uint32_t), data, actual_size);
    
    #ifdef DEBUG
    printf("DEBUG: Stored BLOB data (%u bytes) at offset %u\n", actual_size, offset);
    #endif
    
}

int32_t dynamic_row_get_int(DynamicRow* row, TableDef* table_def, uint32_t col_idx) {
    if (col_idx >= table_def->num_columns || table_def->columns[col_idx].type != COLUMN_TYPE_INT) {
        fprintf(stderr, "ERROR: Cannot get int value from column %u\n", col_idx);
        return 0;
    }
    
    uint32_t offset = get_column_offset(table_def, col_idx);
    int32_t value;
    memcpy(&value, (uint8_t*)row->data + offset, sizeof(int32_t));
    return value;
}

float dynamic_row_get_float(DynamicRow* row, TableDef* table_def, uint32_t col_idx) {
    if (col_idx >= table_def->num_columns || table_def->columns[col_idx].type != COLUMN_TYPE_FLOAT) {
        fprintf(stderr, "ERROR: Cannot get float value from column %u\n", col_idx);
        return 0.0f;
    }
    
    uint32_t offset = get_column_offset(table_def, col_idx);
    float value;
    memcpy(&value, (uint8_t*)row->data + offset, sizeof(float));
    return value;
}

bool dynamic_row_get_boolean(DynamicRow* row, TableDef* table_def, uint32_t col_idx) {
    if (col_idx >= table_def->num_columns || table_def->columns[col_idx].type != COLUMN_TYPE_BOOLEAN) {
        fprintf(stderr, "ERROR: Cannot get boolean value from column %u\n", col_idx);
        return false;
    }
    
    uint32_t offset = get_column_offset(table_def, col_idx);
    uint8_t value;
    memcpy(&value, (uint8_t*)row->data + offset, sizeof(uint8_t));
    return value != 0;
}

int32_t dynamic_row_get_date(DynamicRow* row, TableDef* table_def, uint32_t col_idx) {
    if (col_idx >= table_def->num_columns || table_def->columns[col_idx].type != COLUMN_TYPE_DATE) {
        fprintf(stderr, "ERROR: Cannot get date value from column %u\n", col_idx);
        return 0;
    }
    
    uint32_t offset = get_column_offset(table_def, col_idx);
    int32_t value;
    memcpy(&value, (uint8_t*)row->data + offset, sizeof(int32_t));
    return value;
}

int32_t dynamic_row_get_time(DynamicRow* row, TableDef* table_def, uint32_t col_idx) {
    if (col_idx >= table_def->num_columns || table_def->columns[col_idx].type != COLUMN_TYPE_TIME) {
        fprintf(stderr, "ERROR: Cannot get time value from column %u\n", col_idx);
        return 0;
    }
    
    uint32_t offset = get_column_offset(table_def, col_idx);
    int32_t value;
    memcpy(&value, (uint8_t*)row->data + offset, sizeof(int32_t));
    return value;
}

int64_t dynamic_row_get_timestamp(DynamicRow* row, TableDef* table_def, uint32_t col_idx) {
    if (col_idx >= table_def->num_columns || table_def->columns[col_idx].type != COLUMN_TYPE_TIMESTAMP) {
        fprintf(stderr, "ERROR: Cannot get timestamp value from column %u\n", col_idx);
        return 0;
    }
    
    uint32_t offset = get_column_offset(table_def, col_idx);
    int64_t value;
    memcpy(&value, (uint8_t*)row->data + offset, sizeof(int64_t));
    return value;
}

void* dynamic_row_get_blob(DynamicRow* row, TableDef* table_def, uint32_t col_idx, uint32_t* size) {
    if (col_idx >= table_def->num_columns || table_def->columns[col_idx].type != COLUMN_TYPE_BLOB) {
        if (size) *size = 0;
        fprintf(stderr, "ERROR: Cannot get blob data from column %u\n", col_idx);
        return NULL;
    }
    
    uint32_t offset = get_column_offset(table_def, col_idx);
    
    // Get the actual size
    uint32_t blob_size;
    memcpy(&blob_size, (uint8_t*)row->data + offset, sizeof(uint32_t));
    
    if (size) *size = blob_size;
    
    // Return pointer to the blob data
    return (uint8_t*)row->data + offset + sizeof(uint32_t);
}

void dynamic_row_free(DynamicRow* row) {
    if (row && row->data) {
        free(row->data);
        row->data = NULL;
        row->data_size = 0;
    }
}

void serialize_dynamic_row(DynamicRow* source, TableDef* table_def, void* destination) {
    // Add table_def parameter usage to avoid warning
    if (!source || !destination || !table_def) {
        return;
    }
    memcpy(destination, source->data, source->data_size);
    
    // Debug: Print the first few bytes after serialization
    #ifdef DEBUG
    printf("DEBUG: First 10 bytes after serialization: ");
    for (uint32_t i = 0; i < 10 && i < source->data_size; i++) {
        printf("%02x ", ((unsigned char*)destination)[i]);
    }
    printf("\n");
    #endif
}

void deserialize_dynamic_row(void *source, TableDef *table_def, DynamicRow *destination) {
    // Calculate the size needed for the row
    uint32_t size = 0;
    for (uint32_t i = 0; i < table_def->num_columns; i++) {
        ColumnDef *col = &table_def->columns[i];
        
        switch (col->type) {
            case COLUMN_TYPE_INT:
                size += sizeof(int32_t);
                break;
            case COLUMN_TYPE_FLOAT:
                size += sizeof(float);
                break;
            case COLUMN_TYPE_BOOLEAN:
                size += sizeof(uint8_t);
                break;
            case COLUMN_TYPE_DATE:
            case COLUMN_TYPE_TIME:
                size += sizeof(int32_t);
                break;
            case COLUMN_TYPE_TIMESTAMP:
                size += sizeof(int64_t);
                break;
            case COLUMN_TYPE_STRING:
                size += col->size + 1;  // Include null terminator
                break;
            case COLUMN_TYPE_BLOB:
                size += col->size + sizeof(uint32_t);  // Size prefix + data
                break;
        }
    }
    
    // Allocate memory for the row
    destination->data = malloc(size);
    if (!destination->data) {
        fprintf(stderr, "ERROR: Failed to allocate %u bytes for row during deserialization\n", size);
        exit(EXIT_FAILURE);
    }
    
    destination->data_size = size;
    
    // Copy data from source to destination
    memcpy(destination->data, source, size);
}


void print_dynamic_row(DynamicRow* row, TableDef* table_def) {
    printf("(");
    
    for (uint32_t i = 0; i < table_def->num_columns; i++) {
        ColumnDef* col = &table_def->columns[i];
        
        if (i > 0) {
            printf(", ");
        }
        
        switch (col->type) {
            case COLUMN_TYPE_INT:
                printf("%d", dynamic_row_get_int(row, table_def, i));
                break;
            case COLUMN_TYPE_FLOAT:
                printf("%.2f", dynamic_row_get_float(row, table_def, i));
                break;
            case COLUMN_TYPE_BOOLEAN:
                printf("%s", dynamic_row_get_boolean(row, table_def, i) ? "TRUE" : "FALSE");
                break;
            case COLUMN_TYPE_DATE:
                printf("DATE(%d)", dynamic_row_get_date(row, table_def, i));
                break;
            case COLUMN_TYPE_TIME:
                printf("TIME(%d)", dynamic_row_get_time(row, table_def, i));
                break;
            case COLUMN_TYPE_TIMESTAMP:
                printf("TIMESTAMP(%lld)", (long long)dynamic_row_get_timestamp(row, table_def, i));
                break;
            case COLUMN_TYPE_STRING: {
                char* str = dynamic_row_get_string(row, table_def, i);
                if (str) {
                    printf("%s", str);
                } else {
                    printf("(null)");
                }
                break;
            }
            case COLUMN_TYPE_BLOB: {
                uint32_t size;
                void* blob = dynamic_row_get_blob(row, table_def, i, &size);
                (void)blob; // Avoid unused variable warning
                printf("<BLOB(%u bytes)>", size);
                break;
            }
        }
    }
    
    printf(")\n");
}
static void append_to_buffer(char *buf, size_t bufsize, const char *fmt, ...) {
    va_list args;
    size_t len = strlen(buf);
    va_start(args, fmt);
    vsnprintf(buf + len, bufsize - len, fmt, args);
    va_end(args);
}
void print_dynamic_column(DynamicRow* row, TableDef* table_def, uint32_t col_idx, char *buf, size_t buf_size) {
    if (col_idx >= table_def->num_columns) {
        printf("ERROR");
        return;
    }
    
    ColumnDef* col = &table_def->columns[col_idx];
    
    switch (col->type) {
        case COLUMN_TYPE_INT:
            append_to_buffer(buf, buf_size, "%d", dynamic_row_get_int(row, table_def, col_idx));
            break;
        case COLUMN_TYPE_STRING:
            append_to_buffer(buf, buf_size, "%s", dynamic_row_get_string(row, table_def, col_idx));
            break;
        case COLUMN_TYPE_FLOAT:
            append_to_buffer(buf, buf_size, "%.2f", dynamic_row_get_float(row, table_def, col_idx));
            break;
        case COLUMN_TYPE_BOOLEAN:
            append_to_buffer(buf, buf_size, "%s", dynamic_row_get_boolean(row, table_def, col_idx) ? "true" : "false");
            break;
        case COLUMN_TYPE_DATE:
            append_to_buffer(buf, buf_size, "%d", dynamic_row_get_date(row, table_def, col_idx));
            break;
        case COLUMN_TYPE_TIME:
            append_to_buffer(buf, buf_size, "%d", dynamic_row_get_time(row, table_def, col_idx));
            break;
        case COLUMN_TYPE_TIMESTAMP:
            append_to_buffer(buf, buf_size, "%ld", dynamic_row_get_timestamp(row, table_def, col_idx));
            break;
        case COLUMN_TYPE_BLOB:
            printf("[BLOB]");
            break;
        default:
            printf("?");
            break;
    }
}
