#pragma once

#define E_MESSAGE_SIZE          128
#define E_BUFFER_SIZE           128 + E_MESSAGE_SIZE
#define STACK_TRACE_BUFFER_SIZE 16

#define SUCCESS             0
#define FAILURE             1
#define MEMORY_EXCEPTION    2
#define NULL_PTR_EXCEPTION  3
#define GLFW_EXCEPTION      4
#define GLAD_EXCEPTION      5
#define FILE_EXCEPTION      6
#define OPENGL_EXCEPTION    7
#define STBI_EXCEPTION      8
#define FREETYPE_EXCEPTION  9
#define LIST_EXCEPTION      10
#define YML_EXCEPTION       11
#define STACK_EXCEPTION     12
#define HASHTABLE_EXCEPTION 13
#define QUEUE_EXCEPTION     14
#define THREAD_EXCEPTION    15
#define VECTOR_EXCEPTION    16
#define NUM_EXCEPTIONS      16

typedef int exception;

#define tekExcept(exception_code, exception_message) { const exception __tek_exception = exception_code; if (__tek_exception) { tekSetException(__tek_exception, __LINE__, __FUNCTION__, __FILE__, exception_message); } }
#define tekChainExcept(exception_code) { const exception __tek_exception = exception_code; if (__tek_exception) { tekTraceException(__LINE__, __FUNCTION__, __FILE__); } }
#define tekChainBreak(exception_code) { const exception __tek_exception = exception_code; if (__tek_exception) { tekTraceException(__LINE__, __FUNCTION__, __FILE__); break; } }
#define tekThrow(exception_code, exception_message) { const exception __tek_exception = exception_code; if (__tek_exception) { tekSetException(__tek_exception, __LINE__, __FUNCTION__, __FILE__, exception_message); return __tek_exception; } }
#define tekChainThrow(exception_code) { const exception __tek_exception = exception_code; if (__tek_exception) { tekTraceException(__LINE__, __FUNCTION__, __FILE__); return __tek_exception; } }
#define tekLog(exception_code) { const exception __tek_exception = exception_code; if (__tek_exception) { tekTraceException(__LINE__, __FUNCTION__, __FILE__); tekPrintException(); } }

#define tekThrowThen(exception_code, exception_message, then) { const exception __tek_exception = exception_code; if (__tek_exception) { tekSetException(__tek_exception, __LINE__, __FUNCTION__, __FILE__, exception_message); then; return __tek_exception; } }
#define tekChainThrowThen(exception_code, then) { const exception __tek_exception = exception_code; if (__tek_exception) { tekTraceException(__LINE__, __FUNCTION__, __FILE__); then; return __tek_exception; } }

void tekInitExceptions();
void tekCloseExceptions();
const char* tekGetException();
void tekPrintException();
void tekSetException(int exception_code, int exception_line, const char* exception_function, const char* exception_file, const char* exception_message);
void tekTraceException(int exception_line, const char* exception_function, const char* exception_file);
