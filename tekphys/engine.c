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
#include <cglm/euler.h>

#include "body.h"
#include "collider.h"
#include "../core/vector.h"
#include "../core/queue.h"
#include "GLFW/glfw3.h"
#include "collisions.h"

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
 * @brief Receive a state from the thread queue.
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
 * @param friction The coefficient of friction.
 * @param restitution The coefficient of restitution.
 * @param position The position of the body in the world.
 * @param rotation The rotation of the body as a quaternion.
 * @param scale The scale in x, y, and z direction from the original shape of the body.
 * @param object_id A pointer to where the new object id will be stored. Can be set to NULL if id is not needed.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 */
static exception tekEngineCreateBody(ThreadQueue* state_queue, Vector* bodies, const uint object_id, const char* mesh_filename, const char* material_filename, const float mass, const float friction, const float restitution, vec3 position, vec4 rotation, vec3 scale) {
    TekBody body = {};
    tekChainThrow(tekCreateBody(mesh_filename, mass, friction, restitution, position, rotation, scale, &body));

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
    state.object_id = object_id;

    if (bodies->length <= object_id) {
        TekBody dummy = {};
        memset(&dummy, 0, sizeof(TekBody));
        while (bodies->length < object_id) {
            tekChainThrowThen(vectorAddItem(bodies, &dummy), {
                tekEngineCreateBodyCleanup;
            });
        }
        tekChainThrowThen(vectorAddItem(bodies, &body), {
            tekEngineCreateBodyCleanup;
        });
    } else {
        TekBody* delete_body;
        tekChainThrowThen(vectorGetItemPtr(bodies, object_id, &delete_body), {
           tekEngineCreateBodyCleanup;
        });
        if (delete_body->num_vertices != 0)
            tekDeleteBody(delete_body);

        tekChainThrowThen(vectorSetItem(bodies, object_id, &body), {
            tekEngineCreateBodyCleanup;
        });
    }

    state.type = ENTITY_CREATE_STATE;
    state.data.entity.mesh_filename = mesh_filename;
    state.data.entity.material_filename = material_filename;
    glm_vec3_copy(position, state.data.entity.position);
    glm_vec4_copy(rotation, state.data.entity.rotation);
    glm_vec3_copy(scale, state.data.entity.scale);

    pushState(state_queue, state);

    return SUCCESS;
}

/**
 * @brief Update the position and rotation of a body given its object id.
 * @param state_queue The ThreadQueue that links to the graphics thread.
 * @param bodies A pointer to a vector containing the bodies.
 * @param object_id The object id of the body to update.
 * @param position The new position of the body.
 * @param rotation The new rotation of the body as a quaternion.
 * @param scale The new scale of the body.
 * @throws ENGINE_EXCEPTION if the object id is invalid.
 */
