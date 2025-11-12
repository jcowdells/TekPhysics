#pragma once

#include "../tekgl.h"
#include "exception.h"

typedef struct QueueItem {
    void* data;
    struct QueueItem* next;
} QueueItem;

typedef struct Queue {
    QueueItem* front;
    QueueItem* rear;
    uint length;
} Queue;

void queueCreate(Queue* queue);
void queueDelete(Queue* queue);
exception queueEnqueue(Queue* queue, void* data);
exception queueDequeue(Queue* queue, void** data);
exception queuePeek(const Queue* queue, void** data);
flag queueIsEmpty(const Queue* queue);