#include "yml.h"

#include <stdio.h>
#include <stdlib.h>

#include "file.h"
#include "list.h"

typedef struct Word {
    const char* start;
    uint length;
} Word;

int isWhitespace(const char c) {
    switch (c) {
        case 0x00:
        case 0x20:
        case 0x09:
        case 0x0D:
        case 0x0A:
            return 1;
        default:
            return 0;
    }
}

exception ymlSplitText(const char* buffer, const uint buffer_size, List* split_text) {
    int start_index = -1;
    for (int i = 0; i < buffer_size; i++) {
        if (isWhitespace(buffer[i]) || buffer[i] == ':') {
            if (i > start_index + 1) {
                Word* word = (Word*)malloc(sizeof(Word));
                if (!word) tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for word.");

                word->start = buffer + start_index + 1;
                word->length = i - start_index - 1;

                listAddItem(split_text, word);
            }
            if (buffer[i] == ':') {
                Word* colon = (Word*)malloc(sizeof(Word));
                if (!colon) tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for word.");

                colon->start = buffer + i;
                colon->length = 1;
                listAddItem(split_text, colon);
            }
            start_index = i;
        }
    }
    return SUCCESS;
}

exception ymlReadFile(const char* filename, YmlFile* yml) {
    // get size of file to read
    uint file_size;
    tekChainThrow(getFileSize(filename, &file_size));

    // mallocate memory for file
    char* buffer = (char*)malloc(file_size * sizeof(char));
    if (!buffer) tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory to read yml file into.");

    // read file
    tekChainThrow(readFile(filename, file_size, buffer));

    List split_text;
    listCreate(&split_text);
    tekChainThrow(ymlSplitText(buffer, file_size, &split_text));
    ListItem* word = split_text.data;
    while (word) {
        Word* word_data = (Word*)word->data;
        for (uint i = 0; i < word_data->length; i++) {
            printf("%c", word_data->start[i]);
        }
        printf("\n");
        word = word->next;
    }

    listDelete(&split_text);

    return SUCCESS;
}