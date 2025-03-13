#include "exception.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "../tekgl.h"

flag initialised = 0;
char exception_buffer[E_BUFFER_SIZE];
char stack_trace_buffer[STACK_TRACE_BUFFER_SIZE][E_MESSAGE_SIZE];
uint stack_trace_index = 0;
flag stack_trace_overflowed = 0;
char* default_exception = "Unknown Exception";
char* exceptions[NUM_EXCEPTIONS];

void tekAddException(const int exception_code, const char* exception_name) {
    const size_t len_exception_name = strlen(exception_name) + 1;
    char* buffer = (char*)malloc(len_exception_name);
    memcpy(buffer, exception_name, len_exception_name);
    exceptions[exception_code - 1] = buffer;
}

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
}

void tekCloseExceptions() {
    for (int i = 0; i < NUM_EXCEPTIONS; i++) {
        if (exceptions[i]) free(exceptions[i]);
    }
    initialised = 0;
}

char* tekGetExceptionName(const int exception_code) {
    char* exception = exceptions[exception_code - 1];
    if (!exception) exception = default_exception;
    return exception;
}

const char* tekGetException() {
    return exception_buffer;
}

void tekPrintException() {
    printf("%s", exception_buffer);
    for (uint i = 0; i < stack_trace_index; i++) {
        printf("%s", stack_trace_buffer[i]);
    }
}

void tekSetException(const int exception_code, const int exception_line, const char* exception_function, const char* exception_file, const char* exception_message) {
    char* exception_name;
    if (initialised) {
        exception_name = tekGetExceptionName(exception_code);
    } else {
        exception_name = default_exception;
    }
    sprintf(exception_buffer, "%s (%d) in function '%s', line %d of %s:\n    %s\n", exception_name, exception_code, exception_function, exception_line, exception_file, exception_message);
    stack_trace_index = 0;
    stack_trace_overflowed = 0;
}

void tekTraceException(const int exception_line, const char* exception_function, const char* exception_file) {
    sprintf(stack_trace_buffer[stack_trace_index], "... in function '%s', line %d of %s\n", exception_function, exception_line, exception_file);
    if ((stack_trace_index == STACK_TRACE_BUFFER_SIZE - 1) && !stack_trace_overflowed) {
        sprintf(stack_trace_buffer[stack_trace_index - 1], "... stack trace too large to display entirely ...\n");
        stack_trace_overflowed = 1;
    } else {
        stack_trace_index++;
    }
}