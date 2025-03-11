#include "stack.h"

#include <stdlib.h>

void stackCreate(Stack* stack) {
    // just make sure that the list is empty
    stack->data = 0;
    stack->length = 0;
}

void stackDelete(Stack* stack) {
    // iterate over each item
    StackItem* item = stack->data;

    // if item is 0, it is the last item of the stack
    while (item) {
        // we need to save the pointer to next, so it isn't lost when item is freed
        StackItem* next = item->next;
        free(item);
        item = next;
    }
    // clear the stack so it can't be accidentally used
    stack->data = 0;
    stack->length = 0;
}

exception stackPush(Stack* stack, void* data) {
    // store the original pointer
    StackItem* next = stack->data;

    // keep a pointer to the start so we can overwrite it
    StackItem** item_ptr = &stack->data;

    // write new item
    *item_ptr = (StackItem*)malloc(sizeof(StackItem));
    if (!*item_ptr) tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for stack.");

    // fill item with data, using 'next' we stored
    (*item_ptr)->data = data;
    (*item_ptr)->next = next;

    // increment length
    stack->length++;
    return SUCCESS;
}

exception stackPop(Stack* stack, void** data) {
    if (!stack->data) tekThrow(MEMORY_EXCEPTION, "Stack is empty");

    StackItem* item = stack->data;
    *data = item->data;
    stack->data = item->next;
    free(item);

    return SUCCESS;
}

exception stackPeek(const Stack* stack, void** data) {
    if (!stack->data) tekThrow(MEMORY_EXCEPTION, "Stack is empty");
    *data = stack->data->data;

    return SUCCESS;
}