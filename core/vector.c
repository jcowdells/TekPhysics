#include "vector.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

exception vectorCreate(uint start_capacity, const uint element_size, Vector* vector) {
    if (element_size == 0) tekThrow(VECTOR_EXCEPTION, "Vector elements cannot have a size of 0.");
    if (start_capacity == 0) start_capacity = 1;
    void* internal = (void*)malloc(start_capacity * element_size);
    if (!internal) tekThrow(MEMORY_EXCEPTION, "Failed to allocate initial memory for vector.");
    vector->internal = internal;
    vector->internal_size = start_capacity;
    vector->element_size = element_size;
    vector->length = 0;
    return SUCCESS;
}

void vectorWriteItem(const Vector* vector, const uint index, const void* item) {
    void* dest = vector->internal + index * vector->element_size;
    memcpy(dest, item, vector->element_size);
}

exception vectorDoubleCapacity(Vector* vector) {
    const uint new_size = vector->internal_size * 2;
    void* internal = vector->internal;
    void* temp = (void*)realloc(internal, new_size * vector->element_size);
    if (!temp) tekThrowThen(MEMORY_EXCEPTION, "Failed to allocate more memory to grow vector.", free(internal));
    vector->internal = temp;
    vector->internal_size = new_size;
    printf("grew in size, len=%u\n", vector->length);
    return SUCCESS;
}

exception vectorAddItem(Vector* vector, const void* item) {
    if (vector->length >= vector->internal_size)
        tekChainThrow(vectorDoubleCapacity(vector));
    vectorWriteItem(vector, vector->length, item);
    vector->length++;
    return SUCCESS;
}

exception vectorSetItem(const Vector* vector, const uint index, const void* item) {
    if (index >= vector->length) tekThrow(VECTOR_EXCEPTION, "Attempted to set index out of bounds.");
    vectorWriteItem(vector, index, item);
    return SUCCESS;
}

exception vectorGetItem(const Vector* vector, const uint index, void* item) {
    if (index >= vector->length) tekThrow(VECTOR_EXCEPTION, "Attempted to get index out of bounds.");
    const void* src = vector->internal + index * vector->element_size;
    memcpy(item, src, vector->element_size);
    return SUCCESS;
}

exception vectorGetItemPtr(const Vector* vector, const uint index, void** item) {
    if (index >= vector->length) tekThrow(VECTOR_EXCEPTION, "Attempted to get index out of bounds.");
    *item = vector->internal + index * vector->element_size;
    return SUCCESS;
}

exception vectorRemoveItem(Vector* vector, const uint index, void* item) {
    tekChainThrow(vectorGetItem(vector, index, item));
    void* dest = vector->internal + index * vector->element_size;
    const void* src = dest + vector->element_size;
    const uint remainder_size = (vector->length - index - 1) * vector->element_size;
    memcpy(dest, src, remainder_size);
    vector->length--;
    return SUCCESS;
}

void vectorDelete(Vector* vector) {
    free(vector->internal);
    vector->internal = 0;
    vector->internal_size = 0;
    vector->element_size = 0;
    vector->length = 0;
}
