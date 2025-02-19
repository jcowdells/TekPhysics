#include "exception.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "../tekgl.h"

flag initialised = 0;
char exception_buffer[E_BUFFER_SIZE];
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
}

void tekSetException(const int exception_code, const int exception_line, const char* exception_function, const char* exception_file, const char* exception_message) {
    char* exception_name;
    if (initialised) {
        exception_name = tekGetExceptionName(exception_code);
    } else {
        exception_name = default_exception;
    }
    sprintf(exception_buffer, "%s (%d) in function '%s', line %d of %s:\n    %s\n", exception_name, exception_code, exception_function, exception_line, exception_file, exception_message);
}