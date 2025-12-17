#include "unit_test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "../core/testsuite.h"

#include "../core/vector.h"
#include "../core/list.h"
#include "../core/stack.h"
#include "../core/queue.h"
#include "../core/priorityqueue.h"
#include "../core/bitset.h"
#include "../core/hashtable.h"
#include "../core/threadqueue.h"
#include "../core/yml.h"
#include "../core/file.h"

typedef union TestContext {
    Vector vector;
    List list;
    Stack stack;
    Queue queue;
    PriorityQueue priority_queue;
    BitSet bitset;
    HashTable hashtable;
    ThreadQueue thread_queue;
    char* file;
    YmlFile yml;
} TestContext;

tekTestCreate(vector) (TestContext* test_context) {
    tekChainThrow(vectorCreate(1, sizeof(int), &test_context->vector));
    return SUCCESS;
}

tekTestDelete(vector) (TestContext* test_context) {
    vectorDelete(&test_context->vector);
    return SUCCESS;
}

tekTestFunc(vector, add_one_int) (TestContext* test_context) {
    // add an integer to vector
    const int number = 12345;
    tekChainThrow(vectorAddItem(&test_context->vector, &number));

    // confirm that the length is one.
    tekAssert(1, test_context->vector.length);

    // confirm that the integer has been stored. if working correctly, should be at the first index of the internal array.
    const int* internal = test_context->vector.internal;
    tekAssert(number, internal[0]);

    return SUCCESS;
}

tekTestFunc(vector, resizing) (TestContext* test_context) {
    tekAssert(1, test_context->vector.internal_size);

    const int zero = 0;
    for (uint i = 0; i < 10; i++) {
        tekChainThrow(vectorAddItem(&test_context->vector, &zero));
    }

    // after adding 10 items, the internal size should be big enough to fit them.
    tekAssert(1, test_context->vector.internal_size >= 10);

    return SUCCESS;
}

tekTestFunc(vector, add_ten_ints) (TestContext* test_context) {
    // add 10 integers to the vector
    const int numbers[] = {
        1388, 7609, 7550, 603, 1161, 6109, 6706, 5413, 7497, 4128
    };
    const uint len_numbers = sizeof(numbers) / sizeof(int);
    for (uint i = 0; i < len_numbers; i++) {
        tekChainThrow(vectorAddItem(&test_context->vector, &numbers[i]));
    }

    // confirm that the length is ten.
    tekAssert(10, test_context->vector.length);

    // confirm that the ten integers have been stored.
    const int* internal = test_context->vector.internal;
    tekAssert(0, memcmp(numbers, internal, sizeof(numbers)));

    return SUCCESS;
}

tekTestFunc(vector, get_an_int) (TestContext* test_context) {
    // add 10 integers to the vector
    const int numbers[] = {
        4543, 5832, 66, 1453, 8936, 4936, 1709, 5623, 7166, 3425
    };
    const uint len_numbers = sizeof(numbers) / sizeof(int);
    for (uint i = 0; i < len_numbers; i++) {
        tekChainThrow(vectorAddItem(&test_context->vector, &numbers[i]));
    }

    // get number at index 0
    int number;
    tekChainThrow(vectorGetItem(&test_context->vector, 0, &number));
    tekAssert(numbers[0], number);

    // get number at index 5
    tekChainThrow(vectorGetItem(&test_context->vector, 5, &number));
    tekAssert(numbers[5], number);

    return SUCCESS;
}

tekTestFunc(vector, get_an_int_ptr) (TestContext* test_context) {
    // add 10 integers to the vector
    const int numbers[] = {
        4543, 5832, 66, 1453, 8936, 4936, 1709, 5623, 7166, 3425
    };
    const uint len_numbers = sizeof(numbers) / sizeof(int);
    for (uint i = 0; i < len_numbers; i++) {
        tekChainThrow(vectorAddItem(&test_context->vector, &numbers[i]));
    }

    // retrieve a pointer into the list instead of the actual data.

    // get number pointer at index 0
    int* number;
    tekChainThrow(vectorGetItemPtr(&test_context->vector, 0, &number));
    tekAssert(numbers[0], *number);

    // get number pointer at index 5
    tekChainThrow(vectorGetItemPtr(&test_context->vector, 5, &number));
    tekAssert(numbers[5], *number);

    return SUCCESS;
}

tekTestFunc(vector, set_an_int) (TestContext* test_context) {
    const int values[] = {10, 20, 30, 40};
    for (uint i = 0; i < 4; i++)
        tekChainThrow(vectorAddItem(&test_context->vector, &values[i]));

    // overwrite the value at index 2
    const int new_value = 999;
    tekChainThrow(vectorSetItem(&test_context->vector, 2, &new_value));

    // verify
    int result;
    tekChainThrow(vectorGetItem(&test_context->vector, 2, &result));
    tekAssert(new_value, result);

    return SUCCESS;
}

tekTestFunc(vector, insert_an_int) (TestContext* test_context) {
    const int numbers[] = {1, 2, 3};
    for (uint i = 0; i < 2; i++)
        tekChainThrow(vectorAddItem(&test_context->vector, &numbers[i]));

    // insert value at the beginning (index 0)
    const int inserted = 999;
    tekChainThrow(vectorInsertItem(&test_context->vector, 0, &inserted));

    const int* internal = test_context->vector.internal;
    tekAssert(3, test_context->vector.length);
    tekAssert(inserted, internal[0]);
    tekAssert(numbers[0], internal[1]);
    tekAssert(numbers[1], internal[2]);

    return SUCCESS;
}

tekTestFunc(vector, remove_an_int) (TestContext* test_context) {
    const int numbers[] = {10, 20, 30};
    for (uint i = 0; i < 3; i++)
        tekChainThrow(vectorAddItem(&test_context->vector, &numbers[i]));

    int removed;
    tekChainThrow(vectorRemoveItem(&test_context->vector, 1, &removed));
    tekAssert(20, removed);

    const int* internal = test_context->vector.internal;
    tekAssert(2, test_context->vector.length);
    tekAssert(numbers[0], internal[0]);
    tekAssert(numbers[2], internal[1]);

    return SUCCESS;
}

