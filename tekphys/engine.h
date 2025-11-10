#pragma once

#include "../core/exception.h"
#include "../core/threadqueue.h"

#include "../tekgl/entity.h"
#include "body.h"

#define QUIT_EVENT         0
#define MODE_CHANGE_EVENT  1
#define BODY_CREATE_EVENT  2
#define BODY_DELETE_EVENT  3
#define BODY_UPDATE_EVENT  4
#define CLEAR_EVENT        5
#define TIME_EVENT         6
#define PAUSE_EVENT        7
#define STEP_EVENT         8
#define GRAVITY_EVENT      9

#define MESSAGE_STATE       0
#define EXCEPTION_STATE     1
#define ENTITY_CREATE_STATE 2
#define ENTITY_DELETE_STATE 3
#define ENTITY_UPDATE_STATE 4

typedef struct TekEvent {
    flag type;
    union {
        struct {
            TekBodySnapshot snapshot;
            uint id;
        } body;
        flag mode;
        struct {
            double rate;
            double speed;
        } time;
        flag paused;
        float gravity;
    } data;
} TekEvent;

typedef struct TekState {
    flag type;
    uint object_id;
    union {
        char* message;
        uint exception;
        struct {
            const char* mesh_filename;
            const char* material_filename;
            vec3 position;
            vec4 rotation;
            vec3 scale;
        } entity;
        struct {
            vec3 position;
            vec4 rotation;
            vec3 scale;
        } entity_update;
    } data;
} TekState;

exception recvState(ThreadQueue* queue, TekState* state);
exception pushEvent(ThreadQueue* queue, TekEvent event);
exception tekInitEngine(ThreadQueue* event_queue, ThreadQueue* state_queue, double phys_period, unsigned long long* thread);
void tekAwaitEngineStop(unsigned long long thread);

exception pushTriangle(vec3 triangle[3]);
exception pushOBB(void* obb);