#include "unit_test.h"

#include <stdio.h>
#include <string.h>

#include "../core/testsuite.h"

#include "../core/vector.h"

typedef union TestContext {
    Vector vector;
} TestContext;

tekTestCreate(vector) (TestContext* _) {
    return SUCCESS;
}

tekTestDelete(vector) (TestContext* test_context) {
    vectorDelete(&test_context->vector);
    return SUCCESS;
}

tekTestFunc(vector, add_one_int) (TestContext* test_context) {
    // create a vector to store ints
    tekChainThrow(vectorCreate(1, sizeof(int), &test_context->vector));

    // add an integer to vector
    const int number = 12345;
    tekChainThrow(vectorAddItem(&test_context->vector, &number));

    // confirm that the integer has been stored. if working correctly, should be at the first index of the internal array.
    const int* internal = test_context->vector.internal;
    tekAssert(number, internal[0]);

    return SUCCESS;
}

tekTestFunc(vector, add_ten_ints) (TestContext* test_context) {
    // create a vector to store ints
    tekChainThrow(vectorCreate(1, sizeof(int), &test_context->vector));

    // add 10 integers to the vector
    const int numbers[] = {
        1388, 7609, 7550, 603, 1161, 6109, 6706, 5413, 7497, 4128
    };
    const uint len_numbers = sizeof(numbers) / sizeof(int);
    for (uint i = 0; i < len_numbers; i++) {
        tekChainThrow(vectorAddItem(&test_context->vector, &numbers[i]));
    }

    // confirm that the ten integers have been stored.
    const int* internal = test_context->vector.internal;
    tekAssert(0, memcmp(numbers, internal, sizeof(numbers)));

    return SUCCESS;
}

exception tekUnitTest() {
    TestContext test_context = {};
    tekRunSuite(vector, add_one_int, &test_context);
    tekRunSuite(vector, add_ten_ints, &test_context);

    return SUCCESS;
}