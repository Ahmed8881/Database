#include "../include/transaction.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void txn_manager_init(TransactionManager* manager, uint32_t capacity) {
    manager->transactions = malloc(sizeof(Transaction) * capacity);
    manager->capacity = capacity;
    manager->count = 0;
    manager->next_id = 1;  // Start with txn_id = 1
    manager->enabled = false;
    
    // Initialize all transactions to idle state
    for (uint32_t i = 0; i < capacity; i++) {
        manager->transactions[i].id = 0;  // 0 means unused slot
        manager->transactions[i].state = TRANSACTION_IDLE;
        manager->transactions[i].changes = NULL;
        manager->transactions[i].change_count = 0;
    }
}

void txn_free_changes(RowChange* changes) {
    RowChange* current = changes;
    while (current != NULL) {
        RowChange* next = current->next;
        if (current->old_data) {
            free(current->old_data);
        }
        free(current);
        current = next;
    }
}

void txn_manager_free(TransactionManager* manager) {
    if (!manager) return;
    
    // Free all transaction data
    for (uint32_t i = 0; i < manager->capacity; i++) {
        txn_free_changes(manager->transactions[i].changes);
    }
    
    free(manager->transactions);
    manager->transactions = NULL;
    manager->capacity = 0;
    manager->count = 0;
}

bool txn_manager_enable(TransactionManager* manager) {
    if (!manager) return false;
    manager->enabled = true;
    printf("Transaction support enabled.\n");
    return true;
}

bool txn_manager_disable(TransactionManager* manager) {
    if (!manager) return false;
    
    // Check if any active transactions exist
    for (uint32_t i = 0; i < manager->capacity; i++) {
        if (manager->transactions[i].id != 0 && 
            manager->transactions[i].state == TRANSACTION_ACTIVE) {
            printf("Cannot disable transactions: active transactions exist.\n");
            return false;
        }
    }
    
    manager->enabled = false;
    printf("Transaction support disabled.\n");
    return true;
}

bool txn_manager_is_enabled(TransactionManager* manager) {
    if (!manager) return false;
    return manager->enabled;
}

// Find an available transaction slot or returns -1
static int find_available_slot(TransactionManager* manager) {
    for (uint32_t i = 0; i < manager->capacity; i++) {
        if (manager->transactions[i].id == 0) {
            return i;
        }
    }
    return -1;
}

// Find transaction by ID, returns index or -1 if not found
static int find_transaction(TransactionManager* manager, uint32_t txn_id) {
    for (uint32_t i = 0; i < manager->capacity; i++) {
        if (manager->transactions[i].id == txn_id) {
            return i;
        }
    }
    return -1;
}

uint32_t txn_begin(TransactionManager* manager) {
    if (!manager || !manager->enabled) {
        return 0;  // 0 means invalid transaction
    }
    
    // Check if we've reached capacity
    if (manager->count >= manager->capacity) {
        printf("Error: Maximum number of concurrent transactions reached.\n");
        return 0;
    }
    
    int slot = find_available_slot(manager);
    if (slot < 0) {
        printf("Error: No available transaction slots.\n");
        return 0;
    }
    
    uint32_t txn_id = manager->next_id++;
    if (manager->next_id == 0) manager->next_id = 1;  // Avoid 0 as it's invalid
    
    Transaction* txn = &manager->transactions[slot];
    txn->id = txn_id;
    txn->state = TRANSACTION_ACTIVE;
    txn->start_time = time(NULL);
    txn->changes = NULL;
    txn->change_count = 0;
    
    manager->count++;
    
    printf("Transaction %u started.\n", txn_id);
    return txn_id;
}

bool txn_commit(TransactionManager* manager, uint32_t txn_id) {
    if (!manager || !manager->enabled || txn_id == 0) {
        return false;
    }
    
    int txn_idx = find_transaction(manager, txn_id);
    if (txn_idx < 0) {
        printf("Error: Transaction %u not found.\n", txn_id);
        return false;
    }
    
    Transaction* txn = &manager->transactions[txn_idx];
    if (txn->state != TRANSACTION_ACTIVE) {
        printf("Error: Cannot commit transaction %u, not active.\n", txn_id);
        return false;
    }
    
    // Free any tracked changes as they're no longer needed
    txn_free_changes(txn->changes);
    txn->changes = NULL;
    
    // Mark as committed
    txn->state = TRANSACTION_COMMITTED;
    
    printf("Transaction %u committed successfully.\n", txn_id);
    
    // Clean up the transaction
    txn->id = 0;  // Mark slot as available
    txn->state = TRANSACTION_IDLE;
    manager->count--;
    
    return true;
}

