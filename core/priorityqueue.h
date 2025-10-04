#pragma once

#include "../tekgl.h"
#include "exception.h"

typedef struct PriorityQueueItem {
    double priority;
    void* data;
    struct PriorityQueueItem* next;
} PriorityQueueItem;

typedef struct PriorityQueue {
    PriorityQueueItem* queue;
    uint length;
} PriorityQueue;

void priorityQueueCreate(PriorityQueue* queue);
void priorityQueueDelete(PriorityQueue* queue);
exception priorityQueueEnqueue(PriorityQueue* queue, double priority, void* data);
flag priorityQueueDequeue(PriorityQueue* queue, void** data);
flag priorityQueuePeek(const PriorityQueue* queue, void** data);
flag priorityQueueIsEmpty(const PriorityQueue* queue);
