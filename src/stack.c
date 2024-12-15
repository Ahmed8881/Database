#include <stdlib.h>
#include <string.h>
#include "../include/stack.h"

// create a new stack
struct Stack *createStack()
{
   struct Stack *stack = (struct Stack *)malloc(sizeof(struct Stack));
   stack->top = NULL;
   return stack;
}

// push a new command to the stack
void push(struct Stack *stack, char *command)
{
   struct StackNode *newNode = (struct StackNode *)malloc(sizeof(struct StackNode));
   newNode->command = command;
   newNode->next = stack->top;
   stack->top = newNode;
}

// pop the top command from the stack
char *pop(struct Stack *stack)
{
   if (isEmpty(stack))
   {
      return NULL;
   }
   struct StackNode *temp = stack->top;
   stack->top = stack->top->next;
   char *command = temp->command;
   free(temp);
   return command;
}

// get the top command from the stack
char *peek(struct Stack *stack)
{
   if (isEmpty(stack))
   {
      return NULL;
   }
   return stack->top->command;
}

// check if the stack is empty
int isEmpty(struct Stack *stack)
{
   return stack->top == NULL;
}