tekTestFunc(vector, pop_an_int) (TestContext* test_context) {
    const int numbers[] = {11, 22, 33};
    for (uint i = 0; i < 3; i++)
        tekChainThrow(vectorAddItem(&test_context->vector, &numbers[i]));

    int popped;
    const flag result = vectorPopItem(&test_context->vector, &popped);
    tekAssert(1, result);
    tekAssert(numbers[2], popped);
    tekAssert(2, test_context->vector.length);

    return SUCCESS;
}

tekTestFunc(vector, clear_vector) (TestContext* test_context) {
    int n = 123;
    for (int i = 0; i < 3; i++)
        tekChainThrow(vectorAddItem(&test_context->vector, &n));

    vectorClear(&test_context->vector);
    tekAssert(0, test_context->vector.length);

    return SUCCESS;
}

tekTestFunc(vector, boundary_index_tests) (TestContext* test_context) {
    const int numbers[] = {1, 2};
    for (uint i = 0; i < 2; i++)
        tekChainThrow(vectorAddItem(&test_context->vector, &numbers[i]));

    // valid boundary - last index
    int value;
    tekChainThrow(vectorGetItem(&test_context->vector, 1, &value));
    tekAssert(2, value);

    // invalid boundary - index out of range
    const exception tek_exception = vectorGetItem(&test_context->vector, 2, &value);
    tekAssert(VECTOR_EXCEPTION, tek_exception);

    return SUCCESS;
}

struct VectorTestBlock {
    int integer;
    char* string;
    void* pointer;
};

tekTestFunc(vector, big_block) (TestContext* test_context) {
    tekChainThrow(vectorCreate(1, sizeof(struct VectorTestBlock), &test_context->vector));

    // testing some random struct
    const struct VectorTestBlock test_block_0 = {
        123, "Test String", test_context
    };
    const struct VectorTestBlock test_block_1 = {
        456, "Another Test String", test_context + 1
    };
    const struct VectorTestBlock test_block_2 = {
        789, NULL, test_context + 2
    };

    tekChainThrow(vectorAddItem(&test_context->vector, &test_block_0));
    tekChainThrow(vectorAddItem(&test_context->vector, &test_block_1));
    tekChainThrow(vectorAddItem(&test_context->vector, &test_block_2));

    // check that data can be copied out of the vector
    struct VectorTestBlock test_block;
    tekChainThrow(vectorGetItem(&test_context->vector, 0, &test_block));
    tekAssert(0, memcmp(&test_block_0, &test_block, sizeof(struct VectorTestBlock)));

    tekChainThrow(vectorGetItem(&test_context->vector, 1, &test_block));
    tekAssert(0, memcmp(&test_block_1, &test_block, sizeof(struct VectorTestBlock)));

    // check the functionality of getting pointer.
    struct VectorTestBlock* test_block_ptr;
    tekChainThrow(vectorGetItemPtr(&test_context->vector, 2, &test_block_ptr));
    tekAssert(0, memcmp(&test_block_2, test_block_ptr, sizeof(struct VectorTestBlock)));

    return SUCCESS;
}

tekTestFunc(vector, invalid_create_tests) (TestContext* _) {
    // element_size is 0
    Vector vec;
    const exception tek_exception = vectorCreate(2, 0, &vec);
    tekAssert(VECTOR_EXCEPTION, tek_exception);

    return SUCCESS;
}

tekTestCreate(list) (TestContext* test_context) {
    listCreate(&test_context->list);
    return SUCCESS;
}

tekTestDelete(list) (TestContext* test_context) {
    listDelete(&test_context->list);
    return SUCCESS;
}

tekTestFunc(list, add_and_get_items) (TestContext* test_context) {
    listCreate(&test_context->list);

    // add 3 integers
    int a = 10, b = 20, c = 30;
    tekChainThrow(listAddItem(&test_context->list, &a));
    tekChainThrow(listAddItem(&test_context->list, &b));
    tekChainThrow(listAddItem(&test_context->list, &c));

    tekAssert(3, test_context->list.length);

    // get items back
    int* value_ptr = NULL;
    tekChainThrow(listGetItem(&test_context->list, 0, (void**)&value_ptr));
    tekAssert(a, *value_ptr);

    tekChainThrow(listGetItem(&test_context->list, 1, (void**)&value_ptr));
    tekAssert(b, *value_ptr);

    tekChainThrow(listGetItem(&test_context->list, 2, (void**)&value_ptr));
    tekAssert(c, *value_ptr);
    return SUCCESS;
}

tekTestFunc(list, insert_and_set_items) (TestContext* test_context) {
    listCreate(&test_context->list);

    int a = 1, b = 2, c = 3, new_b = 99;
    tekChainThrow(listAddItem(&test_context->list, &a));
    tekChainThrow(listAddItem(&test_context->list, &c));

    // insert between a and c
    tekChainThrow(listInsertItem(&test_context->list, 1, &b));
    tekAssert(3, test_context->list.length);

    // change middle value to 99
    tekChainThrow(listSetItem(&test_context->list, 1, &new_b));

    int* value_ptr = NULL;
    tekChainThrow(listGetItem(&test_context->list, 1, (void**)&value_ptr));
    tekAssert(new_b, *value_ptr);
    return SUCCESS;
}

tekTestFunc(list, remove_and_pop_items) (TestContext* test_context) {
    listCreate(&test_context->list);

    int a = 5, b = 6, c = 7;
    tekChainThrow(listAddItem(&test_context->list, &a));
    tekChainThrow(listAddItem(&test_context->list, &b));
    tekChainThrow(listAddItem(&test_context->list, &c));

    // remove middle item
    int* removed_ptr = NULL;
    tekChainThrow(listRemoveItem(&test_context->list, 1, (void**)&removed_ptr));
    tekAssert(&b, removed_ptr);
    tekAssert(2, test_context->list.length);

    // pop last item
    int* popped_ptr = NULL;
    tekChainThrow(listPopItem(&test_context->list, (void**)&popped_ptr));
    tekAssert(&c, popped_ptr);
    tekAssert(1, test_context->list.length);
    return SUCCESS;
}

