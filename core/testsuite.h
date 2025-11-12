#include "../tekgl.h"
#include "exception.h"

#define MAX_TESTS 16

typedef exception(*TekTestFunc)(void* test_context);

#define tekTestCreate(test_name) exception __tekCreate_##test_name
#define tekTestDelete(test_name) exception __tekDelete_##test_name
#define tekTestFunc(test_name, test_part) exception __tekTest_##test_name##_##test_part

#define tekRunSuite(test_name, test_part, test_context) \
printf("Testing \"%s %s\":\n", #test_name, #test_part); \
tekChainThrow(__tekCreate_##test_name(test_context)); \
tekChainThrow(__tekTest_##test_name##_##test_part(test_context)); \
tekChainThrow(__tekDelete_##test_name(test_context)); \

#define tekAssert(expected, actual) \
if ((expected) == (actual)) { \
    printf("    Test Passed: %s == %s\n", #expected, #actual); \
} else { \
    printf("    ASSERTION EXCEPTION in line %d, function %s\n    Test Failed: %s != %s\n", __LINE__, __FUNCTION__, #expected, #actual); \
    return SUCCESS; \
}