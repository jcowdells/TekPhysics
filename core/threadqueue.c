#include "threadqueue.h"

#include <stdio.h>
#include <stdlib.h>

/**
 * Initialise a thread queue. Based on the principle of a lock-free circular queue.
 * @note Single-consumer single-producer
 * @param thread_queue A pointer to an existing but empty ThreadQueue struct.
 * @param capacity The capacity of the thread queue.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 */
exception threadQueueCreate(ThreadQueue* thread_queue, const uint capacity) {
    // allocate memory for queue
    thread_queue->buffer = (void**)malloc(capacity * sizeof(void*));
    if (!thread_queue->buffer)
        tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory buffer for thread queue.");
    thread_queue->buffer_size = capacity;

    // atomic integer for front and rear
    atomic_init(&thread_queue->front, 0);
    atomic_init(&thread_queue->rear, 0);
    return SUCCESS;
}

/**
 * Delete a thread queue and free the buffer allocated.
 * @note Only frees the thread queue's used memory, not that of stored pointers.
 * @param thread_queue The thread queue to delete.
 */
void threadQueueDelete(ThreadQueue* thread_queue) {
    // free allocated memory
    free(thread_queue->buffer);

    // prevent misuse by setting things to null
    thread_queue->buffer = 0;
    thread_queue->buffer_size = 0;
}

/**
 * Enqueue a new item to the thread queue, storing a pointer.
 * @note Only to be used by a single producer thread.
 * @param thread_queue The thread queue to enqueue to.
 * @param data A pointer to store in the queue.
 * @returns 0 if the queue is already full, 1 if the operation was successful.
 */
flag threadQueueEnqueue(ThreadQueue* thread_queue, void* data) {
    // can use relaxed memory order here, thread using this "owns" the rear pointer, other thread should write after we are done.
    const uint rear = atomic_load_explicit(&thread_queue->rear, memory_order_relaxed);
    const uint next_rear = (rear + 1) % thread_queue->buffer_size;

    // need to use acquire here, should get front index after the other thread has finished with it.
    // we only need this to make sure the queue isn't full.
    const uint front = atomic_load_explicit(&thread_queue->front, memory_order_acquire);

    // if front == next_rear, this write would overwrite data in the queue
    if (front == next_rear) {
        return 0;
    }

    thread_queue->buffer[rear] = data;
    // release order means that subsequent access to rear must happen after this, so other thread has to have the updated rear ptr
    atomic_store_explicit(&thread_queue->rear, next_rear, memory_order_release);
    return 1;
}

/**
 * Dequeue from a thread queue, returning the stored pointer.
 * @note Only to be used by a single consumer thread.
 * @param thread_queue The thread queue to dequeue from.
 * @param data A pointer to a pointer that will recieve the dequeued data.
 * @returns 0 if the queue is empty, 1 if the operation was successful.
 */
flag threadQueueDequeue(ThreadQueue* thread_queue, void** data) {
    // dequeue side is the main controller of the front ptr, we can be relaxed because other thread is working around what we do with it.
    const uint front = atomic_load_explicit(&thread_queue->front, memory_order_relaxed);
    // however, we need to ensure that other thread is finished with rear ptr before we can use it
    const uint rear = atomic_load_explicit(&thread_queue->rear, memory_order_acquire);

    // if front == rear, the queue is empty
    if (front == rear) {
        return 0;
    }

    *data = thread_queue->buffer[front];
    // make sure that when front ptr is read next, it happens after it has been updated to the new size
    atomic_store_explicit(&thread_queue->front, (front + 1) % thread_queue->buffer_size, memory_order_release);
    return 1;
}

/**
 * Peek into a thread queue, returning the stored pointer.
 * @note Only to be used by a single consumer thread.
 * @param thread_queue The thread queue to peek.
 * @param data A pointer to a pointer that will recieve the peeked data.
 * @returns 0 if the queue is empty, 1 if the operation was successful.
 */
flag threadQueuePeek(ThreadQueue* thread_queue, void** data) {
    // dequeue side is the main controller of the front ptr, we can be relaxed because other thread is working around what we do with it.
    const uint front = atomic_load_explicit(&thread_queue->front, memory_order_relaxed);
    // however, we need to ensure that other thread is finished with rear ptr before we can use it
    const uint rear = atomic_load_explicit(&thread_queue->rear, memory_order_acquire);

    // if front == rear, the queue is empty
    if (front == rear) {
        return 0;
    }

    *data = thread_queue->buffer[front];
    return 1;
}

/**
 * Determine whether a queue is empty.
 * @note Only to be used by a single consumer thread.
 * @param thread_queue The thread queue to check.
 * @returns 1 if the queue is empty, 0 if not.
 */
flag threadQueueIsEmpty(ThreadQueue* thread_queue) {
    // dequeue side is the main controller of the front ptr, we can be relaxed because other thread is working around what we do with it.
    const uint front = atomic_load_explicit(&thread_queue->front, memory_order_relaxed);
    // however, we need to ensure that other thread is finished with rear ptr before we can use it
    const uint rear = atomic_load_explicit(&thread_queue->rear, memory_order_acquire);

    return front == rear;
}
