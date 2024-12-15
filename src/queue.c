#include "queue.h"
#include <stdlib.h>

void queue_init(Queue* queue) {
    queue->front = NULL;
    queue->rear = NULL;
    queue->size = 0;  // number of elements in the queue
}

bool queue_enqueue(Queue* queue, void* data, uint32_t page_num, uint32_t level) {
    QueueNode* new_node = (QueueNode*)malloc(sizeof(QueueNode));
    if (new_node == NULL) {
        return false;
    }
    new_node->data = data;
    new_node->page_num = page_num;
    new_node->level = level;
    new_node->next = NULL;

    if (queue->rear == NULL) {
        queue->front = new_node;
    } else {
        queue->rear->next = new_node;
    }
    queue->rear = new_node;
    queue->size++;
    return true;
}

QueueNode* queue_dequeue(Queue* queue) {
    if (queue->front == NULL) {
        return NULL;
    }
    QueueNode* temp = queue->front;
    queue->front = queue->front->next;
    if (queue->front == NULL) {
        queue->rear = NULL;
    }
    queue->size--;
    return temp;
}

bool queue_is_empty(Queue* queue) {
    return queue->size == 0;
}

void queue_destroy(Queue* queue) {
    while (!queue_is_empty(queue)) {
        QueueNode* temp = queue_dequeue(queue);
        free(temp);
    }
}