tekTestFunc(list, move_item) (TestContext* test_context) {
    listCreate(&test_context->list);

    int a = 1, b = 2, c = 3;
    tekChainThrow(listAddItem(&test_context->list, &a));
    tekChainThrow(listAddItem(&test_context->list, &b));
    tekChainThrow(listAddItem(&test_context->list, &c));

    // move first item to end
    tekChainThrow(listMoveItem(&test_context->list, 0, 2));

    // make sure its in the right order
    int* val;
    tekChainThrow(listGetItem(&test_context->list, 0, (void**)&val));
    tekAssert(b, *val);
    tekChainThrow(listGetItem(&test_context->list, 1, (void**)&val));
    tekAssert(c, *val);
    tekChainThrow(listGetItem(&test_context->list, 2, (void**)&val));
    tekAssert(a, *val);
    return SUCCESS;
}

tekTestFunc(list, free_all_data) (TestContext* test_context) {
    listCreate(&test_context->list);

    // mallocate sum data
    int* x = malloc(sizeof(int));
    if (!x) tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for integer.");
    int* y = malloc(sizeof(int));
    if (!x) tekThrowThen(MEMORY_EXCEPTION, "Failed to allocate memory for integer.", { free(x); });
    *x = 111;
    *y = 222;

    tekChainThrowThen(listAddItem(&test_context->list, x), { free(x); free(y); });
    tekChainThrowThen(listAddItem(&test_context->list, y), { free(x); free(y); });

    // free the mallocated data
    listFreeAllData(&test_context->list);

    // make sure that it didn't bugger up
    tekAssert(2, test_context->list.length);
    return SUCCESS;
}

tekTestFunc(list, boundary_and_invalid_tests) (TestContext* test_context) {
    listCreate(&test_context->list);

    int a = 42;
    tekChainThrow(listAddItem(&test_context->list, &a));

    void* out = NULL;

    // boundary
    tekChainThrow(listGetItem(&test_context->list, 0, &out));
    tekAssert(&a, out);

    // invalid - out of range
    const exception tek_exception = listGetItem(&test_context->list, 5, &out);
    tekAssert(LIST_EXCEPTION, tek_exception);
    return SUCCESS;
}

tekTestCreate(stack) (TestContext* test_context) {
    stackCreate(&test_context->stack);
    return SUCCESS;
}

tekTestDelete(stack) (TestContext* test_context) {
    stackDelete(&test_context->stack);
    return SUCCESS;
}

tekTestFunc(stack, push_and_pop_items) (TestContext* test_context) {
    int a = 10, b = 20, c = 30;

    // push a few items on
    tekChainThrow(stackPush(&test_context->stack, &a));
    tekChainThrow(stackPush(&test_context->stack, &b));
    tekChainThrow(stackPush(&test_context->stack, &c));

    // make sure length looks right
    tekAssert(3, test_context->stack.length);

    // pop everything
    int* value;
    tekChainThrow(stackPop(&test_context->stack, (void**)&value));
    tekAssert(&c, value);

    tekChainThrow(stackPop(&test_context->stack, (void**)&value));
    tekAssert(&b, value);

    tekChainThrow(stackPop(&test_context->stack, (void**)&value));
    tekAssert(&a, value);

    // should be empty now
    tekAssert(0, test_context->stack.length);
    return SUCCESS;
}

tekTestFunc(stack, peek_item) (TestContext* test_context) {
    int first = 99;
    tekChainThrow(stackPush(&test_context->stack, &first));

    // peek
    int* peeked = NULL;
    tekChainThrow(stackPeek(&test_context->stack, (void**)&peeked));
    tekAssert(&first, peeked);

    // stack length should be untouched
    tekAssert(1, test_context->stack.length);
    return SUCCESS;
}

tekTestCreate(queue) (TestContext* test_context) {
    queueCreate(&test_context->queue);
    return SUCCESS;
}

tekTestDelete(queue) (TestContext* test_context) {
    queueDelete(&test_context->queue);
    return SUCCESS;
}

tekTestFunc(stack, boundary_and_invalid_tests) (TestContext* test_context) {
    void* out = NULL;

    // boundary - peek on empty stack (should error)
    exception tek_exception = stackPeek(&test_context->stack, &out);
    tekAssert(STACK_EXCEPTION, tek_exception);

    // boundary - pop on empty stack
    tek_exception = stackPop(&test_context->stack, &out);
    tekAssert(STACK_EXCEPTION, tek_exception);

    int value = 123;
    tekChainThrow(stackPush(&test_context->stack, &value));

    return SUCCESS;
}

tekTestFunc(queue, enqueue_and_dequeue_items) (TestContext* test_context) {
    int a = 10, b = 20, c = 30;

    // shove some items into the queue
    tekChainThrow(queueEnqueue(&test_context->queue, &a));
    tekChainThrow(queueEnqueue(&test_context->queue, &b));
    tekChainThrow(queueEnqueue(&test_context->queue, &c));

    // make sure length looks right
    tekAssert(3, test_context->queue.length);

    // dequeue them
    int* value;
    tekChainThrow(queueDequeue(&test_context->queue, (void**)&value));
    tekAssert(&a, value);

    tekChainThrow(queueDequeue(&test_context->queue, (void**)&value));
    tekAssert(&b, value);

    tekChainThrow(queueDequeue(&test_context->queue, (void**)&value));
    tekAssert(&c, value);

    // queue should be empty now
    tekAssert(0, test_context->queue.length);
    tekAssert(1, queueIsEmpty(&test_context->queue));
    return SUCCESS;
}

tekTestFunc(queue, peek_item) (TestContext* test_context) {
    int first = 99;
    tekChainThrow(queueEnqueue(&test_context->queue, &first));

    // have a peek without touching it
    int* peeked = NULL;
    tekChainThrow(queuePeek(&test_context->queue, (void**)&peeked));
    tekAssert(&first, peeked);

    // make sure it didn't secretly dequeue it
    tekAssert(1, test_context->queue.length);
    return SUCCESS;
}

