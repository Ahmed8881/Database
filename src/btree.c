#include "../include/btree.h"
#include "../include/stack.h"

#include <stdint.h>
#include <string.h>


/*** Leaf Node start ***/
uint32_t *leaf_node_num_cells(void *node) {
  return (uint32_t *)((uint8_t *)node + LEAF_NODE_NUM_CELLS_OFFSET);
}

void *leaf_node_cell(void *node, uint32_t cell_num) {
  return (uint8_t *)node + LEAF_NODE_HEADER_SIZE +
         cell_num * LEAF_NODE_CELL_SIZE;
}

uint32_t *leaf_node_key(void *node, uint32_t cell_num) {
  return (uint32_t *)leaf_node_cell(node, cell_num);
}

void *leaf_node_value(void *node, uint32_t cell_num) {
  return (uint8_t *)leaf_node_cell(node, cell_num) + LEAF_NODE_KEY_SIZE;
}

void initialize_leaf_node(void *node) {
  set_node_type(node, NODE_LEAF);
  set_node_root(node, false);
  *leaf_node_num_cells(node) = 0;
  *leaf_node_next_leaf(node) = 0; // 0 means no sibling
}

void leaf_node_insert(Cursor *cursor, uint32_t key, Row *value) {
  void *node = get_page(cursor->table->pager, cursor->page_num);
  uint32_t num_cells = *leaf_node_num_cells(node);

  if (num_cells >= LEAF_NODE_MAX_CELLS) {
    // Node full
    leaf_node_split_and_insert(cursor, key, value);
    return;
  }

  if (cursor->cell_num < num_cells) {
    // Make room for new cell
    for (uint32_t i = num_cells; i > cursor->cell_num; i--) {
      memcpy(leaf_node_cell(node, i), leaf_node_cell(node, i - 1),
             LEAF_NODE_CELL_SIZE);
    }
  }

  *(leaf_node_num_cells(node)) += 1;
  *(leaf_node_key(node, cursor->cell_num)) = key;
  serialize_row(value, leaf_node_value(node, cursor->cell_num));
}
Cursor *table_find(Table *table, uint32_t key) {
  uint32_t root_page_num = table->root_page_num;
  void *root_node = get_page(table->pager, root_page_num);
  if (get_node_type(root_node) == NODE_LEAF) {
    return leaf_node_find(table, root_page_num, key);
  } else {
    return internal_node_find(table, root_page_num, key);
  }
}
Cursor *leaf_node_find(Table *table, uint32_t page_num, uint32_t key) {
  void *node = get_page(table->pager, page_num);
  uint32_t num_cells = *leaf_node_num_cells(node);
  Cursor *cursor = malloc(sizeof(Cursor));
  cursor->table = table;
  cursor->page_num = page_num;
  // Binary search
  uint32_t min_index = 0;
  uint32_t one_past_max_index = num_cells;
  while (one_past_max_index != min_index) {
    uint32_t index = (min_index + one_past_max_index) / 2;
    uint32_t key_at_index = *leaf_node_key(node, index);
    if (key == key_at_index) {
      cursor->cell_num = index;
      return cursor;
    }
    if (key < key_at_index) {
      one_past_max_index = index;
    } else {
      min_index = index + 1;
    }
  }
  cursor->cell_num = min_index;
  return cursor;
}
NodeType get_node_type(void *node) {
  uint8_t value = *((uint8_t *)node + NODE_TYPE_OFFSET);
  return (NodeType)value;
}
void set_node_type(void *node, NodeType type) {
  uint8_t value = type;
  *((uint8_t *)node + NODE_TYPE_OFFSET) = value;
}

