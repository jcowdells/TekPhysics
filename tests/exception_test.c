#include "exception_test.h"

#include <alloca.h>
#include <stdio.h>

#include "../core/hashtable.h"
#include "../core/vector.h"

static exception functionA() {
    printf("Running function A!\n");
    HashTable hashtable = {};
    tekChainThrow(hashtableCreate(&hashtable, 1));

    Vector vector = {};
    tekChainThrow(vectorCreate(4, sizeof(int), &vector));

    int* data = alloca(sizeof(int));
    *data = 100;
    tekChainThrow(hashtableSet(&hashtable, "test_key", data));
    tekChainThrow(hashtableGet(&hashtable, "test_key", &data));

    tekChainThrow(vectorAddItem(&vector, data));
    tekChainThrow(vectorGetItem(&vector, 0, data));

    hashtableDelete(&hashtable);
    vectorDelete(&vector);

    printf("Finished function A!\n");

    return SUCCESS;
}

static exception functionB() {
    printf("Running function B!\n");

    // tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory.");
    // tekThrow(NULL_PTR_EXCEPTION, "Null pointer.");
    // tekThrow(FAILURE, "Insert useful message here.");

    printf("Finished function B!\n");

    return SUCCESS;
}

static exception functionC() {
    printf("Running function C!\n");

    tekChainThrow(functionA());
    tekChainThrow(functionB());

    printf("Finished function C!\n");

    return SUCCESS;
}

exception tekRunExceptionTest() {
    tekChainThrow(functionC());

    return SUCCESS;
}