tekTestFunc(queue, is_empty_check) (TestContext* test_context) {
    // should start empty
    tekAssert(1, queueIsEmpty(&test_context->queue));

    int val = 42;
    tekChainThrow(queueEnqueue(&test_context->queue, &val));
    tekAssert(0, queueIsEmpty(&test_context->queue));

    // pop it back off
    int* popped = NULL;
    tekChainThrow(queueDequeue(&test_context->queue, (void**)&popped));

    // should be empty again
    tekAssert(1, queueIsEmpty(&test_context->queue));
    return SUCCESS;
}

tekTestFunc(queue, boundary_and_invalid_tests) (TestContext* test_context) {
    void* out = NULL;

    // boundary - peek on empty queue (should blow up politely)
    exception tek_exception = queuePeek(&test_context->queue, &out);
    tekAssert(QUEUE_EXCEPTION, tek_exception);

    // boundary - dequeue on empty queue
    tek_exception = queueDequeue(&test_context->queue, &out);
    tekAssert(QUEUE_EXCEPTION, tek_exception);

    return SUCCESS;
}

tekTestCreate(priority_queue) (TestContext* test_context) {
    priorityQueueCreate(&test_context->priority_queue);
    return SUCCESS;
}

tekTestDelete(priority_queue) (TestContext* test_context) {
    priorityQueueDelete(&test_context->priority_queue);
    return SUCCESS;
}

tekTestFunc(priority_queue, enqueue_and_dequeue_items) (TestContext* test_context) {
    int a = 10, b = 20, c = 30;

    // shove some items in with priorities
    tekChainThrow(priorityQueueEnqueue(&test_context->priority_queue, 3.0, &a));
    tekChainThrow(priorityQueueEnqueue(&test_context->priority_queue, 1.0, &b));
    tekChainThrow(priorityQueueEnqueue(&test_context->priority_queue, 2.0, &c));

    // make sure length is right
    tekAssert(3, test_context->priority_queue.length);

    // dequeue in correct order (lowest number = highest priority)
    int* value;
    tekAssert(0, priorityQueueIsEmpty(&test_context->priority_queue));

    tekAssert(1, priorityQueueDequeue(&test_context->priority_queue, (void**)&value));
    tekAssert(&b, value); // priority 1.0

    tekAssert(1, priorityQueueDequeue(&test_context->priority_queue, (void**)&value));
    tekAssert(&c, value); // priority 2.0

    tekAssert(1, priorityQueueDequeue(&test_context->priority_queue, (void**)&value));
    tekAssert(&a, value); // priority 3.0

    // should be empty now
    tekAssert(0, test_context->priority_queue.length);
    tekAssert(1, priorityQueueIsEmpty(&test_context->priority_queue));
    return SUCCESS;
}

tekTestFunc(priority_queue, peek_item) (TestContext* test_context) {
    int low = 5, high = 999;
    tekChainThrow(priorityQueueEnqueue(&test_context->priority_queue, 1.0, &low));
    tekChainThrow(priorityQueueEnqueue(&test_context->priority_queue, 50.0, &high));

    // have a play (should get low since it's got better priority)
    int* peeked = NULL;
    tekAssert(1, priorityQueuePeek(&test_context->priority_queue, (void**)&peeked));
    tekAssert(&low, peeked);

    // make sure it didn't bugger up
    tekAssert(2, test_context->priority_queue.length);
    return SUCCESS;
}

tekTestFunc(priority_queue, boundary_and_invalid_tests) (TestContext* test_context) {
    void* out = NULL;

    // boundary - peek on empty queue
    tekAssert(0, priorityQueuePeek(&test_context->priority_queue, &out));

    // boundary - dequeue on empty queue
    tekAssert(0, priorityQueueDequeue(&test_context->priority_queue, &out));

    return SUCCESS;
}

tekTestFunc(priority_queue, is_empty_check) (TestContext* test_context) {
    // should start empty
    tekAssert(1, priorityQueueIsEmpty(&test_context->priority_queue));

    int v = 123;
    tekChainThrow(priorityQueueEnqueue(&test_context->priority_queue, 1.0, &v));
    tekAssert(0, priorityQueueIsEmpty(&test_context->priority_queue));

    // dequeue the thing
    int* out = NULL;
    tekAssert(1, priorityQueueDequeue(&test_context->priority_queue, (void**)&out));
    tekAssert(1, priorityQueueIsEmpty(&test_context->priority_queue));

    return SUCCESS;
}

tekTestCreate(bitset) (TestContext* test_context) {
    tekChainThrow(bitsetCreate(64, 1, &test_context->bitset));
    return SUCCESS;
}

tekTestDelete(bitset) (TestContext* test_context) {
    bitsetDelete(&test_context->bitset);
    return SUCCESS;
}

tekTestFunc(bitset, set_and_get_bits) (TestContext* test_context) {
    flag value;

    // set a few bits
    tekChainThrow(bitsetSet(&test_context->bitset, 0));
    tekChainThrow(bitsetSet(&test_context->bitset, 63));
    tekChainThrow(bitsetSet(&test_context->bitset, 64));
    tekChainThrow(bitsetSet(&test_context->bitset, 127));

    // confirm they got set properly
    tekChainThrow(bitsetGet(&test_context->bitset, 0, &value));
    tekAssert(1, value);

    tekChainThrow(bitsetGet(&test_context->bitset, 63, &value));
    tekAssert(1, value);

    tekChainThrow(bitsetGet(&test_context->bitset, 64, &value));
    tekAssert(1, value);

    tekChainThrow(bitsetGet(&test_context->bitset, 127, &value));
    tekAssert(1, value);

    // confirm that bits that weren't set are defaulted to 0
    tekChainThrow(bitsetGet(&test_context->bitset, 14, &value));
    tekAssert(0, value);

    tekChainThrow(bitsetGet(&test_context->bitset, 120, &value));
    tekAssert(0, value);

    // unset one and make sure it's unset
    tekChainThrow(bitsetUnset(&test_context->bitset, 64));
    tekChainThrow(bitsetGet(&test_context->bitset, 64, &value));
    tekAssert(0, value);

    return SUCCESS;
}

