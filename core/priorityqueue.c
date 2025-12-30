#include "priorityqueue.h"

#include <stdlib.h>

/**
 * @brief Initialise a priority queue struct.
 * @note This function requires the priority queue struct to be allocated already.
 * @param queue A pointer to an empty queue.
 */
void priorityQueueCreate(PriorityQueue* queue) {
    // default values
    queue->queue = 0;
    queue->length = 0;
}

/**
 * @brief Free all memory that a priority queue has allocated.
 * @param queue The priority queue to free.
 */
void priorityQueueDelete(PriorityQueue* queue) {
    PriorityQueueItem* item = queue->queue;
    while (item) {
        // essentially, go to each item starting from the front, store the item before it, then free itself.
        PriorityQueueItem* prev = item->next;
        free(item);
        item = prev;
    }
    // zero the struct to avoid misuse of freed pointers.
    queue->queue = 0;
    queue->length = 0;
}

/**
 * Enqueue a piece of data into the priority queue. Data with the lowest priority value will be dequeued first.
 * @param queue The priority queue to enqueue into.
 * @param priority The priority value given to the data.
 * @param data A pointer to the data to be stored.
 * @throws MEMORY_EXCEPTION if a new node for the queue could not be created.
 */
exception priorityQueueEnqueue(PriorityQueue* queue, const double priority, void* data) {
    PriorityQueueItem* new_item = (PriorityQueueItem*)malloc(sizeof(PriorityQueueItem));
    if (!new_item)
        tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for priority queue item.");
    new_item->priority = priority;
    new_item->data = data;
    new_item->next = 0;

    queue->length++;

    // if the queue is uninitialised, then this is the first item, so must be the front of the list.
    if (!queue->queue) {
        queue->queue = new_item;
        return SUCCESS;
    }

    // if the priority is smaller than the first item, this now becomes the first item.
    if (priority < queue->queue->priority) {
        new_item->next = queue->queue;
        queue->queue = new_item;
        return SUCCESS;
    }

    // otherwise, iterate over the list to find the first item that has a bigger priority than the new item.
    // or if no item is found, give the last item in the queue.
    PriorityQueueItem* item = queue->queue;
    while (item->next) {
        if (priority < item->next->priority)
            break;
        item = item->next;
    }

    // insert item into the queue at this point.
    new_item->next = item->next;
    item->next = new_item;
    return SUCCESS;
}

/**
 * Dequeue a piece of data from the front of the priority queue.
 * @param queue The queue to dequeue from.
 * @param data A pointer to where the data can be stored. Will be set to 0 if the queue is empty.
 * @return 0 if the queue is empty, 1 otherwise.
 */
flag priorityQueueDequeue(PriorityQueue* queue, void** data) {
    // if no front pointer, queue is empty.
    if (!queue->queue) {
        *data = 0;
        return 0;
    }

    // shift everything in queue forwards and free the old front.
    PriorityQueueItem* front = queue->queue;
    *data = front->data;
    queue->queue = front->next;
    free(front);

    queue->length--;

    return 1;
}

/**
 * Peek an item at the front of the priority queue.
 * @param queue The queue to peek into.
 * @param data The data that was found at the front.
 * @return 0 if the queue is empty, 1 otherwise.
 */
flag priorityQueuePeek(const PriorityQueue* queue, void** data) {
    if (!queue->queue) { // if queue empty, cannot peek any data
        *data = 0;
        return 0;
    }
    *data = queue->queue->data; // get data at the front of the queue.
    return 1;
}

/**
 * @brief Check whether a priority queue is empty.
 * @param queue The priority queue to check.
 * @return 0 if the queue is empty, 1 if it is not empty.
 */
flag priorityQueueIsEmpty(const PriorityQueue* queue) {
    // if there is no front of the queue, then the queue must be empty.
    return queue->queue ? 0 : 1;
}