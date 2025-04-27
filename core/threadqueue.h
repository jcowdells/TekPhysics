#pragma once

#include <pthread.h>
#include <semaphore.h>
#include "queue.h"

typedef struct ThreadQueue {
    Queue queue;
    pthread_mutex_t mutex;
    sem_t semaphore;
} ThreadQueue;

exception threadQueueCreate(ThreadQueue* thread_queue);
void threadQueueDelete(ThreadQueue* thread_queue);
exception threadQueueEnqueue(ThreadQueue* thread_queue, void* data);
exception threadQueueDequeue(ThreadQueue* thread_queue, void** data);
exception threadQueuePeek(ThreadQueue* thread_queue, void** data);
flag threadQueueIsEmpty(ThreadQueue* thread_queue);