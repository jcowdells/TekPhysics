#pragma once

#include "../core/exception.h"
#include "../core/threadqueue.h"

#define QUIT_EVENT 0

#define MESSAGE_STATE   0
#define EXCEPTION_STATE 1

typedef struct TekEvent {
    flag type;
    union {
        char* message;
    } data;
} TekEvent;

typedef struct TekState {
    flag type;
    uint object_id;
    union {
        char* message;
        uint exception;
    } data;
} TekState;

exception recvState(ThreadQueue* queue, TekState* state);
exception pushEvent(ThreadQueue* queue, TekEvent event);
exception tekInitEngine(ThreadQueue* event_queue, ThreadQueue* state_queue, double phys_period);