tekTestFunc(bitset, boundary_and_invalid_tests) (TestContext* test_context) {
    // i want a non growing bitset for this test.
    bitsetDelete(&test_context->bitset);
    tekChainThrow(bitsetCreate(64, 0, &test_context->bitset));

    flag value;

    // valid boundary
    tekChainThrow(bitsetSet(&test_context->bitset, 63));
    tekChainThrow(bitsetGet(&test_context->bitset, 63, &value));
    tekAssert(1, value);

    // invalid - one past the end (should blow up)
    const exception tek_exception = bitsetSet(&test_context->bitset, 64);
    tekAssert(BITSET_EXCEPTION, tek_exception);

    return SUCCESS;
}

tekTestFunc(bitset, clear_test) (TestContext* test_context) {
    // set a few random bits
    tekChainThrow(bitsetSet(&test_context->bitset, 3));
    tekChainThrow(bitsetSet(&test_context->bitset, 17));
    tekChainThrow(bitsetSet(&test_context->bitset, 31));

    // nuke the whole thing
    bitsetClear(&test_context->bitset);

    flag value;
    for (uint i = 0; i < 32; i++) {
        tekChainThrow(bitsetGet(&test_context->bitset, i, &value));
        tekAssert(0, value);
    }
    return SUCCESS;
}

tekTestFunc(bitset, grows_flag_test) (TestContext* test_context) {
    // make a non-growing one
    bitsetDelete(&test_context->bitset);
    tekChainThrow(bitsetCreate(64, 0, &test_context->bitset));

    // setting inside range should work
    tekChainThrow(bitsetSet(&test_context->bitset, 10));

    // this should bugger up since it can't grow
    exception tek_exception = bitsetSet(&test_context->bitset, 999);
    tekAssert(BITSET_EXCEPTION, tek_exception);

    // make a growing one and try again
    bitsetDelete(&test_context->bitset);
    tekChainThrow(bitsetCreate(64, 1, &test_context->bitset));

    tekChainThrow(bitsetSet(&test_context->bitset, 999)); // should be fine now
    flag value;
    tekChainThrow(bitsetGet(&test_context->bitset, 999, &value));
    tekAssert(1, value);
    return SUCCESS;
}

tekTestFunc(bitset, two_d_functions) (TestContext* test_context) {
    flag value;

    // set a few coordinates, this'll test the zig-zag conversion
    tekChainThrow(bitsetSet2D(&test_context->bitset, 0, 0));
    tekChainThrow(bitsetSet2D(&test_context->bitset, 3, 2));
    tekChainThrow(bitsetSet2D(&test_context->bitset, 15, 15));

    // read them back
    tekChainThrow(bitsetGet2D(&test_context->bitset, 0, 0, &value));
    tekAssert(1, value);

    tekChainThrow(bitsetGet2D(&test_context->bitset, 3, 2, &value));
    tekAssert(1, value);

    tekChainThrow(bitsetGet2D(&test_context->bitset, 15, 15, &value));
    tekAssert(1, value);

    // unset one and make sure it actually unsets
    tekChainThrow(bitsetUnset2D(&test_context->bitset, 3, 2));
    tekChainThrow(bitsetGet2D(&test_context->bitset, 3, 2, &value));
    tekAssert(0, value);

    return SUCCESS;
}

tekTestCreate(hashtable) (TestContext* test_context) {
    tekChainThrow(hashtableCreate(&test_context->hashtable, 1));
    return SUCCESS;
}

tekTestDelete(hashtable) (TestContext* test_context) {
    hashtableDelete(&test_context->hashtable);
    return SUCCESS;
}

tekTestFunc(hashtable, set_and_get_items) (TestContext* test_context) {
    int a = 100, b = 200, c = 300;
    tekChainThrow(hashtableSet(&test_context->hashtable, "alpha", &a));
    tekChainThrow(hashtableSet(&test_context->hashtable, "beta", &b));
    tekChainThrow(hashtableSet(&test_context->hashtable, "charlie", &c));

    // check retrieval
    void* out = NULL;
    tekChainThrow(hashtableGet(&test_context->hashtable, "alpha", &out));
    tekAssert(&a, out);

    tekChainThrow(hashtableGet(&test_context->hashtable, "beta", &out));
    tekAssert(&b, out);

    tekChainThrow(hashtableGet(&test_context->hashtable, "charlie", &out));
    tekAssert(&c, out);

    // overwrite one and make sure it updates instead of duplicating
    int new_c = 999;
    tekChainThrow(hashtableSet(&test_context->hashtable, "charlie", &new_c));
    tekChainThrow(hashtableGet(&test_context->hashtable, "charlie", &out));
    tekAssert(&new_c, out);

    return SUCCESS;
}

tekTestFunc(hashtable, remove_and_haskey_tests) (TestContext* test_context) {
    int a = 111, b = 222;
    tekChainThrow(hashtableSet(&test_context->hashtable, "foo", &a));
    tekChainThrow(hashtableSet(&test_context->hashtable, "bar", &b));

    // make sure it says it has them
    tekAssert(1, hashtableHasKey(&test_context->hashtable, "foo"));
    tekAssert(1, hashtableHasKey(&test_context->hashtable, "bar"));
    tekAssert(0, hashtableHasKey(&test_context->hashtable, "baz"));

    // remove one and check again
    tekChainThrow(hashtableRemove(&test_context->hashtable, "foo"));
    tekAssert(0, hashtableHasKey(&test_context->hashtable, "foo"));
    tekAssert(1, hashtableHasKey(&test_context->hashtable, "bar"));

    // try to remove something that doesn't exist (shouldn't bugger up)
    const exception tek_exception = hashtableRemove(&test_context->hashtable, "idontexist");
    tekAssert(FAILURE, tek_exception);

    return SUCCESS;
}

