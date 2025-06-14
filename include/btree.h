#ifndef BTREE_H
#define BTREE_H

#include "cursor.h"
#include "table.h"
#include <stdbool.h>
#include <stdint.h>

// Node Header Layout
#define NODE_TYPE_SIZE sizeof(uint8_t)
#define NODE_TYPE_OFFSET 0
#define IS_ROOT_SIZE sizeof(uint8_t)
#define IS_ROOT_OFFSET NODE_TYPE_SIZE
#define PARENT_POINTER_SIZE sizeof(uint32_t)
#define PARENT_POINTER_OFFSET (IS_ROOT_OFFSET + IS_ROOT_SIZE)
#define COMMON_NODE_HEADER_SIZE \
  (NODE_TYPE_SIZE + IS_ROOT_SIZE + PARENT_POINTER_SIZE)

/***  Leaf Node Header Layout start ***/
#define LEAF_NODE_NUM_CELLS_SIZE sizeof(uint32_t)
#define LEAF_NODE_NUM_CELLS_OFFSET COMMON_NODE_HEADER_SIZE
// siblings start
#define LEAF_NODE_NEXT_LEAF_SIZE sizeof(uint32_t)
#define LEAF_NODE_NEXT_LEAF_OFFSET \
  (LEAF_NODE_NUM_CELLS_OFFSET + LEAF_NODE_NUM_CELLS_SIZE)
// siblings end
#define LEAF_NODE_HEADER_SIZE                           \
  (COMMON_NODE_HEADER_SIZE + LEAF_NODE_NUM_CELLS_SIZE + \
   LEAF_NODE_NEXT_LEAF_SIZE)
/***  Leaf Node Header Layout end ***/

/***  Leaf Node body Layout start ***/
#define LEAF_NODE_KEY_SIZE sizeof(uint32_t)
#define LEAF_NODE_KEY_OFFSET 0
// Use dynamic row sizing instead of fixed Row struct
// This is the max size - individual rows may be smaller
#define MAX_ROW_SIZE 4096  
#define LEAF_NODE_VALUE_SIZE_OFFSET (LEAF_NODE_KEY_OFFSET + LEAF_NODE_KEY_SIZE)
#define LEAF_NODE_VALUE_SIZE_SIZE sizeof(uint32_t)
#define LEAF_NODE_VALUE_OFFSET (LEAF_NODE_VALUE_SIZE_OFFSET + LEAF_NODE_VALUE_SIZE_SIZE)
#define LEAF_NODE_CELL_HEADER_SIZE (LEAF_NODE_KEY_SIZE + LEAF_NODE_VALUE_SIZE_SIZE)
// Cell size is now variable, this is the minimum size with empty value
#define LEAF_NODE_MIN_CELL_SIZE (LEAF_NODE_CELL_HEADER_SIZE)
#define LEAF_NODE_SPACE_FOR_CELLS (PAGE_SIZE - LEAF_NODE_HEADER_SIZE)
// Adjust max cells based on average expected row size
#define ESTIMATED_AVG_ROW_SIZE 256
#define LEAF_NODE_MAX_CELLS (LEAF_NODE_SPACE_FOR_CELLS / (LEAF_NODE_CELL_HEADER_SIZE + ESTIMATED_AVG_ROW_SIZE))
#define LEAF_NODE_RIGHT_SPLIT_COUNT ((LEAF_NODE_MAX_CELLS + 1) / 2)
#define LEAF_NODE_LEFT_SPLIT_COUNT \
  (LEAF_NODE_MAX_CELLS + 1 - LEAF_NODE_RIGHT_SPLIT_COUNT)
/***  Leaf Node body Layout end ***/

// Internal Node Header Layout
#define INTERNAL_NODE_NUM_KEYS_SIZE sizeof(uint32_t)
#define INTERNAL_NODE_NUM_KEYS_OFFSET (COMMON_NODE_HEADER_SIZE)
#define INTERNAL_NODE_RIGHT_CHILD_SIZE sizeof(uint32_t)
#define INTERNAL_NODE_RIGHT_CHILD_OFFSET \
  (INTERNAL_NODE_NUM_KEYS_OFFSET + INTERNAL_NODE_NUM_KEYS_SIZE)
#define INTERNAL_NODE_HEADER_SIZE                          \
  (COMMON_NODE_HEADER_SIZE + INTERNAL_NODE_NUM_KEYS_SIZE + \
   INTERNAL_NODE_RIGHT_CHILD_SIZE)

// Internal Node Body Layout
#define INTERNAL_NODE_KEY_SIZE sizeof(uint32_t)
#define INTERNAL_NODE_CHILD_SIZE sizeof(uint32_t)
#define INTERNAL_NODE_CELL_SIZE \
  (INTERNAL_NODE_CHILD_SIZE + INTERNAL_NODE_KEY_SIZE)
#define INVALID_PAGE_NUM UINT32_MAX
extern const uint32_t INTERNAL_NODE_MAX_CELLS; /* Keep this small for testing */

// enums
typedef enum
{
  NODE_INTERNAL,
  NODE_LEAF
} NodeType;
// Function declarations
uint32_t *node_parent(void *node);
void update_internal_node_key(void *node, uint32_t old_key, uint32_t new_key);
void internal_node_insert(Table *table, uint32_t parent_page_num, uint32_t child_page_num);
uint32_t internal_node_find_child(void *node, uint32_t key);

/*** Leaf Node start ***/
void initialize_leaf_node(void *node);
uint32_t *leaf_node_num_cells(void *node);
void *leaf_node_cell(void *node, uint32_t cell_num);
uint32_t *leaf_node_key(void *node, uint32_t cell_num);
void *leaf_node_value(void *node, uint32_t cell_num);
uint32_t *leaf_node_value_size(void *node, uint32_t cell_num);
uint32_t leaf_node_cell_size(void *node, uint32_t cell_num);
void *leaf_node_next_cell(void *node, uint32_t cell_num);
void leaf_node_insert(Cursor *cursor, uint32_t key, DynamicRow *row, TableDef *table_def);
Cursor *table_find(Table *table, uint32_t key);
Cursor *leaf_node_find(Table *table, uint32_t page_num, uint32_t key);
void leaf_node_split_and_insert(Cursor *cursor, uint32_t key, DynamicRow *row, TableDef *table_def);
uint32_t *leaf_node_next_leaf(void *node);
/*** Leaf Node end ***/

NodeType get_node_type(void *node);
void set_node_type(void *node, NodeType type);
uint32_t get_unused_page_num(Pager *pager);
uint32_t get_node_max_key(Pager *pager, void *node);

/*** Internal Node start ***/
uint32_t *internal_node_num_keys(void *node);
uint32_t *internal_node_right_child(void *node);
void *internal_node_cell(void *node, uint32_t cell_num);
uint32_t *internal_node_child(void *node, uint32_t child_num);
uint32_t *internal_node_key(void *node, uint32_t key_num);
void initialize_internal_node(void *node);
Cursor *internal_node_find(Table *table, uint32_t page_num, uint32_t key);
/*** Internal Node end ***/

/*** Root Node start ***/
bool is_node_root(void *node);
void set_node_root(void *node, bool is_root);
void create_root_node(Table *table, uint32_t right_child_page_num);
/*** Root Node end ***/

// Add print_tree function declaration so it's visible to command_processor.c
void print_tree(Pager *pager, uint32_t page_num, uint32_t indentation_level);
void indent(uint32_t level);

#endif
