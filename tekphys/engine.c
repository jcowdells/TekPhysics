#include "engine.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <math.h>
#include <pthread.h>

#include "body.h"
#include "collider.h"
#include "../core/vector.h"
#include "../core/queue.h"
#include "GLFW/glfw3.h"

#define BILLION 1000000000

#define MOUSE_SENSITIVITY 0.005f
#define MOVE_SPEED        0.1f

#define RECV_FUNC(func_name, func_type, param_name) \
exception func_name(ThreadQueue* queue, func_type* param_name) { \
    func_type* copy; \
    if (!threadQueueDequeue(queue, &copy)) return FAILURE; \
    memcpy(param_name, copy, sizeof(func_type)); \
    free(copy); \
    return SUCCESS; \
} \

/**
 * @brief Recieve an event from the thread queue.
 * @note Requires the event struct to exist already
 * @param queue A pointer to the thread queue to recieve from.
 * @param event A pointer to an empty TekEvent struct.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 */
static RECV_FUNC(recvEvent, TekEvent, event);

/**
 * @brief Recieve a state from the thread queue.
 * @note Requires the state struct to exist already.
 * @param queue A pointer to the thread queue to recieve from.
 * @param state A pointer to an empty TekState struct.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 */
RECV_FUNC(recvState, TekState, state);

