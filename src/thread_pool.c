#include "../include/thread_pool.h"
#include <stdlib.h>
#include <stdio.h>

// Create a new thread pool
ThreadPool* thread_pool_create(int num_threads, int queue_size) {
    if (num_threads <= 0 || queue_size <= 0) {
        return NULL;
    }
    
    ThreadPool *pool = malloc(sizeof(ThreadPool));
    if (!pool) {
        return NULL;
    }
    
    // Initialize pool members
    pool->threads = malloc(sizeof(pthread_t) * num_threads);
    pool->tasks = malloc(sizeof(Task) * queue_size);
    pool->task_capacity = queue_size;
    pool->task_size = 0;
    pool->task_front = 0;
    pool->task_rear = 0;
    pool->num_threads = num_threads;
    pool->shutdown = false;
    
    // Check allocations
    if (!pool->threads || !pool->tasks) {
        if (pool->threads) free(pool->threads);
        if (pool->tasks) free(pool->tasks);
        free(pool);
        return NULL;
    }
    
    // Initialize synchronization primitives
    if (pthread_mutex_init(&pool->queue_lock, NULL) != 0 ||
        pthread_cond_init(&pool->queue_not_empty, NULL) != 0 ||
        pthread_cond_init(&pool->queue_not_full, NULL) != 0) {
        
        free(pool->threads);
        free(pool->tasks);
        free(pool);
        return NULL;
    }
    
    // Create worker threads
    for (int i = 0; i < num_threads; i++) {
        if (pthread_create(&pool->threads[i], NULL, worker_thread, pool) != 0) {
            // Clean up on error
            pool->shutdown = true;
            pthread_cond_broadcast(&pool->queue_not_empty);
            
            // Wait for created threads to finish
            for (int j = 0; j < i; j++) {
                pthread_join(pool->threads[j], NULL);
            }
            
            // Free resources
            pthread_mutex_destroy(&pool->queue_lock);
            pthread_cond_destroy(&pool->queue_not_empty);
            pthread_cond_destroy(&pool->queue_not_full);
            free(pool->threads);
            free(pool->tasks);
            free(pool);
            return NULL;
        }
    }
    
    return pool;
}

// Destroy a thread pool
void thread_pool_destroy(ThreadPool *pool) {
    if (!pool) {
        return;
    }
    
    // Set shutdown flag and broadcast to all threads
    pthread_mutex_lock(&pool->queue_lock);
    pool->shutdown = true;
    pthread_cond_broadcast(&pool->queue_not_empty);
    pthread_mutex_unlock(&pool->queue_lock);
    
    // Wait for all threads to finish
    for (int i = 0; i < pool->num_threads; i++) {
        pthread_join(pool->threads[i], NULL);
    }
    
    // Free resources
    pthread_mutex_destroy(&pool->queue_lock);
    pthread_cond_destroy(&pool->queue_not_empty);
    pthread_cond_destroy(&pool->queue_not_full);
    free(pool->threads);
    free(pool->tasks);
    free(pool);
}

// Add a task to the thread pool
bool thread_pool_add_task(ThreadPool *pool, void (*function)(void*), void *argument) {
    if (!pool || !function) {
        return false;
    }
    
    pthread_mutex_lock(&pool->queue_lock);
    
    // Wait until queue has space
    while (pool->task_size == pool->task_capacity && !pool->shutdown) {
        pthread_cond_wait(&pool->queue_not_full, &pool->queue_lock);
    }
    
    // If pool is shutting down, don't add new tasks
    if (pool->shutdown) {
        pthread_mutex_unlock(&pool->queue_lock);
        return false;
    }
    
    // Add task to queue
    Task *task = &pool->tasks[pool->task_rear];
    task->function = function;
    task->argument = argument;
    
    // Update queue pointers
    pool->task_rear = (pool->task_rear + 1) % pool->task_capacity;
    pool->task_size++;
    
    // Signal that queue is not empty
    pthread_cond_signal(&pool->queue_not_empty);
    pthread_mutex_unlock(&pool->queue_lock);
    
    return true;
}

// Worker thread function that processes tasks from the queue
void* worker_thread(void *arg) {
    ThreadPool *pool = (ThreadPool*)arg;
    Task task;
    
    while (1) {
        // Get task from queue
        pthread_mutex_lock(&pool->queue_lock);
        
        // Wait for tasks or shutdown signal
        while (pool->task_size == 0 && !pool->shutdown) {
            pthread_cond_wait(&pool->queue_not_empty, &pool->queue_lock);
        }
        
        // If pool is shutting down and queue is empty, exit
        if (pool->shutdown && pool->task_size == 0) {
            pthread_mutex_unlock(&pool->queue_lock);
            pthread_exit(NULL);
        }
        
        // Get task from queue
        task.function = pool->tasks[pool->task_front].function;
        task.argument = pool->tasks[pool->task_front].argument;
        
        // Update queue pointers
        pool->task_front = (pool->task_front + 1) % pool->task_capacity;
        pool->task_size--;
        
        // Signal that queue is not full
        pthread_cond_signal(&pool->queue_not_full);
        pthread_mutex_unlock(&pool->queue_lock);
        
        // Execute task
        task.function(task.argument);
    }
    
    return NULL;
}