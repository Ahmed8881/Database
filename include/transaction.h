#ifndef TRANSACTION_H
#define TRANSACTION_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include "table.h"

typedef enum {
    TRANSACTION_IDLE,
    TRANSACTION_ACTIVE,
    TRANSACTION_COMMITTED,
    TRANSACTION_ABORTED
} TransactionState;

typedef struct RowChange {
    uint32_t page_num;
    uint32_t cell_num;
    uint32_t key;
    void* old_data;        // Original data for rollback
    uint32_t old_size;     // Size of old data
    struct RowChange* next;
} RowChange;

typedef struct {
    uint32_t id;
    TransactionState state;
    time_t start_time;
    RowChange* changes;    // Linked list of changes made
    uint32_t change_count;
} Transaction;

typedef struct {
    Transaction* transactions;
    uint32_t capacity;
    uint32_t count;
    uint32_t next_id;
    bool enabled;          // Flag to enable/disable transactions
} TransactionManager;

// Transaction Manager functions
void txn_manager_init(TransactionManager* manager, uint32_t capacity);
void txn_manager_free(TransactionManager* manager);
bool txn_manager_enable(TransactionManager* manager);
bool txn_manager_disable(TransactionManager* manager);
bool txn_manager_is_enabled(TransactionManager* manager);

// Transaction functions
uint32_t txn_begin(TransactionManager* manager);
bool txn_commit(TransactionManager* manager, uint32_t txn_id);
bool txn_rollback(TransactionManager* manager, uint32_t txn_id);
bool txn_is_active(TransactionManager* manager, uint32_t txn_id);

// Record change tracking functions
bool txn_record_change(TransactionManager* manager, 
                      uint32_t txn_id, 
                      uint32_t page_num, 
                      uint32_t cell_num, 
                      uint32_t key,
                      void* old_data,
                      uint32_t old_size);

// Helper functions for command processor
void txn_print_status(TransactionManager* manager, uint32_t txn_id);
void txn_print_all(TransactionManager* manager);

#endif // TRANSACTION_H