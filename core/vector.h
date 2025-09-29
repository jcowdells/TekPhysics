#pragma once

#include "../tekgl.h"
#include "exception.h"

typedef struct Vector {
    void* internal;
    uint internal_size;
    uint element_size;
    uint length;
} Vector;

exception vectorCreate(uint start_capacity, uint element_size, Vector* vector);
exception vectorAddItem(Vector* vector, const void* item);
exception vectorSetItem(const Vector* vector, uint index, const void* item);
exception vectorGetItem(const Vector* vector, uint index, void* item);
exception vectorGetItemPtr(const Vector* vector, uint index, void** item);
exception vectorRemoveItem(Vector* vector, uint index, void* item);
flag vectorPopItem(Vector* vector, void* item);
exception vectorInsertItem(Vector* vector, uint index, const void* item);
void vectorClear(Vector* vector);
void vectorDelete(Vector* vector);
