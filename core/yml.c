#include "yml.h"

#include <stdlib.h>

#include "file.h"



exception ymlReadFile(const char* filename, YmlFile* yml) {
    // get size of file to read
    uint file_size;
    tekChainThrow(getFileSize(filename, &file_size));

    // mallocate memory for file
    char* buffer = (char*)malloc(file_size * sizeof(char));
    if (!buffer) tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory to read yml file into.");

    // read file
    tekChainThrow(readFile(filename, file_size, buffer));



    return SUCCESS;
}