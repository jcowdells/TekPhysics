#include "engine.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <math.h>

#include "body.h"
#include "../core/vector.h"
#include "../core/queue.h"
#include "GLFW/glfw3.h"

#define BILLION 1000000000

#define MOUSE_SENSITIVITY 0.001f
#define MOVE_SPEED        0.01f

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

#define CAM_UPDATE_FUNC(func_name, param_name, state_type) \
exception func_name(ThreadQueue* state_queue, const vec3 param_name) { \
    TekState state = {}; \
    memcpy(state.data.param_name, param_name, sizeof(vec3)); \
    state.type = state_type; \
    pushState(state_queue, state); \
    return SUCCESS; \
} \

CAM_UPDATE_FUNC(tekUpdateCamPos, cam_position, CAMERA_MOVE_STATE);
CAM_UPDATE_FUNC(tekUpdateCamRot, cam_rotation, CAMERA_ROTATE_STATE);

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

#define tekEngineCreateBodyCleanup \
    if (body) { tekDeleteBody(body); \
    free(body); } \
    if (mesh_copy) free(mesh_copy); \
    if (material_copy) free(material_copy) \

exception tekEngineCreateBody(ThreadQueue* state_queue, Vector* bodies, Queue* unused_ids, const char* mesh_filename, const char* material_filename) {
    TekBody* body = (TekBody*)malloc(sizeof(TekBody));
    if (!body) tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for body.");

    tekChainThrow(tekCreateBody(mesh_filename, body));

    const uint len_mesh = strlen(mesh_filename) + 1;
    const uint len_material = strlen(material_filename) + 1;
    char* mesh_copy = (char*)malloc(len_mesh);
    char* material_copy = 0;
    if (!mesh_copy) tekThrowThen(MEMORY_EXCEPTION, "Failed to allocate memory to copy mesh filename.", {
        tekEngineCreateBodyCleanup;
    });
    material_copy = (char*)malloc(len_material);
    if (!material_copy) tekThrowThen(MEMORY_EXCEPTION, "Failed to allocate memory to copy material filename.", {
        tekEngineCreateBodyCleanup;
    });
    memcpy(mesh_copy, mesh_filename, len_mesh);
    memcpy(material_copy, material_filename, len_material);

    TekState state = {};

    flag create_id_via_queue = 1;
    if (!queueIsEmpty(unused_ids)) {
        uint unused_id;
        tekChainThrowThen(queueDequeue(unused_ids, &unused_id), {
            tekEngineCreateBodyCleanup;
        });
        tekChainThrowThen(vectorSetItem(bodies, unused_id, body),
            queueEnqueue(unused_ids, (void*)unused_id);
            tekEngineCreateBodyCleanup;
        );
        state.object_id = unused_id;
    } else {
        create_id_via_queue = 0;
        tekChainThrowThen(vectorAddItem(bodies, body), {
            tekEngineCreateBodyCleanup;
        });
        state.object_id = bodies->length - 1;
    }

    state.data.entity.mesh_filename = mesh_filename;
    state.data.entity.material_filename = material_filename;
    tekChainThrowThen(pushState(state_queue, state), {
        if (create_id_via_queue) {
            // if this also fails, then you're beyond screwed xD
            // plus we are gonna clean up anyway
            queueEnqueue(unused_ids, (void*)state.object_id);
        }
        tekEngineCreateBodyCleanup;
    });

    state.type = ENTITY_CREATE_STATE;
    return SUCCESS;
}

exception tekEngineDeleteBody(ThreadQueue* state_queue, Vector* bodies, Queue* unused_ids, const uint object_id) {
    TekBody* body;
    tekChainThrow(vectorGetItemPtr(bodies, object_id, &body));
    if (body->num_vertices == 0) {
        // if the number of vertices == 0, then body is not valid
        // most likely, the whole thing is just zeroes
        tekThrow(ENGINE_EXCEPTION, "Body ID is not valid.");
    }
    tekChainThrow(queueEnqueue(unused_ids, (void*)object_id));

    TekState state = {};
    state.object_id = object_id;
    state.type = ENTITY_DELETE_STATE;
    tekChainThrow(pushState(state_queue, state));

    // set the data stored to be zeroes
    // removing the body would change the index of other bodies
    // index needed to find object by id.
    memset(body, 0, sizeof(TekBody));
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

    Vector bodies = {};
    threadChainThrow(vectorCreate(0, sizeof(TekBody), &bodies));

    Queue unused_ids = {};
    queueCreate(&unused_ids);

    flag mouse_moved = 0;
    flag w = 0, a = 0, s = 0, d = 0;
    double prev_mx = 0.0, prev_my = 0.0, delta_mx = 0.0, delta_my = 0.0;
    flag cam_pos_changed = 0;
    vec3 rotation = { 0.0f, 0.0f, 0.0f };
    vec3 position = { 0.0f, 0.0f, 0.0f };

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
                const double effective_pitch = rotation[1];
                double effective_yaw = rotation[0];
                float direction_multiplier = 0.0f;
                if (event.data.key_input.key == GLFW_KEY_W) {
                    if (event.data.key_input.action == GLFW_RELEASE) w = 0;
                    else w = 1;
                }
                if (event.data.key_input.key == GLFW_KEY_A) {
                    if (event.data.key_input.action == GLFW_RELEASE) a = 0;
                    else a = 1;
                }
                if (event.data.key_input.key == GLFW_KEY_S) {
                    if (event.data.key_input.action == GLFW_RELEASE) s = 0;
                    else s = 1;
                }
                if (event.data.key_input.key == GLFW_KEY_D) {
                    if (event.data.key_input.action == GLFW_RELEASE) d = 0;
                    else d = 1;
                }

                if (w) {
                    direction_multiplier = MOVE_SPEED;
                }
                if (a) {
                    direction_multiplier = MOVE_SPEED;
                    effective_yaw += M_PI;
                }
                if (s) {
                    direction_multiplier = -MOVE_SPEED;
                }
                if (d) {
                    direction_multiplier = -MOVE_SPEED;
                    effective_yaw = M_PI;
                }

                position[0] += (float)(cos(effective_yaw) * cos(effective_pitch) * direction_multiplier);
                position[1] += (float)(sin(effective_pitch) * direction_multiplier);
                position[2] += (float)(sin(effective_yaw) * cos(effective_pitch) * direction_multiplier);

                cam_pos_changed = 1;
                break;
            case MOUSE_POS_EVENT:
                if (!mouse_moved) {
                    prev_mx = event.data.mouse_move_input.x;
                    prev_my = event.data.mouse_move_input.y;
                    mouse_moved = 1;
                }
                delta_mx = event.data.mouse_move_input.x - prev_mx;
                delta_my = event.data.mouse_move_input.y - prev_my;
                break;
            default:
                break;
            }
        }

        prev_mx = event.data.mouse_move_input.x;
        prev_my = event.data.mouse_move_input.y;

        rotation[0] = (float)(fmod(rotation[0] + delta_mx * MOUSE_SENSITIVITY + M_PI, 2.0 * M_PI) - M_PI);
        rotation[1] = (float)fmin(fmax(rotation[1] + delta_my * MOUSE_SENSITIVITY, -M_PI_2), M_PI_2);

        if (cam_pos_changed) {
            threadChainThrow(tekUpdateCamPos(state_queue, position));
            cam_pos_changed = 0;
        }

        if ((delta_mx != 0.0) || (delta_my != 0.0)) {
            threadChainThrow(tekUpdateCamRot(state_queue, rotation));
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