void leaf_node_split_and_insert(Cursor* cursor, uint32_t key, Row* value) {
  void* old_node = get_page(cursor->table->pager, cursor->page_num);
  uint32_t old_max = get_node_max_key(old_node);
  uint32_t new_page_num = get_unused_page_num(cursor->table->pager);
  void* new_node = get_page(cursor->table->pager, new_page_num);
  initialize_leaf_node(new_node);
  *node_parent(new_node) = *node_parent(old_node);
  *leaf_node_next_leaf(new_node) = *leaf_node_next_leaf(old_node);
  *leaf_node_next_leaf(old_node) = new_page_num;

  // All existing keys plus new key
  uint32_t temp_keys[LEAF_NODE_MAX_CELLS + 1];
  Row temp_values[LEAF_NODE_MAX_CELLS + 1];

  // Copy all existing keys and values plus the new key and value into temp arrays
  for (uint32_t i = 0; i < LEAF_NODE_MAX_CELLS + 1; i++) {
    if (i == cursor->cell_num) {
      temp_keys[i] = key;
      temp_values[i] = *value;
    } else if (i < cursor->cell_num) {
      temp_keys[i] = *leaf_node_key(old_node, i);
      deserialize_row(leaf_node_value(old_node, i), &temp_values[i]);
    } else {
      temp_keys[i] = *leaf_node_key(old_node, i - 1);
      deserialize_row(leaf_node_value(old_node, i - 1), &temp_values[i]);
    }
  }

  // Split the keys and values between the old (left) and new (right) nodes
  for (uint32_t i = 0; i < LEAF_NODE_LEFT_SPLIT_COUNT; i++) {
    *leaf_node_key(old_node, i) = temp_keys[i];
    serialize_row(&temp_values[i], leaf_node_value(old_node, i));
  }
  for (uint32_t i = 0; i < LEAF_NODE_RIGHT_SPLIT_COUNT; i++) {
    *leaf_node_key(new_node, i) = temp_keys[LEAF_NODE_LEFT_SPLIT_COUNT + i];
    serialize_row(&temp_values[LEAF_NODE_LEFT_SPLIT_COUNT + i], leaf_node_value(new_node, i));
  }

  *leaf_node_num_cells(old_node) = LEAF_NODE_LEFT_SPLIT_COUNT;
  *leaf_node_num_cells(new_node) = LEAF_NODE_RIGHT_SPLIT_COUNT;

  if (is_node_root(old_node)) {
    return create_root_node(cursor->table, new_page_num);
  } else {
    uint32_t parent_page_num = *node_parent(old_node);
    uint32_t new_max = get_node_max_key(old_node);
    void* parent = get_page(cursor->table->pager, parent_page_num);

    update_internal_node_key(parent, old_max, new_max);
    internal_node_insert(cursor->table, parent_page_num, new_page_num);
    return;
  }
}
void internal_node_insert(Table* table, uint32_t parent_page_num, uint32_t child_page_num) {
  /*
  Add a new child/key pair to parent that corresponds to child
  */
  void* parent = get_page(table->pager, parent_page_num);
  void* child = get_page(table->pager, child_page_num);
  uint32_t child_max_key = get_node_max_key(child);
  uint32_t index = internal_node_find_child(parent, child_max_key);

  uint32_t original_num_keys = *internal_node_num_keys(parent);

  if (original_num_keys >= INTERNAL_NODE_MAX_CELLS) {
    internal_node_split_and_insert(table, parent_page_num, child_page_num);
    return;
  }

  uint32_t right_child_page_num = *internal_node_right_child(parent);
  /*
  An internal node with a right child of INVALID_PAGE_NUM is empty
  */
  if (right_child_page_num == INVALID_PAGE_NUM) {
    *internal_node_right_child(parent) = child_page_num;
    return;
  }

  void* right_child = get_page(table->pager, right_child_page_num);
  /*
  If we are already at the max number of cells for a node, we cannot increment
  before splitting. Incrementing without inserting a new key/child pair
  and immediately calling internal_node_split_and_insert has the effect
  of creating a new key at (max_cells + 1) with an uninitialized value
  */
  *internal_node_num_keys(parent) = original_num_keys + 1;

  if (child_max_key > get_node_max_key(right_child)) {
    /* Replace right child */
    *internal_node_child(parent, original_num_keys) = right_child_page_num;
    *internal_node_key(parent, original_num_keys) = get_node_max_key(right_child);
    *internal_node_right_child(parent) = child_page_num;
  } else {
    /* Make room for the new cell */
    for (uint32_t i = original_num_keys; i > index; i--) {
      void* destination = internal_node_cell(parent, i);
      void* source = internal_node_cell(parent, i - 1);
      memcpy(destination, source, INTERNAL_NODE_CELL_SIZE);
    }
    *internal_node_child(parent, index) = child_page_num;
    *internal_node_key(parent, index) = child_max_key;
  }
}

