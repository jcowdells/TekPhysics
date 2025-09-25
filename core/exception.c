#include "exception.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "../tekgl.h"

// using fixed size arrays, basically built to not have exceptions because if an exception occurs, there is nothing in place to handle it yet.

flag initialised = 0;
char exception_buffer[E_BUFFER_SIZE]; // stores the actual exception that occurred e.g. "Memory Exception in something.c"
char stack_trace_buffer[STACK_TRACE_BUFFER_SIZE][E_MESSAGE_SIZE]; // stores stack trace e.g. "... in line 21..."
uint stack_trace_index = 0;
char* default_exception = "Unknown Exception";
char* exceptions[NUM_EXCEPTIONS]; // stores name of each exception e.g. "Memory Exception"

/**
 * Add a new exception to the list of named exceptions.
 * @param exception_code The exception code, via one of the exception macros.
 * @param exception_name The human-readable name of the exception.
 */
void tekAddException(const int exception_code, const char* exception_name) {
    const size_t len_exception_name = strlen(exception_name) + 1;
    char* buffer = (char*)malloc(len_exception_name);
    if (!buffer) return;
    memcpy(buffer, exception_name, len_exception_name);
    exceptions[exception_code - 1] = buffer;
}

/**
 * Initialise exceptions, adding human readable names to the list of exception names.
 */
void tekInitExceptions() {
    initialised = 1;
    tekAddException(FAILURE, "Failure");
    tekAddException(MEMORY_EXCEPTION, "Memory Allocation Exception");
    tekAddException(NULL_PTR_EXCEPTION, "Null Pointer Exception");
    tekAddException(GLFW_EXCEPTION, "GLFW Exception");
    tekAddException(GLAD_EXCEPTION, "GLAD Exception");
    tekAddException(FILE_EXCEPTION, "File Exception");
    tekAddException(OPENGL_EXCEPTION, "OpenGL Exception");
    tekAddException(STBI_EXCEPTION, "STBI Exception");
    tekAddException(FREETYPE_EXCEPTION, "FreeType Exception");
    tekAddException(LIST_EXCEPTION, "List Exception");
    tekAddException(YML_EXCEPTION, "YML Exception");
    tekAddException(STACK_EXCEPTION, "Stack Exception");
    tekAddException(HASHTABLE_EXCEPTION, "Hash Table Exception");
    tekAddException(QUEUE_EXCEPTION, "Queue Exception");
    tekAddException(THREAD_EXCEPTION, "Thread Exception");
    tekAddException(VECTOR_EXCEPTION, "Vector Exception");
    tekAddException(ENGINE_EXCEPTION, "Engine Exception");
    tekAddException(BITSET_EXCEPTION, "BitSet Exception");
}

/**
 * Frees all the names of the exceptions that were created.
 */
void tekCloseExceptions() {
    for (int i = 0; i < NUM_EXCEPTIONS; i++) {
        if (exceptions[i]) free(exceptions[i]);
    }
    initialised = 0;
}

/**
 * Get the name of an exception using its numeric code.
 * @param exception_code The exception code to get.
 * @return A pointer to the exception's name
 */
char* tekGetExceptionName(const int exception_code) {
    char* exception = exceptions[exception_code - 1];
    if (!exception) exception = default_exception;
    return exception;
}

/**
 * Print out the last exception that occurred, including stack trace.
 */
void tekPrintException() {
    if (exception_buffer[0])
        printf("%s", exception_buffer);
    else
        printf("Unknown exception!\n");
    for (uint i = 0; i < stack_trace_index; i++) {
        printf("%s", stack_trace_buffer[i]);
    }
}

/**
 * Record that an exception has occurred, based on line number, function and file.
 * @note Should be used mostly with the 'tekThrow(...)' macro.
 * @param exception_code The exception code via one of the macros, e.g. MEMORY_EXCEPTION
 * @param exception_line The line number of the exception, via the __LINE__ macro.
 * @param exception_function The function in which the exception occurred, should use the __FUNCTION__ macro.
 * @param exception_file The file where the exception occurred, should use the __FILE__ macro.
 * @param exception_message A message to describe the error, e.g. malloc() returned a null pointer.
 */
void tekSetException(const int exception_code, const int exception_line, const char* exception_function, const char* exception_file, const char* exception_message) {
    char* exception_name;
    if (initialised) {
        exception_name = tekGetExceptionName(exception_code);
    } else {
        exception_name = default_exception;
    }
    sprintf(exception_buffer, "%s (%d) in function '%s', line %d of %s:\n    %s\n", exception_name, exception_code, exception_function, exception_line, exception_file, exception_message);
    stack_trace_index = 0;
}

/**
 * Function used to add to the stack trace of an exception.
 * @note Should be used only with the 'tekChainThrow(...)' macro.
 * @param exception_line The line number of the exception, via the __LINE__ macro.
 * @param exception_function The function in which the exception occurred, should use the __FUNCTION__ macro.
 * @param exception_file The file where the exception occurred, should use the __FILE__ macro.
 */
void tekTraceException(const int exception_line, const char* exception_function, const char* exception_file) {
    sprintf(stack_trace_buffer[stack_trace_index], "... in function '%s', line %d of %s\n", exception_function, exception_line, exception_file);
    if (stack_trace_index == STACK_TRACE_BUFFER_SIZE - 1) {
        sprintf(stack_trace_buffer[stack_trace_index - 1], "... stack trace too large to display entirely ...\n");
    } else {
        stack_trace_index++;
    }
}