static exception tekEngineUpdateBody(ThreadQueue* state_queue, const Vector* bodies, const uint object_id, vec3 position, vec4 rotation, vec3 scale) {
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
    glm_vec3_copy(scale, state.data.entity_update.scale);

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
static exception tekEngineDeleteBody(ThreadQueue* state_queue, const Vector* bodies, const uint object_id) {
    TekBody* body;
    tekChainThrow(vectorGetItemPtr(bodies, object_id, &body));
    if (body->num_vertices == 0) {
        // if the number of vertices == 0, then body is not valid
        // most likely, the whole thing is just zeroes
        tekThrow(ENGINE_EXCEPTION, "Body ID is not valid.");
    }

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

static exception tekEngineDeleteAllBodies(ThreadQueue* state_queue, const Vector* bodies) {
    for (uint i = 0; i < bodies->length; i++) {
        TekBody* body;
        tekChainThrow(vectorGetItemPtr(bodies, i, &body));
        if (body->num_vertices == 0) continue;

        tekChainThrow(tekEngineDeleteBody(state_queue, bodies, i));
    }
    return SUCCESS;
}

/// Store the args to link physics thread to graphics thread, so it can be passed as a single pointer to the thread procedure.
struct TekEngineArgs {
    ThreadQueue* event_queue;
    ThreadQueue* state_queue;
    double phys_period;
};

#define threadThrow(exception_code, exception_message) { const exception __thread_exception = exception_code; if (__thread_exception) { tekSetException(__thread_exception, __LINE__, __FUNCTION__, __FILE__, exception_message); threadExcept(state_queue, __thread_exception); goto tek_engine_cleanup; } }
#define threadChainThrow(exception_code) { const exception __thread_exception = exception_code; if (__thread_exception) { tekTraceException(__thread_exception, __LINE__, __FUNCTION__, __FILE__); threadExcept(state_queue, __thread_exception); goto tek_engine_cleanup; } }

/**
 * @brief The main physics thread procedure, will run in parallel to the graphics thread. Responsible for logic, has a loop running at fixed time interval.
 * @param args Required for use with pthread library. Used to pass TekEngineArgs pointer.
 */
static void tekEngine(void* args) {
    const struct TekEngineArgs* engine_args = (struct TekEngineArgs*)args;
    ThreadQueue* event_queue = engine_args->event_queue;
    ThreadQueue* state_queue = engine_args->state_queue;
    double phys_period = engine_args->phys_period;
    free(args);

    Vector bodies = {};
    threadChainThrow(vectorCreate(0, sizeof(TekBody), &bodies));

    Queue unused_ids = {};
    queueCreate(&unused_ids);

    struct timespec engine_time, step_time;
    double phys_period_s = floor(phys_period);
    step_time.tv_sec = (__time_t)phys_period_s;
    step_time.tv_nsec = (__syscall_slong_t)(1e9 * (phys_period - phys_period_s));
    clock_gettime(CLOCK_MONOTONIC, &engine_time);
    flag running = 1;
    uint counter = 0;
    flag mode = 0;
    flag paused = 0;
    flag step = 0;
    float gravity = 9.81f;

    mat4 snapshot_rotation_matrix;
    vec4 snapshot_rotation_quat;
    TekBody* snapshot_body;

    while (running) {
        TekEvent event = {};
        while (recvEvent(event_queue, &event) == SUCCESS) {
            switch (event.type) {
            case QUIT_EVENT:
                // end the loop
                running = 0;

                // free the thread queues, as they won't be needed any more
                threadQueueDelete(event_queue);
                threadQueueDelete(state_queue);
                break;
            case MODE_CHANGE_EVENT:
                mode = event.data.mode;
                break;
            case BODY_CREATE_EVENT:
                // cannot convert directly for some reason
                glm_euler(event.data.body.snapshot.rotation, snapshot_rotation_matrix);
                glm_mat4_quat(snapshot_rotation_matrix, snapshot_rotation_quat);

                threadChainThrow(tekEngineCreateBody(
                    state_queue, &bodies, event.data.body.id,
                    event.data.body.snapshot.model, event.data.body.snapshot.material,
                    event.data.body.snapshot.mass, event.data.body.snapshot.friction, event.data.body.snapshot.restitution,
                    event.data.body.snapshot.position, snapshot_rotation_quat, (vec3){1.0f, 1.0f, 1.0f}
                    ));

                threadChainThrow(vectorGetItemPtr(&bodies, event.data.body.id, &snapshot_body));
                glm_vec3_copy(event.data.body.snapshot.velocity, snapshot_body->velocity);
                snapshot_body->immovable = event.data.body.snapshot.immovable;

                break;
            case BODY_UPDATE_EVENT:
                // cannot convert directly for some reason
                glm_euler(event.data.body.snapshot.rotation, snapshot_rotation_matrix);
                glm_mat4_quat(snapshot_rotation_matrix, snapshot_rotation_quat);
                glm_euler_xyz_quat_rh(event.data.body.snapshot.rotation, snapshot_rotation_quat);

                threadChainThrow(vectorGetItemPtr(&bodies, event.data.body.id, &snapshot_body));
                glm_vec3_copy(event.data.body.snapshot.position, snapshot_body->position);
                glm_vec4_copy(snapshot_rotation_quat, snapshot_body->rotation);
                glm_vec3_copy(event.data.body.snapshot.velocity, snapshot_body->velocity);
                glm_vec3_copy(event.data.body.snapshot.angular_velocity, snapshot_body->angular_velocity);
                snapshot_body->friction = event.data.body.snapshot.friction;
                snapshot_body->restitution = event.data.body.snapshot.restitution;
                threadChainThrow(tekBodySetMass(snapshot_body, event.data.body.snapshot.mass));
                snapshot_body->immovable = event.data.body.snapshot.immovable;

                threadChainThrow(tekEngineUpdateBody(
                    state_queue, &bodies, event.data.body.id,
                    event.data.body.snapshot.position, snapshot_rotation_quat, (vec3){1.0f, 1.0f, 1.0f}
                ));

                break;
            case BODY_DELETE_EVENT:
                threadChainThrow(tekEngineDeleteBody(state_queue, &bodies, event.data.body.id));
                break;
            case CLEAR_EVENT:
                threadChainThrow(tekEngineDeleteAllBodies(state_queue, &bodies));
                break;
            case TIME_EVENT:
                phys_period = 1 / event.data.time.rate;
                phys_period_s = floor(phys_period / event.data.time.speed);
                step_time.tv_sec = (__time_t)(phys_period_s / event.data.time.speed);
                step_time.tv_nsec = (__syscall_slong_t)(1e9 * (phys_period / event.data.time.speed - phys_period_s));
                break;
            case PAUSE_EVENT:
                paused = event.data.paused;
                break;
            case STEP_EVENT:
                if (paused)
                    step = 1;
                break;
            case GRAVITY_EVENT:
                gravity = event.data.gravity;
            default:
                break;
            }
        }

        if (!running) break;

        if (step) paused = 0;

        if (mode == MODE_RUNNER && !paused) {
            threadChainThrow(tekSolveCollisions(&bodies, (float)phys_period));

            for (uint i = 0; i < bodies.length; i++) {
                TekBody* body = 0;
                threadChainThrow(vectorGetItemPtr(&bodies, i, &body));
                if (!body->num_vertices) continue;
                if (body->immovable) {
                    glm_vec3_zero(body->velocity);
                    glm_vec3_zero(body->angular_velocity);
                }
                tekBodyAdvanceTime(body, (float)phys_period, gravity);
                threadChainThrow(tekEngineUpdateBody(state_queue, &bodies, i, body->position, body->rotation, body->scale));
            }
        }

        if (step) {
            paused = 1;
            step = 0;
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

tek_engine_cleanup:
    for (uint i = 0; i < bodies.length; i++) {
        TekBody* loop_body;
        threadChainThrow(vectorGetItemPtr(&bodies, i, &loop_body));
        tekDeleteBody(loop_body);
    }

    vectorDelete(&bodies);
    queueDelete(&unused_ids);
}

/**
 * @brief Start the physics thread, providing two thread queues to send and recieve events / states.
 * @param event_queue A pointer to an existing thread queue that will send events to the physics thread.
 * @param state_queue A pointer to an existing thread queue that will recieve updates of the physics state.
 * @param phys_period A time period that represents the length of a single iteration of the physics loop. In other words '1 / ticks per second'
 * @param thread A pointer to a pthread_t variable that will contain the thread. Do pthread_join(thread) at the end to ensure the thread is finished.
 * @throws THREAD_EXCEPTION if the call to pthread_create() fails.
 */
exception tekInitEngine(ThreadQueue* event_queue, ThreadQueue* state_queue, const double phys_period, unsigned long long* thread) {
    struct TekEngineArgs* engine_args = (struct TekEngineArgs*)malloc(sizeof(struct TekEngineArgs));
    engine_args->event_queue = event_queue;
    engine_args->state_queue = state_queue;
    engine_args->phys_period = phys_period;
    if (pthread_create((pthread_t*)thread, NULL, tekEngine, engine_args))
	    tekThrow(THREAD_EXCEPTION, "Failed to create physics thread");
    return SUCCESS;
}

/**
 * Wait for the engine thread to join.
 * @param thread The engine thread, returned by tekInitEngine()
 */
void tekAwaitEngineStop(const unsigned long long thread) {
    pthread_join(thread, NULL);
}
