#include "threadqueue.h"

#include <stdlib.h>

/**
 * @brief Initialise a thread queue. Based on the principle of a lock-free circular queue.
 *
 * @note Single-consumer single-producer
 *
 * @param thread_queue A pointer to an existing but empty ThreadQueue struct.
 * @param capacity The capacity of the thread queue.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 */
exception threadQueueCreate(ThreadQueue* thread_queue, const uint capacity) {
    thread_queue->buffer = (void**)malloc(capacity * sizeof(void*));
    if (!thread_queue->buffer)
        tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory buffer for thread queue.");
    thread_queue->buffer_size = capacity;
    atomic_init(&thread_queue->front, 0);
    atomic_init(&thread_queue->rear, 0);
    return SUCCESS;
}

/**
 * @brief Delete a thread queue and free the buffer allocated.
 *
 * @note Only frees the thread queue's used memory, not that of stored pointers.
 *
 * @param thread_queue The thread queue to delete.
 */
void threadQueueDelete(ThreadQueue* thread_queue) {
    free(thread_queue->buffer);
    thread_queue->buffer = 0;
    thread_queue->buffer_size = 0;
}


exception threadQueueEnqueue(ThreadQueue* thread_queue, void* data) {
    uint head = atomic_load_explicit()
    return SUCCESS;
}

exception threadQueueDequeue(ThreadQueue* thread_queue, void** data) {

    return SUCCESS;
}

exception threadQueuePeek(ThreadQueue* thread_queue, void** data) {

    return SUCCESS;
}

flag threadQueueIsEmpty(ThreadQueue* thread_queue) {

    return 0;
}
