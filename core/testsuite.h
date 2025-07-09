#include "../tekgl.h"

typedef void (*testFunc)(void);

#define tekCreateSuite(test_name, test_create, test_delete, ...) \
uint test_name##_len   = \
uint test_name##_array = \


#define tekAssert(expected, actual) \
if ((expected) == (actual)) { \
    printf("    Test Passed: %s == %s\n", #expected, #actual); \
} else { \
    printf("    ASSERTION EXCEPTION in line %d, function %s\n    Test Failed: %s != %s\n", __LINE__, __FUNCTION__, #expected, #actual); \
} \
