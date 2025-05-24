#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <pthread.h>
#include <stdbool.h>

// Forward declaration to avoid circular dependency
typedef struct ClientConnection ClientConnection;

// Task structure for the thread pool
typedef struct {
    void (*function)(void*);
    void *argument;
} Task;

// Thread pool structure
typedef struct {
    pthread_t *threads;
    
    // Task queue
    Task *tasks;
    int task_capacity;
    int task_size;
    int task_front;
    int task_rear;
    
    // Synchronization
    pthread_mutex_t queue_lock;
    pthread_cond_t queue_not_empty;
    pthread_cond_t queue_not_full;
    
    // Thread pool state
    int num_threads;
    bool shutdown;
} ThreadPool;

// Thread pool functions
ThreadPool* thread_pool_create(int num_threads, int queue_size);
void thread_pool_destroy(ThreadPool *pool);
bool thread_pool_add_task(ThreadPool *pool, void (*function)(void*), void *argument);
void* worker_thread(void *arg);

#endif // THREAD_POOL_H