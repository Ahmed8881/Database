#include "../include/btree.h"
#include <stdint.h>
#include <string.h>

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
    printf("Need to implement searching an internal node\n");
    exit(EXIT_FAILURE);
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

void leaf_node_split_and_insert(Cursor *cursor, uint32_t key, Row *value) {
  void *old_node = get_page(cursor->table->pager, cursor->page_num);
  uint32_t new_page_num = get_unused_page_num(cursor->table->pager);
  void *new_node = get_page(cursor->table->pager, new_page_num);
  initialize_leaf_node(new_node);

  // All existing keys plus new key
  uint32_t temp_keys[LEAF_NODE_MAX_CELLS + 1];
  Row temp_values[LEAF_NODE_MAX_CELLS + 1];

  // Copy all existing keys and values plus the new key and value into temp
  // arrays
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
    serialize_row(&temp_values[LEAF_NODE_LEFT_SPLIT_COUNT + i],
                  leaf_node_value(new_node, i));
  }

  *leaf_node_num_cells(old_node) = LEAF_NODE_LEFT_SPLIT_COUNT;
  *leaf_node_num_cells(new_node) = LEAF_NODE_RIGHT_SPLIT_COUNT;

  if (is_node_root(old_node)) {
    return create_root_node(cursor->table, new_page_num);
  } else {
    printf("Need to implement updating parent after split\n");
    exit(EXIT_FAILURE);
  }
}

uint32_t *internal_node_num_keys(void *node) {
  return node + INTERNAL_NODE_NUM_KEYS_OFFSET;
}

uint32_t *internal_node_right_child(void *node) {
  return node + INTERNAL_NODE_RIGHT_CHILD_OFFSET;
}

void *internal_node_cell(void *node, uint32_t cell_num) {
  return node + INTERNAL_NODE_HEADER_SIZE + cell_num * INTERNAL_NODE_CELL_SIZE;
}

uint32_t *internal_node_child(void *node, uint32_t child_num) {
  uint32_t num_keys = *internal_node_num_keys(node);
  if (child_num > num_keys) {
    printf("Tried to access child_num %d > num_keys %d\n", child_num, num_keys);
    exit(EXIT_FAILURE);
  } else if (child_num == num_keys) {
    return internal_node_right_child(node);
  } else {
    return internal_node_cell(node, child_num);
  }
}

uint32_t *internal_node_key(void *node, uint32_t key_num) {
  return internal_node_cell(node, key_num) + INTERNAL_NODE_CHILD_SIZE;
}

void initialize_internal_node(void *node) {
  set_node_type(node, NODE_INTERNAL);
  set_node_root(node, false);
  *internal_node_num_keys(node) = 0;
}

uint32_t get_unused_page_num(Pager *pager) { return pager->num_pages; }

uint32_t get_node_max_key(void *node) {
  switch (get_node_type(node)) {
  case NODE_INTERNAL:
    return *internal_node_key(node, *internal_node_num_keys(node) - 1);
  case NODE_LEAF:
    return *leaf_node_key(node, *leaf_node_num_cells(node) - 1);
  }
  return 0;
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
