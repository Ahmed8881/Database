#ifndef QUEUE_H
#define QUEUE_H

#include <stdbool.h>
#include <stdint.h>

typedef struct QueueNode {
    void* data;
    uint32_t page_num;
    uint32_t level;
    struct QueueNode* next;
} QueueNode;

typedef struct {
    QueueNode* front;
    QueueNode* rear;
    int size;
} Queue;

void queue_init(Queue* queue);
bool queue_enqueue(Queue* queue, void* data, uint32_t page_num, uint32_t level);
QueueNode* queue_dequeue(Queue* queue);
bool queue_is_empty(Queue* queue);
void queue_destroy(Queue* queue);

#endif