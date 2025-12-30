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
    const size_t len_exception_name = strlen(exception_name) + 1; // length of string + null terminator
    char* buffer = (char*)malloc(len_exception_name);
    if (!buffer) return; // cant throw an exception cuz no exception handler yet... oopsies
    memcpy(buffer, exception_name, len_exception_name);
    exceptions[exception_code - 1] = buffer;
}

/**
 * Initialise exceptions, adding human readable names to the list of exception names.
 */
void tekInitExceptions() {
    // in no particular order (in the order i realised that that thing could fail) (drumroll please)
    // default exception
    tekAddException(FAILURE, "Failure");

    // C issues
    tekAddException(MEMORY_EXCEPTION, "Memory Allocation Exception");
    tekAddException(NULL_PTR_EXCEPTION, "Null Pointer Exception");

    // graphics + os stuff
    tekAddException(GLFW_EXCEPTION, "GLFW Exception");
    tekAddException(GLAD_EXCEPTION, "GLAD Exception");
    tekAddException(FILE_EXCEPTION, "File Exception");
    tekAddException(OPENGL_EXCEPTION, "OpenGL Exception");
    tekAddException(STBI_EXCEPTION, "STBI Exception");
    tekAddException(FREETYPE_EXCEPTION, "FreeType Exception");
    
    // data structures + my code
    tekAddException(LIST_EXCEPTION, "List Exception");
    tekAddException(YML_EXCEPTION, "YML Exception");
    tekAddException(STACK_EXCEPTION, "Stack Exception");
    tekAddException(HASHTABLE_EXCEPTION, "Hash Table Exception");
    tekAddException(QUEUE_EXCEPTION, "Queue Exception");
    tekAddException(THREAD_EXCEPTION, "Thread Exception");
    tekAddException(VECTOR_EXCEPTION, "Vector Exception");
    tekAddException(ENGINE_EXCEPTION, "Engine Exception");
    tekAddException(BITSET_EXCEPTION, "BitSet Exception");

    // unit testing
    tekAddException(ASSERT_EXCEPTION, "Assertion Exception");
    initialised = 1;
}

/**
 * Frees all the names of the exceptions that were created.
 */
void tekCloseExceptions() {
    // exception names are dynamically allocated for some reason idk
    for (int i = 0; i < NUM_EXCEPTIONS; i++) {
        if (exceptions[i]) free(exceptions[i]);
    }
    initialised = 0;
}

/**
 * Get the name of an exception using its numeric code.
 * @param exception_code The exception code to get.
 * @return A pointer to the exception's name
 * @note Will return a default name if the exception code is not valid, or the exception handler is not initialised.
 */
char* tekGetExceptionName(const int exception_code) {
    if (!initialised || exception_code > NUM_EXCEPTIONS || exception_code <= 0)
        return default_exception;
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
        printf("Unknown exception!\n"); // had some issues before where i tekChainThrow'ed some non-exception-returning functions and it tried to print an exception that wasn't there. This makes it more clear that it happened.
    // print stack trace
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
    // write into the error buffer up to the maximum size of the buffer.
    snprintf(exception_buffer, E_BUFFER_SIZE, "%s (%d) in function '%s', line %d of %s:\n    %s\n", tekGetExceptionName(exception_code), exception_code, exception_function, exception_line, exception_file, exception_message);
    // if message too long, not space for nullptr at end, so possibly add it
    if (exception_buffer[E_BUFFER_SIZE - 2] != 0) {
        exception_buffer[E_BUFFER_SIZE - 2] = '\n';
        exception_buffer[E_BUFFER_SIZE - 1] = 0;
    }

    // restart stack trace buffer
    stack_trace_index = 0;
}

/**
 * Function used to add to the stack trace of an exception.
 * @note Should be used only with the 'tekChainThrow(...)' macro.
 * @param exception_code The exception code, e.g. MEMORY_EXCEPTION
 * @param exception_line The line number of the exception, via the __LINE__ macro.
 * @param exception_function The function in which the exception occurred, should use the __FUNCTION__ macro.
 * @param exception_file The file where the exception occurred, should use the __FILE__ macro.
 */
void tekTraceException(const int exception_code, const int exception_line, const char* exception_function, const char* exception_file) {
    // Write out the exception as long as possible in the message buffer.
    snprintf(stack_trace_buffer[stack_trace_index], E_MESSAGE_SIZE,  "... %s (%d) in function '%s', line %d of %s\n", tekGetExceptionName(exception_code), exception_code, exception_function, exception_line, exception_file);
    // if we blew right past the end of the buffer, the last char will not be null.
    // so just chop off at the end
    if (stack_trace_buffer[stack_trace_index][E_MESSAGE_SIZE - 2] != 0) {
        stack_trace_buffer[stack_trace_index][E_MESSAGE_SIZE - 2] = '\n';
        stack_trace_buffer[stack_trace_index][E_MESSAGE_SIZE - 1] = 0;
    }

    // if the stack trace buffer is full, then just have to ignore this one to show more important stack trace
    if (stack_trace_index == STACK_TRACE_BUFFER_SIZE - 1) {
        snprintf(stack_trace_buffer[stack_trace_index - 1], E_MESSAGE_SIZE, "... stack trace too large to display entirely ...\n");
    } else {
        stack_trace_index++;
    }
}
