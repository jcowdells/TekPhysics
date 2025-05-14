#include "threadqueue.h"

#include <errno.h>

#define threadQueueMutexLock(thread_queue) if (pthread_mutex_lock(&thread_queue->mutex)) tekThrow(THREAD_EXCEPTION, "Error while waiting for mutex to lock.")
#define threadQueueMutexUnlock(thread_queue) if (pthread_mutex_unlock(&thread_queue->mutex)) tekThrow(THREAD_EXCEPTION, "Failed to unlock mutex.")

#define threadQueueSemPost(thread_queue) if (sem_post(&thread_queue->semaphore)) tekThrow(THREAD_EXCEPTION, "Failed to post semaphore.")
#define threadQueueSemTryWait(thread_queue) if (sem_trywait(&thread_queue->semaphore) == -1) { \
    int __error_code = errno; \
    switch (__error_code) { \
    case EAGAIN: \
        printf("EAGAIN\n"); \
        break; \
    case EDEADLK: \
        printf("EDEADLK\n"); \
        break; \
    case EINTR: \
        printf("EINTR\n"); \
        break; \
    case EINVAL: \
        printf("EINVAL\n"); \
        break; \
    default: \
        printf("Other Error: %d\n", __error_code); \
        break; \
    } \
    tekThrow(THREAD_EXCEPTION, "Failed to try to wait for semaphore."); \
} \

#define threadQueueSemWait(thread_queue) if (sem_wait(&thread_queue->semaphore)) tekThrow(THREAD_EXCEPTION, "Failed to wait for semaphore.")

#define threadQueueMutexWrap(thread_queue, method) \
threadQueueMutexLock(thread_queue); \
method; \
threadQueueMutexUnlock(thread_queue); \

exception threadQueueCreate(ThreadQueue* thread_queue) {
    if (pthread_mutex_init(&thread_queue->mutex, NULL))
        tekThrow(THREAD_EXCEPTION, "Failed to initialise mutex.");
    if (sem_init(&thread_queue->semaphore, 0, 0))
        tekThrow(THREAD_EXCEPTION, "Failed to initialise semaphore.");
    queueCreate(&thread_queue->queue);
    return SUCCESS;
}

void threadQueueDelete(ThreadQueue* thread_queue) {
    pthread_mutex_destroy(&thread_queue->mutex);
    sem_destroy(&thread_queue->semaphore);
    queueDelete(&thread_queue->queue);
}

exception threadQueueEnqueue(ThreadQueue* thread_queue, void* data) {
    threadQueueMutexWrap(thread_queue, {
        tekChainThrow(queueEnqueue(&thread_queue->queue, data));
    });
    threadQueueSemPost(thread_queue);
    return SUCCESS;
}

exception threadQueueDequeueX(ThreadQueue* thread_queue, void** data) {
    threadQueueSemTryWait(thread_queue);
    threadQueueMutexWrap(thread_queue, {
        tekChainThrow(queueDequeue(&thread_queue->queue, data));
    });
    return SUCCESS;
}

exception threadQueueDequeueY(ThreadQueue* thread_queue, void** data, int line, const char* function, const char* file) {
    printf("%d %s %s\n", line, function, file);
    return threadQueueDequeueX(thread_queue, data);
}

exception threadQueuePeek(ThreadQueue* thread_queue, void** data) {
    threadQueueSemTryWait(thread_queue);
    threadQueueMutexWrap(thread_queue, {
        tekChainThrow(queuePeek(&thread_queue->queue, data));
    });
    return SUCCESS;
}

flag threadQueueIsEmpty(ThreadQueue* thread_queue) {
    threadQueueSemTryWait(thread_queue);
    if (pthread_mutex_lock(&thread_queue->mutex)) return 1;
    const flag empty = queueIsEmpty(&thread_queue->queue);
    if (pthread_mutex_unlock(&thread_queue->mutex)) return 1;
    return empty;
}
