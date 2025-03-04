#pragma once

#define E_MESSAGE_SIZE 128
#define E_BUFFER_SIZE  128 + E_MESSAGE_SIZE

#define SUCCESS            0
#define FAILURE            1
#define MEMORY_EXCEPTION   2
#define NULL_PTR_EXCEPTION 3
#define GLFW_EXCEPTION     4
#define GLAD_EXCEPTION     5
#define FILE_EXCEPTION     6
#define OPENGL_EXCEPTION   7
#define STBI_EXCEPTION     8
#define FREETYPE_EXCEPTION 9
#define NUM_EXCEPTIONS     9

typedef int exception;

#define tekThrow(exception_code, exception_message) { const exception __tek_exception = exception_code; if (__tek_exception) { tekSetException(__tek_exception, __LINE__, __FUNCTION__, __FILE__, exception_message); return __tek_exception; } }
#define tekChainThrow(exception_code) { const exception __tek_exception = exception_code; if (__tek_exception) return __tek_exception; }

void tekInitExceptions();
void tekCloseExceptions();
const char* tekGetException();
void tekPrintException();
void tekSetException(int exception_code, int exception_line, const char* exception_function, const char* exception_file, const char* exception_message);