#pragma once

#include "../tekgl.h"
#include "exception.h"

exception getFileSize(const char* filename, uint* file_size);
exception readFile(const char* filename, uint buffer_size, char* buffer);