void internal_node_split_and_insert(Table* table, uint32_t parent_page_num, uint32_t child_page_num) {
  uint32_t old_page_num = parent_page_num;
  void* old_node = get_page(table->pager, parent_page_num);
  uint32_t old_max = get_node_max_key(table->pager, old_node);

  void* child = get_page(table->pager, child_page_num); 
  uint32_t child_max = get_node_max_key(table->pager, child);

  uint32_t new_page_num = get_unused_page_num(table->pager);

  uint32_t splitting_root = is_node_root(old_node);

  void* parent;
  void* new_node;
  if (splitting_root) {
    create_new_root(table, new_page_num);
    parent = get_page(table->pager, table->root_page_num);
    old_page_num = *internal_node_child(parent, 0);
    old_node = get_page(table->pager, old_page_num);
  } else {
    parent = get_page(table->pager, *node_parent(old_node));
    new_node = get_page(table->pager, new_page_num);
    initialize_internal_node(new_node);
  }

  uint32_t* old_num_keys = internal_node_num_keys(old_node);

  uint32_t cur_page_num = *internal_node_right_child(old_node);
  void* cur = get_page(table->pager, cur_page_num);

  internal_node_insert(table, new_page_num, cur_page_num);
  *node_parent(cur) = new_page_num;
  *internal_node_right_child(old_node) = INVALID_PAGE_NUM;

  for (int i = INTERNAL_NODE_MAX_CELLS - 1; i > INTERNAL_NODE_MAX_CELLS / 2; i--) {
    cur_page_num = *internal_node_child(old_node, i);
    cur = get_page(table->pager, cur_page_num);

    internal_node_insert(table, new_page_num, cur_page_num);
    *node_parent(cur) = new_page_num;

    (*old_num_keys)--;
  }

  *internal_node_right_child(old_node) = *internal_node_child(old_node, *old_num_keys - 1);
  (*old_num_keys)--;

  uint32_t max_after_split = get_node_max_key(table->pager, old_node);

  uint32_t destination_page_num = child_max < max_after_split ? old_page_num : new_page_num;

  internal_node_insert(table, destination_page_num, child_page_num);
  *node_parent(child) = destination_page_num;

  update_internal_node_key(parent, old_max, get_node_max_key(table->pager, old_node));

  if (!splitting_root) {
    internal_node_insert(table, *node_parent(old_node), new_page_num);
    *node_parent(new_node) = *node_parent(old_node);
  }
}

void create_new_root(Table* table, uint32_t right_child_page_num) {
  void* root = get_page(table->pager, table->root_page_num);
  void* right_child = get_page(table->pager, right_child_page_num);
  uint32_t left_child_page_num = get_unused_page_num(table->pager);
  void* left_child = get_page(table->pager, left_child_page_num);

  if (get_node_type(root) == NODE_INTERNAL) {
    initialize_internal_node(right_child);
    initialize_internal_node(left_child);
  }

  memcpy(left_child, root, PAGE_SIZE);
  set_node_root(left_child, false);

  if (get_node_type(left_child) == NODE_INTERNAL) {
    void* child;
    for (int i = 0; i < *internal_node_num_keys(left_child); i++) {
      child = get_page(table->pager, *internal_node_child(left_child, i));
      *node_parent(child) = left_child_page_num;
    }
    child = get_page(table->pager, *internal_node_right_child(left_child));
    *node_parent(child) = left_child_page_num;
  }

  initialize_internal_node(root);
  set_node_root(root, true);
  *internal_node_num_keys(root) = 1;
  *internal_node_child(root, 0) = left_child_page_num;
  uint32_t left_child_max_key = get_node_max_key(table->pager, left_child);
  *internal_node_key(root, 0) = left_child_max_key;
  *internal_node_right_child(root) = right_child_page_num;
  *node_parent(left_child) = table->root_page_num;
  *node_parent(right_child) = table->root_page_num;
}

