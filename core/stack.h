#pragma once

#include "../tekgl.h"
#include "exception.h"

typedef struct StackItem {
    void* data;
    struct StackItem* next;
} StackItem;

typedef struct Stack {
    StackItem* data;
    uint length;
} Stack;

void stackCreate(Stack* stack);
void stackDelete(Stack* stack);
exception stackPush(Stack* stack, void* data);
exception stackPop(Stack* stack, void** data);
exception stackPeek(const Stack* stack, void** data);