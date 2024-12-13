#include "../include/btree.h"
#include <string.h>

uint32_t* leaf_node_num_cells(void* node) {
    return (uint32_t*)((uint8_t*)node + LEAF_NODE_NUM_CELLS_OFFSET);
}

void* leaf_node_cell(void* node, uint32_t cell_num) {
    return (uint8_t*)node + LEAF_NODE_HEADER_SIZE + cell_num * LEAF_NODE_CELL_SIZE;
}

uint32_t* leaf_node_key(void* node, uint32_t cell_num) {
    return (uint32_t*)leaf_node_cell(node, cell_num);
}

void* leaf_node_value(void* node, uint32_t cell_num) {
    return (uint8_t*)leaf_node_cell(node, cell_num) + LEAF_NODE_KEY_SIZE;
}

void initialize_leaf_node(void* node) {
    // *((uint8_t*)node + NODE_TYPE_OFFSET) = NODE_LEAF;
    *((uint8_t*)node + IS_ROOT_OFFSET) = 0;
    *leaf_node_num_cells(node) = 0;
}

void leaf_node_insert(Cursor* cursor, uint32_t key, Row* value) {
    void* node = get_page(cursor->table->pager, cursor->page_num);
    uint32_t num_cells = *leaf_node_num_cells(node);
    
    if (num_cells >= LEAF_NODE_MAX_CELLS) {
        // Node full
        printf("Need to implement splitting leaf node\n");
        exit(EXIT_FAILURE);
    }

    if (cursor->cell_num < num_cells) {
        // Make room for new cell
        for (uint32_t i = num_cells; i > cursor->cell_num; i--) {
            memcpy(leaf_node_cell(node, i), 
                   leaf_node_cell(node, i - 1),
                   LEAF_NODE_CELL_SIZE);
        }
    }

    *(leaf_node_num_cells(node)) += 1;
    *(leaf_node_key(node, cursor->cell_num)) = key;
    serialize_row(value, leaf_node_value(node, cursor->cell_num));
}