tekTestFunc(hashtable, get_keys_and_values) (TestContext* test_context) {
    int val1 = 10, val2 = 20, val3 = 30;
    tekChainThrow(hashtableSet(&test_context->hashtable, "one", &val1));
    tekChainThrow(hashtableSet(&test_context->hashtable, "two", &val2));
    tekChainThrow(hashtableSet(&test_context->hashtable, "three", &val3));

    char** keys = NULL;
    void** values = NULL;

    tekChainThrow(hashtableGetKeys(&test_context->hashtable, &keys));
    tekChainThrow(hashtableGetValues(&test_context->hashtable, &values));

    // make sure all keys are there somewhere
    flag got_one = 0, got_two = 0, got_three = 0;
    uint index_one = 0, index_two = 0, index_three = 0;
    for (uint i = 0; i < test_context->hashtable.num_items; i++) {
        if (!strcmp(keys[i], "one")) {
            got_one = 1;
            index_one = i;
        }
        else if (!strcmp(keys[i], "two")) {
            got_two = 1;
            index_two = i;
        }
        else if (!strcmp(keys[i], "three")) {
            got_three = 1;
            index_three = i;
        }
    }
    tekAssert(1, got_one);
    tekAssert(1, got_two);
    tekAssert(1, got_three);

    // check that the values match the ones we shoved in
    // need to cast to int pointer and dereference because we gave it int pointers to begin with.
    tekAssert(val1, *(int*)values[index_one]);
    tekAssert(val2, *(int*)values[index_two]);
    tekAssert(val3, *(int*)values[index_three]);

    // clean up what the function mallocated
    free(keys);
    free(values);
    return SUCCESS;
}

tekTestFunc(hashtable, boundary_and_invalid_tests) (TestContext* test_context) {
    void* out = NULL;

    // invalid - get a nonexistent key
    exception tek_exception = hashtableGet(&test_context->hashtable, "missing", &out);
    tekAssert(FAILURE, tek_exception);

    // boundary - empty string key
    tek_exception = hashtableSet(&test_context->hashtable, "", &out);
    tekAssert(SUCCESS, tek_exception);

    // boundary - null data
    tek_exception = hashtableSet(&test_context->hashtable, "empty", NULL);
    tekAssert(SUCCESS, tek_exception);

    return SUCCESS;
}

tekTestFunc(hashtable, overwrite_existing_key) (TestContext* test_context) {
    int a = 42;
    int b = 999;
    tekChainThrow(hashtableSet(&test_context->hashtable, "testkey", &a));

    void* out = NULL;
    tekChainThrow(hashtableGet(&test_context->hashtable, "testkey", &out));
    tekAssert(&a, out);

    // overwrite with a new pointer
    tekChainThrow(hashtableSet(&test_context->hashtable, "testkey", &b));
    tekChainThrow(hashtableGet(&test_context->hashtable, "testkey", &out));
    tekAssert(&b, out);

    return SUCCESS;
}

#define THREAD_QUEUE_ITEMS 256

typedef struct ThreadQueueTestData {
    ThreadQueue* queue;
    int produced[THREAD_QUEUE_ITEMS];
    int consumed[THREAD_QUEUE_ITEMS];
} ThreadQueueTestData;

static void* threadQueueProducer(void* arg) {
    ThreadQueueTestData* data = (ThreadQueueTestData*)arg;
    for (uint i = 0; i < THREAD_QUEUE_ITEMS; i++) {
        data->produced[i] = i * 5; // some obvious pattern
        while (!threadQueueEnqueue(data->queue, &data->produced[i])) {
            // queue full, spin until it's ready
        }
    }
    return NULL;
}

static void* threadQueueConsumer(void* arg) {
    ThreadQueueTestData* data = (ThreadQueueTestData*)arg;
    for (uint i = 0; i < THREAD_QUEUE_ITEMS; i++) {
        void* out = NULL;
        while (!threadQueueDequeue(data->queue, &out)) {
            // queue empty, spin until item appears
        }
        data->consumed[i] = *(int*)out;
    }
    return NULL;
}

tekTestCreate(thread_queue) (TestContext* test_context) {
    tekChainThrow(threadQueueCreate(&test_context->thread_queue, 32));
    return SUCCESS;
}

tekTestDelete(thread_queue) (TestContext* test_context) {
    threadQueueDelete(&test_context->thread_queue);
    return SUCCESS;
}

tekTestFunc(thread_queue, enqueue_and_dequeue_items) (TestContext* test_context) {
    // simple test to check enqueue/dequeue order
    int values[5] = { 10, 20, 30, 40, 50 };
    void* out = NULL;

    for (uint i = 0; i < 5; i++)
        tekAssert(1, threadQueueEnqueue(&test_context->thread_queue, &values[i]));

    for (uint i = 0; i < 5; i++) {
        tekAssert(1, threadQueueDequeue(&test_context->thread_queue, &out));
        tekAssert(values[i], *(int*)out);
    }

    tekAssert(1, threadQueueIsEmpty(&test_context->thread_queue));
    return SUCCESS;
}

tekTestFunc(thread_queue, multithread_transfer) (TestContext* test_context) {
    // test that the queue can actually handle multithreaded ops
    ThreadQueueTestData shared = { .queue = &test_context->thread_queue };

    pthread_t prod, cons;
    pthread_create(&prod, NULL, threadQueueProducer, &shared);
    pthread_create(&cons, NULL, threadQueueConsumer, &shared);

    pthread_join(prod, NULL);
    pthread_join(cons, NULL);

    for (uint i = 0; i < THREAD_QUEUE_ITEMS; i++)
        // silent assert to avoid putting 256 print statements
        tekSilentAssert(shared.produced[i], shared.consumed[i]);

    tekAssert(1, threadQueueIsEmpty(&test_context->thread_queue));
    return SUCCESS;
}

