typedef struct StackNode
{
   char *command;
   struct StackNode *next;
};
typedef struct Stack
{
   struct StackNode *top;
};

// functions declarations
struct Stack *createStack();
void push(struct Stack *stack, char *command);
char *pop(struct Stack *stack);
char *peek(struct Stack *stack);
int isEmpty(struct Stack *stack);