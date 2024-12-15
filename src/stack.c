#include "../include/stack.h"
#include <stdlib.h>

void stack_init(Stack* stack) {
    stack->top = NULL;
    stack->size = 0;
}

bool stack_push(Stack* stack, void* data, uint32_t page_num, uint32_t level) {
    StackNode* node = (StackNode*)malloc(sizeof(StackNode));
    if (!node) return false;
    
    node->data = data;
    node->page_num = page_num;
    node->level = level;
    node->next = stack->top;
    stack->top = node;
    stack->size++;
    return true;
}

StackNode* stack_pop(Stack* stack) {
    if (stack_is_empty(stack)) return NULL;
    
    StackNode* node = stack->top;
    stack->top = stack->top->next;
    stack->size--;
    return node;
}

StackNode* stack_peek(Stack* stack) {
    return stack->top;
}

bool stack_is_empty(Stack* stack) {
    return stack->top == NULL;
}

void stack_destroy(Stack* stack) {
    while (!stack_is_empty(stack)) {
        StackNode* node = stack_pop(stack);
        free(node);
    }
}