#ifndef STACK_H
#define STACK_H

#include <stdint.h>
#include <stdbool.h>

// Stack node structure for tree traversal
typedef struct StackNode
{
   void *data;
   uint32_t page_num;
   uint32_t level;
   struct StackNode *next;
} StackNode;

typedef struct
{
   StackNode *top;
   int size;
} Stack;

// Stack operations
void stack_init(Stack *stack);
bool stack_push(Stack *stack, void *data, uint32_t page_num, uint32_t level);
StackNode *stack_pop(Stack *stack);
StackNode *stack_peek(Stack *stack);
bool stack_is_empty(Stack *stack);
void stack_destroy(Stack *stack);

#endif