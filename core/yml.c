#include "yml.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "file.h"
#include "list.h"

#define ID_TOKEN     0b00000001
#define STRING_TOKEN 0b00000010
#define INT_TOKEN    0b00000100
#define FLOAT_TOKEN  0b00001000
#define BOOL_TOKEN   0b00010000
#define LIST_TOKEN   0b00100000
#define IN_TOKEN     0b01000000
#define OUT_TOKEN    0b10000000

#define ID_STAGE  0
#define SEP_STAGE 1
#define VAL_STAGE 2

typedef struct Word {
    const char* start;
    uint length;
    uint indent;
    uint line;
} Word;

typedef struct Token {
    Word* word;
    flag type;
} Token;

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
    flag trace_indent = 1;
    uint indent = 0;
    uint line = 1;
    for (int i = 0; i < buffer_size; i++) {
        if (isWhitespace(buffer[i]) || buffer[i] == ':') {
            if (i > start_index + 1) {
                Word* word = (Word*)malloc(sizeof(Word));
                if (!word) tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for word.");

                word->start = buffer + start_index + 1;
                word->length = i - start_index - 1;
                word->indent = indent;
                word->line = line;
                indent = 0;
                trace_indent = 0;
                tekChainThrow(listAddItem(split_text, word));
            }
            if (buffer[i] == ':') {
                Word* colon = (Word*)malloc(sizeof(Word));
                if (!colon) tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for word.");

                colon->start = buffer + i;
                colon->length = 1;
                colon->indent = 0;
                colon->line = line;
                indent = 0;
                tekChainThrow(listAddItem(split_text, colon));
            }
            if ((buffer[i] == ' ') && (trace_indent)) {
                indent++;
            }
            if (buffer[i] == '\n') {
                line++;
                trace_indent = 1;
            }
            start_index = i;
        }
    }
    return SUCCESS;
}

exception ymlThrowSyntax(const Word* word) {
    char error_message[E_MESSAGE_SIZE];
    sprintf(error_message, "YML syntax error at line %u, in '", word->line);
    const uint len_message = strlen(error_message);
    const uint len_copy = E_MESSAGE_SIZE - len_message - 2 > word->length ? word->length : E_MESSAGE_SIZE - len_message - 2;
    printf("length: %u", word->length);
    memcpy(error_message + len_message, word->start, len_copy);
    error_message[len_message + len_copy] = '\'';
    error_message[len_message + len_copy + 1] = 0;
    tekThrow(YML_EXCEPTION, error_message);
}

exception ymlCreateTokens(List* split_text, List* tokens) {
    ListItem* item = split_text->data;
    uint indent = 0;
    while (item) {
        Word* word = (Word*)item->data;

        if (word->indent > indent) {
            Token* step_token = (Token*)malloc(sizeof(Token));
            step_token->word = 0;
            step_token->type = OUT_TOKEN;
            tekChainThrow(listAddItem(tokens, step_token));
            indent = word->indent;
        }

        if (word->start[0] == ':') tekChainThrow(ymlThrowSyntax(word));
        Word* id_word = word;

        item = item->next;
        if (!item); // TODO: idk

        word = (Word*)item->data;
        if (word->start[0] == ':') {
            // add key token
        } else {
            if (word->indent < indent)
        }

        item = item->next;
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
    List tokens;
    listCreate(&split_text);
    tekChainThrow(ymlSplitText(buffer, file_size, &split_text));
    tekChainThrow(ymlCreateTokens(&split_text, &tokens));

    listDelete(&split_text);

    return SUCCESS;
}