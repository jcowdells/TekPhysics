#include "stack.h"

#include <stdlib.h>

/**
 * Create a stack from an existing struct.
 * @note Not strictly necessary, essentially just zeroes out the struct.
 * @param stack The stack to initialise.
 */
void stackCreate(Stack* stack) {
    // just make sure that the stack is empty
    stack->data = 0;
    stack->length = 0;
}

/**
 * Delete a stack, by freeing all nodes of the stack.
 * @note Doesn't free any of the stored data, just the structure itself. Freeing data should be done manually.
 * @param stack The stack to delete.
 */
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

/**
 * Push an item onto the stack.
 * @note This will not copy the data that the pointer points to, it will only store the pointer. So beware of the pointer being freed while stack still exists.
 * @param stack The stack to push an item onto.
 * @param data Data / a pointer to be added.
 * @throws MEMORY_EXCEPTION if a new node could not be allocated.
 */
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

/**
 * Pop an item from the stack.
 * @param stack The stack to pop from.
 * @param data A pointer to where the data will be stored.
 * @throws STACK_EXCEPTION if the stack is empty.
 */
exception stackPop(Stack* stack, void** data) {
    if (!stack->data) tekThrow(STACK_EXCEPTION, "Stack is empty");

    StackItem* item = stack->data;
    *data = item->data;
    stack->data = item->next;
    free(item);

    // decrement length
    stack->length--;
    return SUCCESS;
}

/**
 * Peek the stack / return item on the top of the stack without removing it.
 * @param stack The stack to peek.
 * @param data A pointer to where the data should be stored.
 * @throws STACK_EXCEPTION if the stack is empty.
 */
exception stackPeek(const Stack* stack, void** data) {
    if (!stack->data) tekThrow(STACK_EXCEPTION, "Stack is empty");
    *data = stack->data->data;

    return SUCCESS;
}