tekTestFunc(thread_queue, boundary_and_invalid_tests) (TestContext* test_context) {
    // boundary and bogus data tests
    void* out = NULL;
    int value = 999;

    // boundary - dequeue on empty queue
    tekAssert(0, threadQueueDequeue(&test_context->thread_queue, &out));

    // enqueue valid value
    tekAssert(1, threadQueueEnqueue(&test_context->thread_queue, &value));

    // peek should give us the front item
    tekAssert(1, threadQueuePeek(&test_context->thread_queue, &out));
    tekAssert(&value, out);

    // dequeue should return same pointer
    tekAssert(1, threadQueueDequeue(&test_context->thread_queue, &out));
    tekAssert(&value, out);

    return SUCCESS;
}

tekTestFunc(thread_queue, stress_test) (TestContext* test_context) {
    // stress test to see if it can keep up under load
    int values[1024];
    void* out = NULL;

    for (uint i = 0; i < 1024; i++) {
        values[i] = i;
        while (!threadQueueEnqueue(&test_context->thread_queue, &values[i])) {
            // spin if full
        }
        threadQueueDequeue(&test_context->thread_queue, &out);
        // silent assert to avoid printing 1024 lines
        tekSilentAssert(values[i], *(int*)out);
    }

    tekAssert(1, threadQueueIsEmpty(&test_context->thread_queue));
    return SUCCESS;
}

tekTestCreate(file) (TestContext* test_context) {
    return SUCCESS;
}

tekTestDelete(file) (TestContext* test_context) {
    return SUCCESS;
}


tekTestFunc(file, len_file) (TestContext* test_context) {
    const char* filename = "../tests/ten_chars.bin";

    uint len_file;
    tekChainThrow(getFileSize(filename, &len_file));

    // 10 characters + null terminator character
    // took ages to even make a file that has ten characters and doesnt automatically have a newline
    tekAssert(10 + 1, len_file);

    return SUCCESS;
}

tekTestFunc(file, read) (TestContext* test_context) {
    // read the file and make sure that the contents match what we expect
    const char* filename = "../tests/take_me_away.txt";
    const char* expected = 
        "Come with me to the dancefloor\n"
        "You and me, cause that's what it's for.\n"
        "Show me now what it is\n"
        "You've got to be doing\n"
        "'Cause the music in the house is so soothing.\n"
        "I wanna dance the night away, you see\n"
        "It's just a party and now come with me.\n"
        "Take me away...\n"
        "Take me away...\n";

    uint len_file;
    tekChainThrow(getFileSize(filename, &len_file));

    char* file = alloca(len_file * sizeof(char));
    if (!file)
        tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for file.");

    tekChainThrow(readFile(filename, len_file, file));

    tekAssert(0, strcmp(file, expected));

    return SUCCESS;
}

tekTestFunc(file, empty) (TestContext* test_context) {
    const char* filename = "../tests/empty.txt";

    uint len_file;
    tekChainThrow(getFileSize(filename, &len_file));

    // single null char
    tekAssert(1, len_file);

    char* file = alloca(len_file * sizeof(char));
    if (!file)
        tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for file.");

    tekChainThrow(readFile(filename, len_file, file));

    // make sure that the string is just the null char
    tekAssert(0, file[0]);

    return SUCCESS;
}

tekTestCreate(yml) (TestContext* test_context) {
    return SUCCESS;
}

tekTestDelete(yml) (TestContext* test_context) {
    ymlDelete(&test_context->yml);
    return SUCCESS;
}

tekTestFunc(yml, single_datatypes) (TestContext* test_context) {
    const char* filename = "../tests/all_datatypes.yml";
    tekChainThrow(ymlReadFile(filename, &test_context->yml));

    YmlData* yml_data;

    // testing string data, get data stored under the key "string" and convert
    char* string;
    tekChainThrow(ymlGet(&test_context->yml, &yml_data, "string"));
    tekChainThrow(ymlDataToString(yml_data, &string));
    tekAssert(0, strcmp(string, "A test string."));

    free(string);

    // testing integer data, get data stored under key "integer" and convert.
    long integer;
    tekChainThrow(ymlGet(&test_context->yml, &yml_data, "integer"));
    tekChainThrow(ymlDataToInteger(yml_data, &integer));
    tekAssert(12345, integer);

    // testing floating point data, get data stored under key "float" and convert.
    double floating;
    tekChainThrow(ymlGet(&test_context->yml, &yml_data, "float"));
    tekChainThrow(ymlDataToFloat(yml_data, &floating));

    tekAssert(0.12345, floating);

    return SUCCESS;
}

tekTestFunc(yml, list_datatypes) (TestContext* test_context) {
    const char* filename = "../tests/all_datatypes.yml";
    tekChainThrow(ymlReadFile(filename, &test_context->yml));

    YmlData* yml_data;

    // testing string list
    char* string;
    tekChainThrow(ymlGet(&test_context->yml, &yml_data, "string_list"));

    // check a couple items
    tekChainThrow(ymlListGetString(yml_data, 0, &string));
    tekAssert(0, strcmp("A test string 0", string));
    free(string);

    tekChainThrow(ymlListGetString(yml_data, 2, &string));
    tekAssert(0, strcmp("A test string 2", string));
    free(string);

    // testing integer list
    long integer;
    tekChainThrow(ymlGet(&test_context->yml, &yml_data, "integer_list"));

    // check a couple items
    tekChainThrow(ymlListGetInteger(yml_data, 0, &integer));
    tekAssert(0, integer);

    tekChainThrow(ymlListGetInteger(yml_data, 2, &integer));
    tekAssert(200, integer);

    // testing floating point list
    double floating;
    tekChainThrow(ymlGet(&test_context->yml, &yml_data, "float_list"));

    // check a couple items
    tekChainThrow(ymlListGetFloat(yml_data, 0, &floating));
    tekAssert(0.0, floating);

    tekChainThrow(ymlListGetFloat(yml_data, 1, &floating));
    tekAssert(0.1, floating);


    return SUCCESS;
}

tekTestFunc(yml, empty) (TestContext* test_context) {
    const char* filename = "../tests/empty.yml";
    exception tek_exception = ymlReadFile(filename, &test_context->yml);
    tekAssert(SUCCESS, tek_exception);

    // make sure that we cant actually get anything out cuz it should be empty
    YmlData* yml_data;
    tek_exception = ymlGet(&test_context->yml, &yml_data, "anything", "whatsoever");
    tekAssert(tek_exception, YML_EXCEPTION);

    return SUCCESS;
}

