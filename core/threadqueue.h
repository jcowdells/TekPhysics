#pragma once

#include <stdatomic.h>
#include <semaphore.h>

#include "../tekgl.h"
#include "exception.h"

typedef struct ThreadQueue {
    void** buffer;
    uint buffer_size;
    atomic_uint front;
    atomic_uint rear;
} ThreadQueue;

exception threadQueueCreate(ThreadQueue* thread_queue, uint capacity);
void threadQueueDelete(ThreadQueue* thread_queue);
flag threadQueueEnqueue(ThreadQueue* thread_queue, void* data);
flag threadQueueDequeue(ThreadQueue* thread_queue, void** data);
flag threadQueuePeek(ThreadQueue* thread_queue, void** data);
flag threadQueueIsEmpty(ThreadQueue* thread_queue);
