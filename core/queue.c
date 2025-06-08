#include "queue.h"

#include <stdlib.h>

/**
 * @brief Initialise a queue struct.
 * @note This function requires the queue struct to be allocated already.
 * @param queue A pointer to an empty queue.
 */
void queueCreate(Queue* queue) {
    queue->rear = 0;
    queue->front = 0;
    queue->length = 0;
}

/**
 * @brief Free all memory that a queue has allocated.
 * @param queue The queue to free.
 */
void queueDelete(Queue* queue) {
    QueueItem* item = queue->front;
    while (item) {
        // essentially, go to each item starting from the front, store the item before it, then free itself.
        QueueItem* prev = item->prev;
        free(item);
        item = prev;
    }
    // zero the struct to avoid misuse of freed pointers.
    queue->rear = 0;
    queue->front = 0;
    queue->length = 0;
}

/**
 * @brief Add a piece of data to the queue.
 * @param queue The queue to enqueue into.
 * @param data The data to enqueue.
 * @throws MEMORY_EXCEPTION if memory could not be allocated for the queue.
 */
exception queueEnqueue(Queue* queue, void* data) {
    // we are going to overwrite the last item of the queue.
    // so make sure to store what was there before we overwrite it.
    QueueItem* item = queue->rear;
    queue->rear = (QueueItem*)malloc(sizeof(QueueItem));
    if (!queue->rear)
        tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for queue.");
    queue->rear->data = data;

    // as this is the last item, there is nothing before it in the queue.
    queue->rear->prev = 0;

    // depending on whether there was a last item before we added this one, we need to act differently.
    if (item) {
        // if we have changed the last item, then the old last item has now got a previous item to point to, which is this new one.
        item->prev = queue->rear;
    } else {
        //if there wasn't a last item before, this new item is also the first item (as it is the only item right now)
        queue->front = queue->rear;
    }
    queue->length++;
    return SUCCESS;
}

/**
 * @brief Retrieve the data stored at the front of the queue, and remove it from the queue.
 * @param queue The queue to dequeue from.
 * @param data A pointer to where the retrieved data should be stored.
 * @throws QUEUE_EXCEPTION if the queue is empty.
 */
exception queueDequeue(Queue* queue, void** data) {
    // if there is nothing at the front, then we can't dequeue
    if (!queue->front)
        tekThrow(QUEUE_EXCEPTION, "Cannot dequeue from an empty queue.");
    *data = queue->front->data;

    // set the front to the item before the old front.
    QueueItem* prev = queue->front->prev;
    free(queue->front);
    queue->front = prev;

    // if there is no longer a front of the queue, then that means we cleared the last item.
    if (!queue->front) queue->rear = 0;
    queue->length--;
    return SUCCESS;
}

/**
 * @brief Retrieve the data stored at the front of the queue.
 * @param queue The queue to be peeked.
 * @param data A pointer to where the retrieved data should be stored.
 * @throws QUEUE_EXCEPTION if the queue is empty.
 */
exception queuePeek(const Queue* queue, void** data) {
    if (!queue->front)
        tekThrow(QUEUE_EXCEPTION, "Cannot peek an empty queue.");
    *data = queue->front->data;
    return SUCCESS;
}

/**
 * @brief Check whether a queue is empty.
 * @param queue The queue to check.
 * @return 0 if the queue is empty, 1 if it is not empty.
 */
flag queueIsEmpty(const Queue* queue) {
    // if there is no front of the queue, then the queue must be empty.
    return queue->front ? 0 : 1;
}