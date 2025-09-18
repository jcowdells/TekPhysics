#pragma once

#include "../core/exception.h"
#include "../core/threadqueue.h"

#include "../tekgl/entity.h"
#include "cglm/ivec3.h"

#define QUIT_EVENT        0
#define KEY_EVENT         1
#define MOUSE_POS_EVENT   2
#define MODE_CHANGE_EVENT 3

#define MESSAGE_STATE       0
#define EXCEPTION_STATE     1
#define ENTITY_CREATE_STATE 2
#define ENTITY_DELETE_STATE 3
#define CAMERA_MOVE_STATE   4
#define CAMERA_ROTATE_STATE 5
#define ENTITY_UPDATE_STATE 6

typedef struct TekEvent {
    flag type;
    union {
        char* message;
        struct {
            int key;
            int scancode;
            int action;
            int mods;
        } key_input;
        struct {
            double x;
            double y;
        } mouse_move_input;
        flag mode;
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
        vec3 cam_position;
        vec3 cam_rotation;
        struct {
            vec3 position;
            vec4 rotation;
        } entity_update;
        void* collider;
        struct {
            void* vertices;
            flag collision;
        } triangle_pair;
        vec3 triangle[3];
    } data;
} TekState;

exception recvState(ThreadQueue* queue, TekState* state);
exception pushEvent(ThreadQueue* queue, TekEvent event);
exception tekInitEngine(ThreadQueue* event_queue, ThreadQueue* state_queue, double phys_period);

exception pushTriangle(vec3 triangle[3]);
exception pushOBB(void* obb);