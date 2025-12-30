#include "../tekgl.h"
#include "exception.h"

#define MAX_TESTS 16

typedef exception(*TekTestFunc)(void* test_context);

/**
 * Create a creation function for a suite. Called every test.
 * @param test_name Name of the suite 
 */
#define tekTestCreate(test_name) exception __tekCreate_##test_name
/**
 * Create a cleanup function for a test suite.
 * @param test_name Name of the suite
 */
#define tekTestDelete(test_name) exception __tekDelete_##test_name
/**
 * Create a new function in a test suite.
 * @param test_name Name of suite
 * @param test_part Name of test
 */
#define tekTestFunc(test_name, test_part) exception __tekTest_##test_name##_##test_part

/**
 * Run a suite of tests
 * @param test_name The name of the suite
 * @param test_part The function to run in that suite
 * @param test_context The shared memory struct.
 */
#define tekRunSuite(test_name, test_part, test_context) \
printf("Testing \"%s %s\":\n", #test_name, #test_part); \
tekChainThrow(__tekCreate_##test_name(test_context)); \
tekChainThrow(__tekTest_##test_name##_##test_part(test_context)); \
tekChainThrow(__tekDelete_##test_name(test_context)); \

/**
 * Assert if two things are equal, and print if they are or are not.
 * @param expected
 * @param actual
 * @return 
 */
#define tekAssert(expected, actual) \
if ((expected) == (actual)) { \
    printf("    Test Passed: %s == %s\n", #expected, #actual); \
} else { \
    tekThrow(ASSERT_EXCEPTION, "Test Failed: " #expected " != " #actual "\n"); \
}

/**
 * Assert if two things are equivalent, but only alert if they are not equal.
 * @param expected
 * @param actual
 */
#define tekSilentAssert(expected, actual) \
if ((expected) != (actual)) { \
    tekThrow(ASSERT_EXCEPTION, "Test Failed: " #expected " != " #actual "\n"); \
}