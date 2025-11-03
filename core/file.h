#pragma once

#include "../tekgl.h"
#include "exception.h"

exception getFileSize(const char* filename, uint* file_size);
exception readFile(const char* filename, uint buffer_size, char* buffer);
exception writeFile(const char* buffer, const char* filename);
exception addPathToFile(const char* directory, const char* filename, char** result);
flag fileExists(const char* filename);