tekTestFunc(yml, typical) (TestContext* test_context) {
    // bogstandard yml file, has some "additions" where people have used different indents
    const char* filename = "../tests/typical.yml";
    exception tek_exception = ymlReadFile(filename, &test_context->yml);
    tekAssert(SUCCESS, tek_exception);

    YmlData* yml_data;

    // testing double indentation
    double volume;
    tekChainThrow(ymlGet(&test_context->yml, &yml_data, "properties", "volume"));
    tekChainThrow(ymlDataToFloat(yml_data, &volume));
    tekAssert(434.1, volume);

    // testing triple indentation
    double x;
    tekChainThrow(ymlGet(&test_context->yml, &yml_data, "properties", "position", "x"));
    tekChainThrow(ymlDataToFloat(yml_data, &x));
    tekAssert(10.0, x);

    // testing list
    long* neighbours;
    uint len_neighbours;
    tekChainThrow(ymlGet(&test_context->yml, &yml_data, "properties", "neighbours"));
    tekChainThrow(ymlListToIntegerArray(yml_data, &neighbours, &len_neighbours));

    tekAssert(3, len_neighbours);
    tekAssert(4121, neighbours[0]);
    tekAssert(1123, neighbours[1]);
    tekAssert(3435, neighbours[2]);

    // testing with different indentation
    tek_exception = ymlGet(&test_context->yml, &yml_data, "addition", "new_indent");
    tekAssert(SUCCESS, tek_exception);

    tek_exception = ymlGet(&test_context->yml, &yml_data, "addition2", "test", "random_indent");
    tekAssert(SUCCESS, tek_exception);

    return SUCCESS;
}

tekTestFunc(yml, syntax_errors) (TestContext* test_context) {
    // bad indentation
    tekAssert(YML_EXCEPTION, ymlReadFile("../tests/bad_indent.yml", &test_context->yml));

    // two duplicated keys
    tekAssert(YML_EXCEPTION, ymlReadFile("../tests/duplicate_key.yml", &test_context->yml));

    // space in key
    tekAssert(YML_EXCEPTION, ymlReadFile("../tests/key_space.yml", &test_context->yml));

    return SUCCESS;
}

exception tekUnitTest() {
    TestContext test_context = {};

    // vector
    tekRunSuite(vector, add_one_int, &test_context);
    tekRunSuite(vector, resizing, &test_context);
    tekRunSuite(vector, add_ten_ints, &test_context);
    tekRunSuite(vector, get_an_int, &test_context);
    tekRunSuite(vector, get_an_int_ptr, &test_context);
    tekRunSuite(vector, set_an_int, &test_context);
    tekRunSuite(vector, insert_an_int, &test_context);
    tekRunSuite(vector, remove_an_int, &test_context);
    tekRunSuite(vector, pop_an_int, &test_context);
    tekRunSuite(vector, clear_vector, &test_context);
    tekRunSuite(vector, boundary_index_tests, &test_context);
    tekRunSuite(vector, invalid_create_tests, &test_context);
    tekRunSuite(vector, big_block, &test_context);

    // list (linked list)
    tekRunSuite(list, add_and_get_items, &test_context);
    tekRunSuite(list, insert_and_set_items, &test_context);
    tekRunSuite(list, remove_and_pop_items, &test_context);
    tekRunSuite(list, move_item, &test_context);
    tekRunSuite(list, free_all_data, &test_context);
    tekRunSuite(list, boundary_and_invalid_tests, &test_context);

    // stack
    tekRunSuite(stack, push_and_pop_items, &test_context);
    tekRunSuite(stack, peek_item, &test_context);
    tekRunSuite(stack, boundary_and_invalid_tests, &test_context);

    // queue
    tekRunSuite(queue, enqueue_and_dequeue_items, &test_context);
    tekRunSuite(queue, is_empty_check, &test_context);
    tekRunSuite(queue, peek_item, &test_context);
    tekRunSuite(queue, boundary_and_invalid_tests, &test_context);

    // priority queue
    tekRunSuite(priority_queue, enqueue_and_dequeue_items, &test_context);
    tekRunSuite(priority_queue, peek_item, &test_context);
    tekRunSuite(priority_queue, boundary_and_invalid_tests, &test_context);
    tekRunSuite(priority_queue, is_empty_check, &test_context);

    // bitset
    tekRunSuite(bitset, set_and_get_bits, &test_context);
    tekRunSuite(bitset, boundary_and_invalid_tests, &test_context);
    tekRunSuite(bitset, clear_test, &test_context);
    tekRunSuite(bitset, grows_flag_test, &test_context);
    tekRunSuite(bitset, two_d_functions, &test_context);

    // hashtable
    tekRunSuite(hashtable, set_and_get_items, &test_context);
    tekRunSuite(hashtable, remove_and_haskey_tests, &test_context);
    tekRunSuite(hashtable, get_keys_and_values, &test_context);
    tekRunSuite(hashtable, boundary_and_invalid_tests, &test_context);
    tekRunSuite(hashtable, overwrite_existing_key, &test_context);

    // thread queue
    tekRunSuite(thread_queue, enqueue_and_dequeue_items, &test_context);
    tekRunSuite(thread_queue, multithread_transfer, &test_context);
    tekRunSuite(thread_queue, boundary_and_invalid_tests, &test_context);
    tekRunSuite(thread_queue, stress_test, &test_context);

    // file
    tekRunSuite(file, len_file, &test_context);
    tekRunSuite(file, read, &test_context);
    tekRunSuite(file, empty, &test_context);

    // yml file
    tekRunSuite(yml, single_datatypes, &test_context);
    tekRunSuite(yml, list_datatypes, &test_context);
    tekRunSuite(yml, empty, &test_context);
    tekRunSuite(yml, typical, &test_context);
    tekRunSuite(yml, syntax_errors, &test_context);

    return SUCCESS;
}