// void print_tree(Pager* pager, uint32_t page_num, uint32_t indentation_level) {
//   void* node = get_page(pager, page_num);
//   uint32_t num_keys, child;
//   switch (get_node_type(node)) {
//     case NODE_LEAF:
//       num_keys = *leaf_node_num_cells(node);
//       indent(indentation_level);
//       printf("- leaf (size %d)\n", num_keys);
//       for (uint32_t i = 0; i < num_keys; i++) {
//         indent(indentation_level + 1);
//         printf("- %d\n", *leaf_node_key(node, i));
//       }
//       break;
//     case NODE_INTERNAL:
//       num_keys = *internal_node_num_keys(node);
//       indent(indentation_level);
//       printf("- internal (size %d)\n", num_keys);
//       if (num_keys > 0) {
//         for (uint32_t i = 0; i < num_keys; i++) {
//           child = *internal_node_child(node, i);
//           print_tree(pager, child, indentation_level + 1);

//           indent(indentation_level + 1);
//           printf("- key %d\n", *internal_node_key(node, i));
//         }
//         child = *internal_node_right_child(node);
//         print_tree(pager, child, indentation_level + 1);
//       }
//       break;
//   }
// }

// Add this non-recursive print function using stack
void print_tree_iterative(Pager* pager, uint32_t root_page_num) {
    Stack stack;
    stack_init(&stack);
    
    void* root = get_page(pager, root_page_num);
    stack_push(&stack, root, root_page_num, 0);
    
    while (!stack_is_empty(&stack)) {
        StackNode* current = stack_pop(&stack);
        void* node = current->data;
        uint32_t level = current->level;
        
        switch (get_node_type(node)) {
            case NODE_LEAF:
                uint32_t num_cells = *leaf_node_num_cells(node);
                indent(level);
                printf("- leaf (size %d)\n", num_cells);
                for (uint32_t i = 0; i < num_cells; i++) {
                    indent(level + 1);
                    printf("- %d\n", *leaf_node_key(node, i));
                }
                break;
                
            case NODE_INTERNAL:
                uint32_t num_keys = *internal_node_num_keys(node);
                indent(level);
                printf("- internal (size %d)\n", num_keys);
                
                // Push right child first
                uint32_t right_child = *internal_node_right_child(node);
                if (right_child != INVALID_PAGE_NUM) {
                    void* right_node = get_page(pager, right_child);
                    stack_push(&stack, right_node, right_child, level + 1);
                }
                
                // Push other children from right to left
                for (int i = num_keys - 1; i >= 0; i--) {
                    uint32_t child_page_num = *internal_node_child(node, i);
                    void* child_node = get_page(pager, child_page_num);
                    stack_push(&stack, child_node, child_page_num, level + 1);
                }
                break;
        }
        free(current);
    }
    stack_destroy(&stack);
}

// Modify the original print_tree function to use iterative version
void print_tree(Pager* pager, uint32_t page_num, uint32_t indentation_level) {
    print_tree_iterative(pager, page_num);
}

void update_internal_node_key(void* node, uint32_t old_key, uint32_t new_key) {
  uint32_t old_child_index = internal_node_find_child(node, old_key);
  *internal_node_key(node, old_child_index) = new_key;
}
uint32_t internal_node_find_child(void* node, uint32_t key) {
  /*
  Return the index of the child which should contain
  the given key.
  */
  uint32_t num_keys = *internal_node_num_keys(node);

  /* Binary search */
  uint32_t min_index = 0;
  uint32_t max_index = num_keys; /* there is one more child than key */

  while (min_index != max_index) {
    uint32_t index = (min_index + max_index) / 2;
    uint32_t key_to_right = *internal_node_key(node, index);
    if (key_to_right >= key) {
      max_index = index;
    } else {
      min_index = index + 1;
    }
  }

  return min_index;
}
uint32_t *leaf_node_next_leaf(void *node) {
  return node + LEAF_NODE_NEXT_LEAF_OFFSET;
}

