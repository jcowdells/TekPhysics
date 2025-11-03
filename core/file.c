#include "file.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/**
 * Return the size of a file in bytes.
 * @param filename The path that leads to the file.
 * @param file_size A pointer to a uint where the size will be recorded.
 * @throws FILE_EXCEPTION if the file does not exist.
 */
exception getFileSize(const char* filename, uint* file_size) {
    // linux allows file size to be checked
    struct stat file_stat;
    if (stat(filename, &file_stat) == -1) tekThrow(FILE_EXCEPTION, "Could not read file stat.");

    // add one to size of file returned to allow for terminating character to be added.
    *file_size = file_stat.st_size + 1;

    return SUCCESS;
}

/**
 * Read a file into an existing buffer.
 * @note Buffer is required to be large enough for the file. Use the 'getFileSize(...)' function to find the size, and then allocate memory.
 * @param filename The path that leads to the file to be read.
 * @param buffer_size The size of the buffer passed in.
 * @param buffer The buffer that has been allocated that will store the contents of the file.
 * @throws FILE_EXCEPTION if the file does not exist, or is larger than what can be stored in the buffer.
 */
exception readFile(const char* filename, const uint buffer_size, char* buffer) {
    // get file pointer to the file
    FILE* file_ptr = fopen(filename, "r");
    if (!file_ptr) tekThrow(FILE_EXCEPTION, "Could not open file.");

    // iterate over each character until end of file is reached
    int c; uint count = 0;
    while ((c = fgetc(file_ptr)) != EOF) {
        buffer[count++] = (char)c;
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

/**
 * Write a string buffer into a file. This will create a file if one doesn't exist, and will overwrite if it does.
 * @param buffer The contents to write to the file.
 * @param filename The path to the file that should be written to.
 * @throws FILE_EXCEPTION
 */
exception writeFile(const char* buffer, const char* filename) {
    // get pointer to file.
    FILE* file_ptr = fopen(filename, "w");
    if (!file_ptr) tekThrow(FILE_EXCEPTION, "Could not open file.");

    // write file
    const int fpf_out = fprintf(file_ptr, "%s", buffer);
    fclose(file_ptr);
    if (fpf_out < 0)
        tekThrow(FILE_EXCEPTION, "Failed to write to file.");

    return SUCCESS;
}

/**
 * Combine a directory and a file name into a single string. For example, directory="/usr/tekphys/" filename="file.txt" -> result="/usr/tekphys/file.txt"
 * @param[in] directory The directory to start with
 * @param[in] filename The file name to finish with
 * @param[out] result A pointer to a char pointer which will be overwritten to point to the new buffer.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 */
exception addPathToFile(const char* directory, const char* filename, char** result) {
    const uint len_directory = strlen(directory);
    const uint len_filename = strlen(filename);
    const uint len_result = len_directory + len_filename + 1;
    *result = (char*)malloc(len_result * sizeof(char));
    if (!*result)
        tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for result.");
    memcpy(*result, directory, len_directory);
    memcpy(*result + len_directory, filename, len_filename);
    (*result)[len_directory + len_filename] = 0;
    return SUCCESS;
}

/**
 * Check if a file exists.
 * @param filename The file to check.
 * @return 0 if it does not exist, 1 if it does.
 */
flag fileExists(const char* filename) {
    FILE* file_ptr = fopen(filename, "r");
    if (file_ptr) {
        fclose(file_ptr);
        return 1;
    }
    return 0;
}
