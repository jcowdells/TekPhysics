#include "vector.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Create a vector / resizing array
 * @param start_capacity The number of items that the vector can initially store (use 1 if unsure)
 * @param element_size The size of each element, use 'sizeof(TheTypeIWannaUse)' here.
 * @param vector A pointer to a Vector struct to be used.
 * @throws VECTOR_EXCEPTION if the element size is 0
 * @throws MEMORY_EXCEPTION if the initial list could not be allocated.
 */
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

/**
 * Directly write an item to the vector.
 * @note Doesn't perform any safety checks, only used internally to avoid repeating code.
 * @param vector The vector to write into.
 * @param index The index of the element to write.
 * @param item The item to write into the vector.
 */
static void vectorWriteItem(const Vector* vector, const uint index, const void* item) {
    void* dest = vector->internal + index * vector->element_size;
    memcpy(dest, item, vector->element_size);
}

/**
 * Increase the capacity of the vector by a factor of 2.
 * @param vector The vector to increase the size of.
 * @throws MEMORY_EXCEPTION if extra memory could not be allocated.
 */
static exception vectorDoubleCapacity(Vector* vector) {
    const uint new_size = vector->internal_size * 2;
    void* internal = vector->internal;
    void* temp = (void*)realloc(internal, new_size * vector->element_size);
    if (!temp) tekThrowThen(MEMORY_EXCEPTION, "Failed to allocate more memory to grow vector.", free(internal));
    vector->internal = temp;
    vector->internal_size = new_size;
    return SUCCESS;
}

/**
 * Add an item to the vector.
 * @note The item WILL be copied into the vector, you can safely free anything added to vector afterwards and keep the data here.
 * @param vector The vector to add the item to.
 * @param item The item that should be copied into the vector.
 * @throws MEMORY_EXCEPTION if the vector required a resize and this failed.
 */
exception vectorAddItem(Vector* vector, const void* item) {
    if (vector->length >= vector->internal_size)
        tekChainThrow(vectorDoubleCapacity(vector));
    vectorWriteItem(vector, vector->length, item);
    vector->length++;
    return SUCCESS;
}

/**
 * Set an item in the vector at a certain index.
 * @note The item WILL be copied into the vector, you can safely free anything added to vector afterwards and keep the data here.
 * @param vector The vector to operate on.
 * @param index The index of the item that should be set.
 * @param item The new data to store at this index.
 * @throws VECTOR_EXCEPTION if the index is out of bounds.
 */
exception vectorSetItem(const Vector* vector, const uint index, const void* item) {
    if (index >= vector->length) tekThrow(VECTOR_EXCEPTION, "Attempted to set index out of bounds.");
    vectorWriteItem(vector, index, item);
    return SUCCESS;
}

/**
 * Get an item from the vector.
 * @note The pointer to the item shoud be a buffer large enough to contain the retrieved item. Works best to use the struct you used when setting the element size at initialisation.
 * @param vector The vector to get an item from.
 * @param index The index of the item to get from the vector.
 * @param item A pointer to where the item should be stored.
 * @throws VECTOR_EXCEPTION if the index is out of bounds.
 */
exception vectorGetItem(const Vector* vector, const uint index, void* item) {
    if (index >= vector->length) tekThrow(VECTOR_EXCEPTION, "Attempted to get index out of bounds.");
    const void* src = vector->internal + index * vector->element_size;
    memcpy(item, src, vector->element_size);
    return SUCCESS;
}

/**
 * Get the pointer to an item in the list.
 * @note Can be better than using \ref vectorGetItem as it avoids making a copy of the data. But if a copy is needed, use the other version.
 * @param vector The vector to get the item pointer from.
 * @param index The index of the item to get.
 * @param item A pointer to where the item pointer should be stored.
 * @throws VECTOR_EXCEPTION if the index is out of bounds.
 */
exception vectorGetItemPtr(const Vector* vector, const uint index, void** item) {
    if (index >= vector->length) tekThrow(VECTOR_EXCEPTION, "Attempted to get index out of bounds.");
    *item = vector->internal + index * vector->element_size;
    return SUCCESS;
}

/**
 * Remove an item from the vector.
 * @note This will shift all the other items in the vector down to fill the gap that is left. If you want to avoid this, just use \ref vectorSetItem with a zeroed struct.
 * @param vector The vector to remove the item from.
 * @param index The index of the item to be removed.
 * @param item A pointer to where the removed item should be stored. Needs to point to a block of memory big enough to store the item. Set to NULL if item is not needed.
 * @throws VECTOR_EXCEPTION if the index is out of bounds.
 */
exception vectorRemoveItem(Vector* vector, const uint index, void* item) {
    if (index >= vector->length) tekThrow(VECTOR_EXCEPTION, "Attempted to get index out of bounds.");
    if (item) tekChainThrow(vectorGetItem(vector, index, item));
    void* dest = vector->internal + index * vector->element_size;
    const void* src = dest + vector->element_size;
    const uint remainder_size = (vector->length - index - 1) * vector->element_size;
    memcpy(dest, src, remainder_size);
    vector->length--;
    return SUCCESS;
}

/**
 * Pop an item from the vector, e.g. return last item and remove it from the vector.
 * @param vector The vector to pop from.
 * @param item A pointer to where the item will be stored. Must point to a buffer large enough to store the returned data.
 * @note Internally uses \ref vectorGetItem, check for more info.
 * @returns 1 if successful, 0 if the vector is empty.
 */
flag vectorPopItem(Vector* vector, void* item) {
    if (vector->length == 0) {
        memset(item, 0, vector->element_size);
        return 0;
    };
    // allowed to not check this error, can only fail if index out of range, and the by using length - 1, it must be in range.
    // don't need to use vectorRemoveItem(...) because we are taking out the last item, so nothing needs to be moved around in the vector.
    // new writes should overwrite the old data.
    vectorGetItem(vector, vector->length - 1, item);
    vector->length--;
    return 1;
}

/**
 * Insert an item into a vector at a certain index.
 * @param vector The vector in which to insert the item.
 * @param index The index at which to insert the new item.
 * @param item The item which is to be inserted.
 * @throws VECTOR_EXCEPTION if the index is out of bounds.
 * @throws MEMORY_EXCEPTION if malloc() failed.
 */
exception vectorInsertItem(Vector* vector, const uint index, const void* item) {
    if (index > vector->length) tekThrow(VECTOR_EXCEPTION, "Attempted to set index out of bounds.");
    if (index == vector->length) {
        tekChainThrow(vectorAddItem(vector, item));
        return SUCCESS;
    }

    if (vector->length >= vector->internal_size)
        tekChainThrow(vectorDoubleCapacity(vector));

    void* src = vector->internal + index * vector->element_size;
    void* dest = src + vector->element_size;
    const uint size = (vector->length - index) * vector->element_size;
    memcpy(dest, src, size);
    vectorWriteItem(vector, index, item);
    vector->length++;
    return SUCCESS;
}

/**
 * Set all items in the vector to NULL and reset the length to 0.
 * @param vector The vector to clear out.
 * @note This will maintain the previous internal capacity of the vector.
 */
void vectorClear(Vector* vector) {
    memset(vector->internal, 0, vector->internal_size * vector->element_size);
    vector->length = 0;
}

/**
 * Delete a vector, freeing the internal list and zeroing the struct.
 * @param vector The vector to delete.
 */
void vectorDelete(Vector* vector) {
    free(vector->internal);
    vector->internal = 0;
    vector->internal_size = 0;
    vector->element_size = 0;
    vector->length = 0;
}