bool txn_rollback(TransactionManager* manager, uint32_t txn_id) {
    if (!manager || !manager->enabled || txn_id == 0) {
        return false;
    }
    
    int txn_idx = find_transaction(manager, txn_id);
    if (txn_idx < 0) {
        printf("Error: Transaction %u not found.\n", txn_id);
        return false;
    }
    
    Transaction* txn = &manager->transactions[txn_idx];
    if (txn->state != TRANSACTION_ACTIVE) {
        printf("Error: Cannot rollback transaction %u, not active.\n", txn_id);
        return false;
    }
    
    // TODO: Apply rollback changes in reverse order
    // This requires implementing a way to restore original data
    // to the respective pages
    
    // For now, just print what would be rolled back
    printf("Rolling back transaction %u (%u changes):\n", txn_id, txn->change_count);
    
    RowChange* change = txn->changes;
    while (change != NULL) {
        printf("  - Reverting change to key %u on page %u, cell %u\n", 
            change->key, change->page_num, change->cell_num);
        change = change->next;
    }
    
    // Free the change tracking data
    txn_free_changes(txn->changes);
    txn->changes = NULL;
    
    // Mark as aborted
    txn->state = TRANSACTION_ABORTED;
    
    printf("Transaction %u rolled back.\n", txn_id);
    
    // Clean up the transaction
    txn->id = 0;  // Mark slot as available
    txn->state = TRANSACTION_IDLE;
    manager->count--;
    
    return true;
}

bool txn_is_active(TransactionManager* manager, uint32_t txn_id) {
    if (!manager || !manager->enabled || txn_id == 0) {
        return false;
    }
    
    int txn_idx = find_transaction(manager, txn_id);
    if (txn_idx < 0) {
        return false;
    }
    
    return manager->transactions[txn_idx].state == TRANSACTION_ACTIVE;
}

bool txn_record_change(TransactionManager* manager, 
                      uint32_t txn_id, 
                      uint32_t page_num, 
                      uint32_t cell_num, 
                      uint32_t key,
                      void* old_data,
                      uint32_t old_size) {
    if (!manager || !manager->enabled || txn_id == 0 || !old_data) {
        return false;
    }
    
    int txn_idx = find_transaction(manager, txn_id);
    if (txn_idx < 0) {
        return false;
    }
    
    Transaction* txn = &manager->transactions[txn_idx];
    if (txn->state != TRANSACTION_ACTIVE) {
        return false;
    }
    
    // Create new change record
    RowChange* change = malloc(sizeof(RowChange));
    if (!change) {
        return false;
    }
    
    // Make a copy of the old data for potential rollback
    void* data_copy = malloc(old_size);
    if (!data_copy) {
        free(change);
        return false;
    }
    
    memcpy(data_copy, old_data, old_size);
    
    // Set up the change record
    change->page_num = page_num;
    change->cell_num = cell_num;
    change->key = key;
    change->old_data = data_copy;
    change->old_size = old_size;
    change->next = txn->changes;  // Add to front of list
    
    // Update transaction
    txn->changes = change;
    txn->change_count++;
    
    return true;
}

void txn_print_status(TransactionManager* manager, uint32_t txn_id) {
    if (!manager || txn_id == 0) {
        printf("Invalid transaction.\n");
        return;
    }
    
    int txn_idx = find_transaction(manager, txn_id);
    if (txn_idx < 0) {
        printf("Transaction %u not found.\n", txn_id);
        return;
    }
    
    Transaction* txn = &manager->transactions[txn_idx];
    
    printf("Transaction %u: ", txn_id);
    switch (txn->state) {
        case TRANSACTION_IDLE:
            printf("IDLE");
            break;
        case TRANSACTION_ACTIVE:
            printf("ACTIVE");
            break;
        case TRANSACTION_COMMITTED:
            printf("COMMITTED");
            break;
        case TRANSACTION_ABORTED:
            printf("ABORTED");
            break;
    }
    
    printf(", Changes: %u\n", txn->change_count);
    
    // Convert start time to readable format
    char time_buf[64];
    struct tm* tm_info = localtime(&txn->start_time);
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);
    
    printf("Started: %s\n", time_buf);
}

void txn_print_all(TransactionManager* manager) {
    if (!manager) {
        return;
    }
    
    printf("Transaction Manager Status:\n");
    printf("Enabled: %s\n", manager->enabled ? "YES" : "NO");
    printf("Active transactions: %u/%u\n", manager->count, manager->capacity);
    
    bool found_active = false;
    for (uint32_t i = 0; i < manager->capacity; i++) {
        if (manager->transactions[i].id != 0) {
            found_active = true;
            printf("------------------------------------------\n");
            txn_print_status(manager, manager->transactions[i].id);
        }
    }
    
    if (!found_active) {
        printf("No active transactions.\n");
    }
}