#define PUSH_FUNC(func_name, func_type, param_name) \
exception func_name(ThreadQueue* queue, const func_type param_name) { \
    func_type* copy = (func_type*)malloc(sizeof(func_type)); \
    if (!copy) tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for thread queue"); \
    memcpy(copy, &param_name, sizeof(func_type)); \
    if (!threadQueueEnqueue(queue, copy)) return FAILURE; \
    return SUCCESS; \
} \

/**
 * @brief Push a state to the thread queue.
 * @param queue A pointer to the thread queue to push to.
 * @param state A pointer to the state struct to push.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 */
static PUSH_FUNC(pushState, TekState, state);

/**
 * @brief Push an event to the thread queue.
 * @param queue A pointer to the thread queue to push to.
 * @param event A pointer to the event to push.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 */
PUSH_FUNC(pushEvent, TekEvent, event);

#define CAM_UPDATE_FUNC(func_name, param_name, state_type) \
static exception func_name(ThreadQueue* state_queue, const vec3 param_name) { \
    TekState state = {}; \
    memcpy(state.data.param_name, param_name, sizeof(vec3)); \
    state.type = state_type; \
    tekChainThrow(pushState(state_queue, state)); \
    return SUCCESS; \
} \

/**
 * @brief Send a state that updates the camera position.
 * @param state_queue The state queue (ThreadQueue) to push the state to.
 * @param cam_position The new position of the camera.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 */
CAM_UPDATE_FUNC(tekUpdateCamPos, cam_position, CAMERA_MOVE_STATE);

/**
 * @brief Send a state that updates the camera rotation.
 * @param state_queue The state queue (ThreadQueue) to push the camera rotation to.
 * @param cam_rotation The new rotation of the camera.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 */
CAM_UPDATE_FUNC(tekUpdateCamRot, cam_rotation, CAMERA_ROTATE_STATE);

/**
 * @brief Send a print message through the state queue.
 * @param state_queue The state queue to send the message through.
 * @param format The message to send + formatting e.g. "string: %s"
 * @param ... The formatting to add to the string
 */
static void threadPrint(ThreadQueue* state_queue, const char* format, ...) {
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

#define tprint(format, ...) threadPrint(state_queue, format, __VA_ARGS__)

/**
 * @brief Send an exception message to the state queue.
 * @param state_queue The state queue to send the exception to.
 * @param exception The exception code.
 */
static void threadExcept(ThreadQueue* state_queue, const uint exception) {
    TekState exception_state = {};
    exception_state.type = EXCEPTION_STATE;
    exception_state.object_id = 0;
    exception_state.data.exception = exception;
    pushState(state_queue, exception_state);
}

#define threadThrow(exception_code, exception_message) { const exception __thread_exception = exception_code; if (__thread_exception) { tekSetException(__thread_exception, __LINE__, __FUNCTION__, __FILE__, exception_message); threadExcept(state_queue, __thread_exception); return; } }
#define threadChainThrow(exception_code) { const exception __thread_exception = exception_code; if (__thread_exception) { tekTraceException(__LINE__, __FUNCTION__, __FILE__); threadExcept(state_queue, __thread_exception); return; } }

#define tekEngineCreateBodyCleanup \
    tekDeleteBody(&body); \
    if (mesh_copy) free(mesh_copy); \
    if (material_copy) free(material_copy) \

/**
 * @brief Create a body given the mesh, material, position etc.
 * @note Will create both a body and a corresponding entity on the graphics thread. The object ids are assigned in order, filling gaps in the order when they appear.
 * @param state_queue The ThreadQueue that will be used to send the entity creation message to the graphics thread.
 * @param bodies A pointer to a vector that contains the bodies.
 * @param unused_ids A queue containing unused object ids, allowing gaps in the list of ids to be filled by new objects.
 * @param mesh_filename The file that contains the mesh for the object.
 * @param material_filename The file that contains the material for the object. Only used in the graphics thread.
 * @param mass The mass of the body.
 * @param position The position of the body in the world.
 * @param rotation The rotation of the body as a quaternion.
 * @param scale The scale in x, y, and z direction from the original shape of the body.
 * @param object_id A pointer to where the new object id will be stored. Can be set to NULL if id is not needed.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 */
static exception tekEngineCreateBody(ThreadQueue* state_queue, Vector* bodies, Queue* unused_ids, const char* mesh_filename, const char* material_filename,
                              const float mass, vec3 position, vec4 rotation, vec3 scale, uint* object_id) {
    TekBody body = {};
    tekChainThrow(tekCreateBody(mesh_filename, mass, position, rotation, scale, &body));

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
        tekChainThrowThen(vectorSetItem(bodies, unused_id, &body),
            queueEnqueue(unused_ids, (void*)unused_id);
            tekEngineCreateBodyCleanup;
        );
        state.object_id = unused_id;
    } else {
        create_id_via_queue = 0;
        tekChainThrowThen(vectorAddItem(bodies, &body), {
            tekEngineCreateBodyCleanup;
        });
        state.object_id = bodies->length - 1;
    }

    state.type = ENTITY_CREATE_STATE;
    state.data.entity.mesh_filename = mesh_filename;
    state.data.entity.material_filename = material_filename;
    glm_vec3_copy(position, state.data.entity.position);
    glm_vec4_copy(rotation, state.data.entity.rotation);
    glm_vec3_copy(scale, state.data.entity.scale);
    tekChainThrowThen(pushState(state_queue, state), {
        if (create_id_via_queue) {
            // if this enqueue also fails, then you're beyond screwed xD
            // plus we are gonna clean up anyway
            queueEnqueue(unused_ids, (void*)state.object_id);
        }
        tekEngineCreateBodyCleanup;
    });

    state.type = __COLLIDER_STATE;
    state.data.collider = body.collider;
    tprint("Pushing collider: %p\n", body.collider);
    pushState(state_queue, state);

    if (object_id)
        *object_id = state.object_id;
    return SUCCESS;
}

/**
 * @brief Update the position and rotation of a body given its object id.
 * @param state_queue The ThreadQueue that links to the graphics thread.
 * @param bodies A pointer to a vector containing the bodies.
 * @param object_id The object id of the body to update.
 * @param position The new position of the body.
 * @param rotation The new rotation of the body as a quaternion.
 * @throws ENGINE_EXCEPTION if the object id is invalid.
 */
static exception tekEngineUpdateBody(ThreadQueue* state_queue, const Vector* bodies, const uint object_id, vec3 position, vec4 rotation) {
    TekBody* body;
    tekChainThrow(vectorGetItemPtr(bodies, object_id, &body));
    if (body->num_vertices == 0) {
        // if the number of vertices == 0, then body is not valid
        // most likely, the whole thing is just zeroes
        tekThrow(ENGINE_EXCEPTION, "Body ID is not valid.");
    }

    TekState state = {};
    state.type = ENTITY_UPDATE_STATE;
    state.object_id = object_id;
    glm_vec3_copy(position, state.data.entity_update.position);
    glm_vec4_copy(rotation, state.data.entity_update.rotation);

    tekChainThrow(pushState(state_queue, state));
    return SUCCESS;
}

/**
 * @brief Delete a body, freeing the object id for reuse and removing the counterpart on the graphics thread.
 * @param state_queue The ThreadQueue linking to the graphics thread.
 * @param bodies A vector containing the bodies.
 * @param unused_ids The queue containing unused ids, the removed id will be enqueued to this.
 * @param object_id The id of the body to delete.
 * @throws ENGINE_EXCEPTION if the object id is invalid.
 */
static exception tekEngineDeleteBody(ThreadQueue* state_queue, const Vector* bodies, Queue* unused_ids, const uint object_id) {
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
    tekDeleteBody(body);
    memset(body, 0, sizeof(TekBody));
    return SUCCESS;
}

/// Store the args to link physics thread to graphics thread, so it can be passed as a single pointer to the thread procedure.
struct TekEngineArgs {
    ThreadQueue* event_queue;
    ThreadQueue* state_queue;
    double phys_period;
};

/**
 * @brief The main physics thread procedure, will run in parallel to the graphics thread. Responsible for logic, has a loop running at fixed time interval.
 * @param args Required for use with pthread library. Used to pass TekEngineArgs pointer.
 */
static void tekEngine(void* args) {
    const struct TekEngineArgs* engine_args = (struct TekEngineArgs*)args;
    ThreadQueue* event_queue = engine_args->event_queue;
    ThreadQueue* state_queue = engine_args->state_queue;
    const double phys_period = engine_args->phys_period;
    free(args);

    tekBodyInit();

    Vector bodies = {};
    threadChainThrow(vectorCreate(0, sizeof(TekBody), &bodies));

    Queue unused_ids = {};
    queueCreate(&unused_ids);

    flag mouse_moved = 0, mouse_moving = 0;
    flag w = 0, a = 0, s = 0, d = 0, q = 0;
    double prev_mx = 0.0, prev_my = 0.0, cur_mx = 0.0, cur_my = 0.0, delta_mx = 0.0, delta_my = 0.0;
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
                if (event.data.key_input.key == GLFW_KEY_Q) {
                    if (event.data.key_input.action == GLFW_RELEASE) q = 0;
                    else q = 1;
                }
                if ((event.data.key_input.key == GLFW_KEY_E) && (event.data.key_input.action == GLFW_RELEASE)) {
                    vec4 cube_rotation = { 0.0f, 0.0f, 0.0f, 1.0f };
                    vec3 cube_scale = { 1.0f, 1.0f, 1.0f };
                    threadChainThrow(tekEngineCreateBody(state_queue, &bodies, &unused_ids, "../res/rad1.tmsh", "../res/material.tmat",
                                                         10.0f, position, cube_rotation, cube_scale, 0));
                }
                break;
            case MOUSE_POS_EVENT:
                if (!mouse_moved) {
                    prev_mx = event.data.mouse_move_input.x;
                    prev_my = event.data.mouse_move_input.y;
                    mouse_moved = 1;
                }
                cur_mx = event.data.mouse_move_input.x;
                cur_my = event.data.mouse_move_input.y;
                delta_mx = event.data.mouse_move_input.x - prev_mx;
                delta_my = event.data.mouse_move_input.y - prev_my;

                mouse_moving = 1;
                break;
            default:
                break;
            }
        }

        for (uint i = 0; i < bodies.length; i++) {
            TekBody* body = 0;
            threadChainThrow(vectorGetItemPtr(&bodies, i, &body));
            if (!body->num_vertices) continue;
            tekBodyAdvanceTime(body, (float)phys_period);
            threadChainThrow(tekEngineUpdateBody(state_queue, &bodies, i, body->position, body->rotation));
        }

        if (mouse_moving) {
            rotation[0] = (float)(fmod(rotation[0] + delta_mx * MOUSE_SENSITIVITY + M_PI, 2.0 * M_PI) - M_PI);
            rotation[1] = (float)fmin(fmax(rotation[1] - delta_my * MOUSE_SENSITIVITY, -M_PI_2), M_PI_2);
            prev_mx = cur_mx;
            prev_my = cur_my;
            threadChainThrow(tekUpdateCamRot(state_queue, rotation));
            delta_mx = 0.0;
            delta_my = 0.0;
            mouse_moving = 0;
        }

        const double pitch = rotation[1], yaw = rotation[0];
        if (w) {
            position[0] += (float)(cos(yaw) * cos(pitch) * MOVE_SPEED);
            position[1] += (float)(sin(pitch) * MOVE_SPEED);
            position[2] += (float)(sin(yaw) * cos(pitch) * MOVE_SPEED);
            cam_pos_changed = 1;
        }
        if (a) {
            position[0] += (float)(cos(yaw + M_PI_2) * -MOVE_SPEED);
            position[2] += (float)(sin(yaw + M_PI_2) * -MOVE_SPEED);
            cam_pos_changed = 1;
        }
        if (s) {
            position[0] += (float)(cos(yaw) * cos(pitch) * -MOVE_SPEED);
            position[1] += (float)(sin(pitch) * -MOVE_SPEED);
            position[2] += (float)(sin(yaw) * cos(pitch) * -MOVE_SPEED);
            cam_pos_changed = 1;
        }
        if (d) {
            position[0] += (float)(cos(yaw + M_PI_2) * MOVE_SPEED);
            position[2] += (float)(sin(yaw + M_PI_2) * MOVE_SPEED);
            cam_pos_changed = 1;
        }

        for (uint i = 0; i < bodies.length; i++) {
            TekBody* body_i;
            threadChainThrow(vectorGetItemPtr(&bodies, i, &body_i));
            for (uint j = 0; j < i; j++) {
                TekBody* body_j;
                threadChainThrow(vectorGetItemPtr(&bodies, j, &body_j));
                printf("Testing body %u against body %u\n", i, j);
                printf("%s between outers\n", testCollision(body_i, body_j) ? "collision" : "no collision");
            }
        }

        // TODO: remove, as this is a test
        if (q) {
            vec3 impulse = {
                (float)(cos(yaw) * cos(pitch)),
                (float)(sin(pitch)),
                (float)(sin(yaw) * cos(pitch))
            };
            if (bodies.length >= 1) {
                TekBody* body_ptr;
                threadChainThrow(vectorGetItemPtr(&bodies, 0, &body_ptr));
                tekBodyApplyImpulse(body_ptr, position, impulse, (float)phys_period);
            }
        }

        if (cam_pos_changed) {
            threadChainThrow(tekUpdateCamPos(state_queue, position));
            cam_pos_changed = 0;
        }

        engine_time.tv_sec += step_time.tv_sec;
        engine_time.tv_nsec += step_time.tv_nsec;

        while (engine_time.tv_nsec >= BILLION) {
            engine_time.tv_nsec -= BILLION;
            engine_time.tv_sec += 1;
        }

        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &engine_time, NULL);
        counter++;
    }
    for (uint i = 0; i < bodies.length; i++) {
        TekBody* loop_body;
        threadChainThrow(vectorGetItemPtr(&bodies, i, &loop_body));
        tekDeleteBody(loop_body);
    }
    vectorDelete(&bodies);
    queueDelete(&unused_ids);
    tekBodyDelete();
    printf("thread ended :(\n");
}

/**
 * @brief Start the physics thread, providing two thread queues to send and recieve events / states.
 * @param event_queue A pointer to an existing thread queue that will send events to the physics thread.
 * @param state_queue A pointer to an existing thread queue that will recieve updates of the physics state.
 * @param phys_period A time period that represents the length of a single iteration of the physics loop. In other words '1 / ticks per second'
 * @throws THREAD_EXCEPTION if the call to pthread_create() fails.
 */
exception tekInitEngine(ThreadQueue* event_queue, ThreadQueue* state_queue, const double phys_period) {
    struct TekEngineArgs* engine_args = (struct TekEngineArgs*)malloc(sizeof(struct TekEngineArgs));
    engine_args->event_queue = event_queue;
    engine_args->state_queue = state_queue;
    engine_args->phys_period = phys_period;
    pthread_t engine_thread;
    if (pthread_create(&engine_thread, NULL, tekEngine, engine_args))
	    tekThrow(THREAD_EXCEPTION, "Failed to create physics thread");
    return SUCCESS;
}