/*** Leaf Node end ***/
uint32_t* node_parent(void* node) {
  return (uint32_t*)((uint8_t*)node + PARENT_POINTER_OFFSET);
}

/*** Internal Node start ***/
uint32_t *internal_node_num_keys(void *node) {
  return node + INTERNAL_NODE_NUM_KEYS_OFFSET;
}

uint32_t *internal_node_right_child(void *node) {
  return node + INTERNAL_NODE_RIGHT_CHILD_OFFSET;
}

void *internal_node_cell(void *node, uint32_t cell_num) {
  return node + INTERNAL_NODE_HEADER_SIZE + cell_num * INTERNAL_NODE_CELL_SIZE;
}

uint32_t* internal_node_child(void* node, uint32_t child_num) {
  uint32_t num_keys = *internal_node_num_keys(node);
  if (child_num > num_keys) {
    printf("Tried to access child_num %d > num_keys %d\n", child_num, num_keys);
    exit(EXIT_FAILURE);
  } else if (child_num == num_keys) {
    uint32_t* right_child = internal_node_right_child(node);
    if (*right_child == INVALID_PAGE_NUM) {
      printf("Tried to access right child of node, but was invalid page\n");
      exit(EXIT_FAILURE);
    }
    return right_child;
  } else {
    uint32_t* child = internal_node_cell(node, child_num);
    if (*child == INVALID_PAGE_NUM) {
      printf("Tried to access child %d of node, but was invalid page\n", child_num);
      exit(EXIT_FAILURE);
    }
    return child;
  }
}

uint32_t* internal_node_key(void* node, uint32_t key_num) {
  return (void*)internal_node_cell(node, key_num) + INTERNAL_NODE_CHILD_SIZE;
}

Cursor* internal_node_find(Table* table, uint32_t page_num, uint32_t key) {
  void* node = get_page(table->pager, page_num);

  uint32_t child_index = internal_node_find_child(node, key);
  uint32_t child_num = *internal_node_child(node, child_index);
  void* child = get_page(table->pager, child_num);
  switch (get_node_type(child)) {
    case NODE_LEAF:
      return leaf_node_find(table, page_num, key);
    case NODE_INTERNAL:
      return internal_node_find(table, child_num, key);
  }
}
void initialize_internal_node(void* node) {
  set_node_type(node, NODE_INTERNAL);
  set_node_root(node, false);
  *internal_node_num_keys(node) = 0;
  *internal_node_right_child(node) = INVALID_PAGE_NUM;
}
/*** Internal Node end ***/

uint32_t get_unused_page_num(Pager *pager) { return pager->num_pages; }

uint32_t get_node_max_key(Pager* pager, void* node) {
  if (get_node_type(node) == NODE_LEAF) {
    return *leaf_node_key(node, *leaf_node_num_cells(node) - 1);
  }
  void* right_child = get_page(pager, *internal_node_right_child(node));
  return get_node_max_key(pager, right_child);
}

bool is_node_root(void *node) {
  uint8_t value = *((uint8_t *)node + IS_ROOT_OFFSET);
  return (bool)value;
}

void set_node_root(void *node, bool is_root) {
  uint8_t value = is_root;
  *((uint8_t *)node + IS_ROOT_OFFSET) = value;
}

void create_root_node(Table *table, uint32_t right_child_page_num) {
  void *root = get_page(table->pager, table->root_page_num);
  void *right_child = get_page(table->pager, right_child_page_num);
  uint32_t left_child_page_num = get_unused_page_num(table->pager);
  void *left_child = get_page(table->pager, left_child_page_num);

  memcpy(left_child, root, PAGE_SIZE);
  set_node_root(left_child, false);

  initialize_internal_node(root);
  set_node_root(root, true);
  *(internal_node_num_keys(root)) = 1;
  *(internal_node_child(root, 0)) = left_child_page_num;
  uint32_t left_child_max_key = get_node_max_key(left_child);
  *(internal_node_key(root, 0)) = left_child_max_key;
  *(internal_node_right_child(root)) = right_child_page_num;
}
