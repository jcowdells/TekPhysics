#pragma once

#include "../core/exception.h"
#include "../core/threadqueue.h"

#include "../tekgl/entity.h"

#define QUIT_EVENT 0
#define KEY_EVENT  1

#define MESSAGE_STATE   0
#define EXCEPTION_STATE 1

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
        } entity;
    } data;
} TekState;

exception recvState(ThreadQueue* queue, TekState* state);
exception pushEvent(ThreadQueue* queue, TekEvent event);
exception tekInitEngine(ThreadQueue* event_queue, ThreadQueue* state_queue, double phys_period);