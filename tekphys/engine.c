#include "engine.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#include "body.h"

#define BILLION 1000000000

#define RECV_FUNC(func_name, func_type, param_name) \
exception func_name(ThreadQueue* queue, func_type* param_name) { \
    func_type* copy; \
    tekChainThrow(threadQueueDequeue(queue, &copy)); \
    memcpy(param_name, copy, sizeof(func_type)); \
    free(copy); \
    return SUCCESS; \
} \

RECV_FUNC(recvEvent, TekEvent, event);
RECV_FUNC(recvState, TekState, state);

#define PUSH_FUNC(func_name, func_type, param_name) \
exception func_name(ThreadQueue* queue, const func_type param_name) { \
    func_type* copy = (func_type*)malloc(sizeof(func_type)); \
    if (!copy) tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for thread queue"); \
    memcpy(copy, &param_name, sizeof(func_type)); \
    tekChainThrow(threadQueueEnqueue(queue, copy)); \
    return SUCCESS; \
} \

PUSH_FUNC(pushState, TekState, state);
PUSH_FUNC(pushEvent, TekEvent, event);

void thread_print(ThreadQueue* state_queue, const char* format, ...) {
    char temp_write;
    va_list va;

    va_start(va, format);
    const int len_message = vsnprintf(&temp_write, 0, format, va);
    va_end(va);

    char* buffer = (char*)malloc(len_message * sizeof(char));
    if (!buffer) return;

    va_start(va, format);
    vsprintf(buffer, format, va);
    va_end(va);

    TekState message_state = {};
    message_state.type = MESSAGE_STATE;
    message_state.object_id = 0;
    message_state.data.message = buffer;
    pushState(state_queue, message_state);
}

#define tprint(format, ...) thread_print(state_queue, format, __VA_ARGS__)

void thread_except(ThreadQueue* state_queue, const uint exception) {
    TekState exception_state = {};
    exception_state.type = EXCEPTION_STATE;
    exception_state.object_id = 0;
    exception_state.data.exception = exception;
    pushState(state_queue, exception_state);
}

#define threadThrow(exception_code, exception_message) { const exception __thread_exception = exception_code; if (__thread_exception) { tekSetException(__thread_exception, __LINE__, __FUNCTION__, __FILE__, exception_message); thread_except(state_queue, __thread_exception); return; } }
#define threadChainThrow(exception_code) { const exception __thread_exception = exception_code; if (__thread_exception) { tekTraceException(__LINE__, __FUNCTION__, __FILE__); thread_except(state_queue, __thread_exception); return; } }

exception tekEngineCreateBody(ThreadQueue* state_queue, const char* mesh_filename, const char* material_filename, TekBody* body) {
    tekChainThrow(tekCreateBody(mesh_filename, body));

    const uint len_mesh = strlen(mesh_filename) + 1;
    const uint len_material = strlen(material_filename) + 1;
    char* mesh_copy = (char*)malloc(len_mesh);
    if (!mesh_copy) tekThrowThen(MEMORY_EXCEPTION, "Failed to allocate memory to copy mesh filename.", {
        tekDeleteBody(body);
    });
    char* material_copy = (char*)malloc(len_material);
    if (!material_copy) tekThrowThen(MEMORY_EXCEPTION, "Failed to allocate memory to copy material filename.", {
        tekDeleteBody(body);
        free(mesh_copy);
    });
    memcpy(mesh_copy, mesh_filename, len_mesh);
    memcpy(material_copy, material_filename, len_material);

    TekState state = {};
    state.data.entity.mesh_filename = mesh_filename;
    state.data.entity.material_filename = material_filename;
    tekChainThrowThen(pushState(state_queue, state), {
        tekDeleteBody(body);
        free(mesh_copy);
        free(material_copy);
    });
    return SUCCESS;
}

struct TekEngineArgs {
    ThreadQueue* event_queue;
    ThreadQueue* state_queue;
    double phys_period;
};

void tekEngine(void* args) {
    const struct TekEngineArgs* engine_args = (struct TekEngineArgs*)args;
    ThreadQueue* event_queue = engine_args->event_queue;
    ThreadQueue* state_queue = engine_args->state_queue;
    const double phys_period = engine_args->phys_period;
    free(args);

    struct timespec engine_time, step_time;
    const double phys_period_s = floor(phys_period);
    step_time.tv_sec = (__time_t)phys_period_s;
    step_time.tv_nsec = (__syscall_slong_t)(1e9 * (phys_period - phys_period_s));
    clock_gettime(CLOCK_MONOTONIC, &engine_time);
    flag running = 1;
    uint counter = 0;
    while (running) {
        TekEvent event = {};
        while (recvEvent(event_queue, &event) == SUCCESS) {
            switch (event.type) {
            case QUIT_EVENT:
                running = 0;
                break;
            case KEY_EVENT:
                tprint("Key pressed: %d\n", event.data.key_input.key);
                break;
            default:
                break;
            }
        }

        engine_time.tv_sec += step_time.tv_sec;
        engine_time.tv_nsec += step_time.tv_nsec;

        while (engine_time.tv_nsec >= BILLION) {
            engine_time.tv_nsec -= BILLION;
            engine_time.tv_sec += 1;
        }

        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &engine_time, NULL);
    }
    printf("thread ended :(\n");
}

exception tekInitEngine(ThreadQueue* event_queue, ThreadQueue* state_queue, const double phys_period) {
    struct TekEngineArgs* engine_args = (struct TekEngineArgs*)malloc(sizeof(struct TekEngineArgs));
    engine_args->event_queue = event_queue;
    engine_args->state_queue = state_queue;
    engine_args->phys_period = phys_period;
    pthread_t engine_thread;
    pthread_create(&engine_thread, NULL, tekEngine, engine_args);
    return SUCCESS;
}