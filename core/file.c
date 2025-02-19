#include "file.h"

#include <stdio.h>
#include <sys/stat.h>

exception getFileSize(const char* filename, uint* file_size) {
    // linux allows file size to be checked
    struct stat file_stat;
    if (stat(filename, &file_stat) == -1) tekThrow(FILE_EXCEPTION, "Could not read file stat.");

    // add one to size of file returned to allow for terminating character to be added.
    *file_size = file_stat.st_size + 1;

    return SUCCESS;
}

exception readFile(const char* filename, const uint buffer_size, char* buffer) {
    // get file pointer to the file
    FILE* file_ptr = fopen(filename, "r");
    if (!file_ptr) tekThrow(FILE_EXCEPTION, "Could not open file.");

    // iterate over each character until end of file is reached
    char c; uint count = 0;
    while ((c = fgetc(file_ptr)) != EOF) {
        buffer[count++] = c;
        if (count == buffer_size) {
            fclose(file_ptr);
            tekThrow(FILE_EXCEPTION, "File contents larger than buffer.");
        }
    }

    // add terminating character and close file
    buffer[count] = 0;
    fclose(file_ptr);

    return SUCCESS;
}