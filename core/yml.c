#include "yml.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

#include "file.h"
#include "list.h"
#include "stack.h"

#define YML_HASHTABLE_SIZE 4

#define VAR_TOKEN    0b000
#define ID_TOKEN     0b001
#define LIST_TOKEN   0b010
#define IN_TOKEN     0b011
#define OUT_TOKEN    0b100

#define STRING_TOKEN  0b00001000
#define INTEGER_TOKEN 0b00010000
#define FLOAT_TOKEN   0b00100000
#define BOOL_TOKEN    0b01000000
#define TYPE_MASK     0b01111000

#define YML_DATA     0
#define STRING_DATA  1
#define INTEGER_DATA 2
#define FLOAT_DATA   3
#define LIST_DATA    4

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

exception ymlWordToString(const Word* word, char** string) {
    // allocate memory for string
    *string = (char*)malloc(word->length + 1);
    if (!(*string)) tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for word string.");

    // copy in string data from token
    memcpy(*string, word->start, word->length);

    // add null termminator character
    (*string)[word->length] = 0;

    return SUCCESS;
}

exception ymlCreateStringData(const char* string, YmlData** yml_data) {
    // allocate some memory for yml data
    *yml_data = (YmlData*)malloc(sizeof(YmlData));
    if (!(*yml_data)) tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for string data.");

    // set type of data to be string
    (*yml_data)->type = STRING_DATA;

    // allocate memory for said string
    char* string_copy = (char*)malloc(strlen(string) + 1);
    if (!string_copy) tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for string data.");

    // copy in string data
    memcpy(string_copy, string, strlen(string) + 1);

    // set value ptr of data to be the string
    (*yml_data)->value = string_copy;
    return SUCCESS;
}

exception ymlCreateStringDataToken(const Token* token, YmlData** yml_data) {
    // allocate some memory for yml data
    *yml_data = (YmlData*)malloc(sizeof(YmlData));
    if (!(*yml_data)) tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for string data.");

    // set type of data to be string
    (*yml_data)->type = STRING_DATA;

    // allocate memory for said string
    char* string;
    ymlWordToString(token->word, &string);

    // set value ptr of data to be the string
    (*yml_data)->value = string;
    return SUCCESS;
}

exception ymlCreateIntegerData(const long integer, YmlData** yml_data) {
    // allocate some memory for yml data
    *yml_data = (YmlData*)malloc(sizeof(YmlData));
    if (!(*yml_data)) tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for integer data.");
    (*yml_data)->type = INTEGER_DATA;
    (*yml_data)->value = (void*)integer;
    return SUCCESS;
}

exception ymlCreateIntegerDataToken(const Token* token, YmlData** yml_data) {
    // allocate some memory for yml data
    *yml_data = (YmlData*)malloc(sizeof(YmlData));
    if (!(*yml_data)) tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for integer data.");
    (*yml_data)->type = INTEGER_DATA;

    // allocate memory to store integer in a string
    char* string;
    ymlWordToString(token->word, &string);

    // convert string to be an integer, and check for errors
    const long integer = strtol(string, NULL, 10);
    const int error = errno;
    free(string);
    if (error == ERANGE) tekThrow(YML_EXCEPTION, "Integer is either too small or large.");

    // set value ptr of data to be the number
    (*yml_data)->value = (void*)integer;
    return SUCCESS;
}

exception ymlCreateFloatData(const double number, YmlData** yml_data) {
    // allocate some memory for yml data
    *yml_data = (YmlData*)malloc(sizeof(YmlData));
    if (!(*yml_data)) tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for integer data.");
    (*yml_data)->type = FLOAT_DATA;
    memcpy(&(*yml_data)->value, &number, sizeof(void*));
    return SUCCESS;
}

exception ymlCreateFloatDataToken(const Token* token, YmlData** yml_data) {
    // allocate some memory for yml data
    *yml_data = (YmlData*)malloc(sizeof(YmlData));
    if (!(*yml_data)) tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for integer data.");
    (*yml_data)->type = FLOAT_DATA;

    // allocate memory to store number in a string
    char* string;
    ymlWordToString(token->word, &string);

    // convert string to be a number, and check for errors
    const double number = strtod(string, NULL);
    const int error = errno;
    free(string);
    if (error == ERANGE) tekThrow(YML_EXCEPTION, "Number is either too small or large.");

    // set value ptr of data to be the number
    memcpy(&(*yml_data)->value, &number, sizeof(void*));
    return SUCCESS;
}

exception ymlCreateAutoDataToken(const Token* token, YmlData** yml_data) {
    switch (token->type & TYPE_MASK) {
        case STRING_TOKEN:
            tekChainThrow(ymlCreateStringDataToken(token, yml_data));
            break;
        case INTEGER_TOKEN:
            tekChainThrow(ymlCreateIntegerDataToken(token, yml_data));
            break;
        case FLOAT_TOKEN:
            tekChainThrow(ymlCreateFloatDataToken(token, yml_data));
            break;
        default:
            tekThrow(YML_EXCEPTION, "Bad token type.");
    }
    return SUCCESS;
}

exception ymlCreateListData(List* list, YmlData** yml_data) {
    // allocate some memory for yml data
    *yml_data = (YmlData*)malloc(sizeof(YmlData));
    if (!(*yml_data)) tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for integer data.");
    (*yml_data)->type = LIST_DATA;
    (*yml_data)->value = (void*)list;
    return SUCCESS;
}

void ymlDeleteData(YmlData* yml_data) {
    if (yml_data->value) {
        switch (yml_data->type) {
            case STRING_DATA:
                free(yml_data->value);
                break;
            case LIST_DATA:
                List* yml_list = yml_data->value;
                const ListItem* item = yml_list->data;
                while (item) {
                    YmlData* list_data = item->data;
                    ymlDeleteData(list_data);
                    item = item->next;
                }
                listDelete(yml_list);
            default:
                break;
        }
    }
    free(yml_data);
}

exception ymlDelete(YmlFile* yml) {
    if (!yml) tekThrow(NULL_PTR_EXCEPTION, "Cannot free null pointer.");

    // create pointer to store list of values
    YmlData** values;

    // get number of values
    const uint num_items = yml->num_items;

    // get values from hashtable
    tekChainThrow(hashtableGetValues(yml, &values));
    exception tek_exception = SUCCESS;

    // iterate over the values we got
    for (uint i = 0; i < num_items; i++) {
        // if this is a pointer to another hashtable, we need to recursively free it
        if (values[i]->type == YML_DATA) {
            HashTable* hashtable = values[i]->value;
            tek_exception = ymlDelete(hashtable);
            if (tek_exception) break;
            free(values[i]);
        // otherwise, just delete as normal
        } else {
            ymlDeleteData(values[i]);
        }

    }

    // clean up the array we used
    free(values);
    tekChainThrow(tek_exception);
    return SUCCESS;
}

exception ymlCreate(YmlFile* yml) {
    // simply need to create the hashtable which the yml file needs to work
    // this is just the root hash table
    tekChainThrow(hashtableCreate(yml, YML_HASHTABLE_SIZE));
    return SUCCESS;
}

exception ymlGetList(YmlFile* yml, YmlData** data, const List* keys) {
    // initial hashtable pointer
    HashTable* hashtable = yml;

    // place to store data we get during the loop
    YmlData* loop_data;

    // store any exceptions that occur during the loop
    exception tek_exception = SUCCESS;
    const ListItem* key = keys->data;
    while (key) {
        // say we had something like
        // key: "value"
        // and i tried to do yml[key][something]
        // obviously that can't work, so don't allow access if not a hashtable
        if (!hashtable) {
            tek_exception = YML_EXCEPTION;
            tekExcept(tek_exception, "Invalid key - inaccessible type.");
            break;
        }
        
        // get the data stored in the hashtable, and make sure that it is a valid key
        tek_exception = hashtableGet(hashtable, key->data, &loop_data);
        if (tek_exception) {
            tekExcept(tek_exception, "Invalid key - key does not exist.");
            break;
        }

        // if the data stored is type 'yml data', then a hashtable is stored there
        // set this as the new hashtable
        if (loop_data->type == YML_DATA) {
            hashtable = loop_data->value;
        }

        key = key->next;
    }

    // write data and throw any exceptions if they occurred
    tekChainThrow(tek_exception);
    *data = loop_data;
    return SUCCESS;
}

exception ymlGetVA(YmlFile* yml, YmlData** data, ...) {
    // separate method. should be called from a macro ymlGet
    // the macro will force the final argument to be null

    // start variadic arguments
    va_list keys;
    va_start(keys, data);
    const char* key;

    List keys_list = {};
    listCreate(&keys_list);

    while ((key = va_arg(keys, const char*))) {
        listAddItem(&keys_list, key);
    }

    // finish variadic arguments
    va_end(keys);

    // get item and throw any exceptions if they occurred
    exception tek_exception = ymlGetList(yml, data, &keys_list);
    listDelete(&keys_list);
    
    return tek_exception;
}

exception ymlSetList(YmlFile* yml, YmlData* data, const List* keys) {
    // set up some variables
    const ListItem* key = keys->data;

    HashTable* hashtable = yml;
    YmlData* loop_data;
    exception tek_exception = SUCCESS;

    // if the next key is 0, we don't want to process this
    // hence the use of the separate key and next_key variables
    while (key->next) {
        // get the data stored at current key
        tek_exception = hashtableGet(hashtable, key->data, &loop_data);

        // if there is another hash table at that key
        if (!tek_exception && (loop_data->type == YML_DATA)) {
            // update searched hash table and increment key
            hashtable = loop_data->value;
            key = key->next;

            // go to next iteration
            continue;
        }

        loop_data = 0;

        // if there isn't a hashtable there, assume that one should be created
        HashTable* next_hashtable = (HashTable*)malloc(sizeof(HashTable));
        if (!next_hashtable) {
            tek_exception = MEMORY_EXCEPTION;
            tekExcept(tek_exception, "Failed to allocate memory for new hash table.");
            break;
        }

        // init the hashtable if no memory errors happened
        hashtableCreate(next_hashtable, YML_HASHTABLE_SIZE);

        // create a yml data struct to store the hashtable pointer
        YmlData* yml_data = (YmlData*)malloc(sizeof(YmlData));
        if (!yml_data) {
            tek_exception = MEMORY_EXCEPTION;
            tekExcept(tek_exception, "Failed to allocate memory for new yml data.");
            break;
        }

        // fill with necessary data if all went well
        yml_data->type = YML_DATA;
        yml_data->value = next_hashtable;

        // delete the old data that was stored here, in the appropriate way
        if (loop_data && !tek_exception) {
            if (loop_data->type == YML_DATA) {
                tek_exception = ymlDelete(loop_data->value);
                if (tek_exception) break;
                //free(loop_data);
            } else {
                ymlDeleteData(loop_data);
            }
        }

        // set the hashtable at this index
        tek_exception = hashtableSet(hashtable, key->data, yml_data);
        if (tek_exception) break;

        // update hashtable pointer and increment to the next key
        hashtable = next_hashtable;
        key = key->next;
    }

    // check for errors, and then set the data at the correct key
    tekChainThrow(tek_exception);
    tekChainThrow(hashtableSet(hashtable, key->data, data));
    return tek_exception;
}


exception ymlSetVA(YmlFile* yml, YmlData* data, ...) {
    // separate method. should be called from a macro ymlGet
    // the macro will force the final argument to be null

    // start variadic arguments
    va_list keys;
    va_start(keys, data);
    const char* key;

    List keys_list = {};
    listCreate(&keys_list);

    while ((key = va_arg(keys, const char*))) {
        listAddItem(&keys_list, key);
    }

    // finish variadic arguments
    va_end(keys);

    // get item and throw any exceptions if they occurred
    exception tek_exception = ymlSetList(yml, data, &keys_list);
    listDelete(&keys_list);
    
    return tek_exception;
}

exception ymlRemoveList(YmlFile* yml, const List* keys) {
    const ListItem* key = keys->data;
    const char* prev_key = 0;

    // initial hashtable pointer
    HashTable* hashtable = yml;

    // place to store data we get during the loop
    YmlData* loop_data = 0;

    // store any exceptions that occur during the loop
    exception tek_exception = SUCCESS;
    while (key) {
        // say we had something like
        // key: "value"
        // and i tried to do yml[key][something]
        // obviously that can't work, so don't allow access if not a hashtable
        if (!hashtable) {
            tek_exception = YML_EXCEPTION;
            tekExcept(tek_exception, "Invalid key - inaccessible type.");
            break;
        }

        // get the data stored in the hashtable, and make sure that it is a valid key
        tek_exception = hashtableGet(hashtable, key->data, &loop_data);
        if (tek_exception) {
            tekExcept(tek_exception, "Invalid key - key does not exist.");
            break;
        }

        // if the data stored is type 'yml data', then a hashtable is stored there
        // set this as the new hashtable
        if (loop_data->type == YML_DATA) {
            hashtable = loop_data->value;
        }

        prev_key = key->data;
        key = key->next;
    }

    // write data and throw any exceptions if they occurred
    tekChainThrow(tek_exception);

    // delete the old data that was stored here, in the appropriate way
    if (loop_data) {
        if (loop_data->type == YML_DATA) {
            tek_exception = ymlDelete(loop_data->value);
            // free(loop_data);
            tekChainThrow(tek_exception);
        } else {
            ymlDeleteData(loop_data);
        }
    }
    tekChainThrow(hashtableRemove(hashtable, prev_key));
    return SUCCESS;
}

exception ymlRemoveVA(YmlFile* yml, ...) {
    // separate method. should be called from a macro ymlRemove
    // the macro will force the final argument to be null

    // start variadic arguments
    va_list keys;
    va_start(keys, yml);
    const char* key;

    List keys_list = {};
    listCreate(&keys_list);

    while ((key = va_arg(keys, const char*))) {
        listAddItem(&keys_list, key);
    }

    // finish variadic arguments
    va_end(keys);

    // get item and throw any exceptions if they occurred
    exception tek_exception = ymlRemoveList(yml, &keys_list);
    listDelete(&keys_list);
    
    return tek_exception;
}

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
    flag inside_marks = 0;
    flag allow_next = 0;
    uint indent = 0;
    uint line = 1;
    for (int i = 0; i < buffer_size; i++) {
        if (buffer[i] == '\\') {
            allow_next = 1;
            continue;
        }
        if (allow_next) {
            allow_next = 0;
            continue;
        }
        if (buffer[i] == '"') {
            inside_marks = (flag)!inside_marks;
        }
        if (isWhitespace(buffer[i]) || buffer[i] == ':') {
            if ((i > start_index + 1) && !inside_marks) {
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
            if (!inside_marks) {
                start_index = i;
            }
        }
    }
    return SUCCESS;
}

exception ymlThrowSyntax(const Word* word) {
    char error_message[E_MESSAGE_SIZE];
    sprintf(error_message, "YML syntax error at line %u, in '", word->line);
    const uint len_message = strlen(error_message);
    const uint len_copy = E_MESSAGE_SIZE - len_message - 2 > word->length ? word->length : E_MESSAGE_SIZE - len_message - 2;
    memcpy(error_message + len_message, word->start, len_copy);
    error_message[len_message + len_copy] = '\'';
    error_message[len_message + len_copy + 1] = 0;
    tekThrow(YML_EXCEPTION, error_message);
}

flag ymlDetectType(Word* word) {
    if ((word->start[0] == '"') && (word->start[word->length - 1] == '"')) {
        if (word->length <= 1) return STRING_TOKEN;
        word->start += 1;
        word->length -= 2;
        return STRING_TOKEN;
    }
    flag contains_digits = 0;
    uint num_decimals = 0;
    flag contains_letters = 0;
    for (uint i = 0; i < word->length; i++) {
        const char c = word->start[i];
        if (c == '.') num_decimals += 1;
        else if ((c >= '0') && (c <= '9')) contains_digits = 1;
        else contains_letters = 1;
    }
    if (contains_letters) return STRING_TOKEN;
    if (num_decimals > 1) return STRING_TOKEN;
    if ((num_decimals == 1) && contains_digits) return FLOAT_TOKEN;
    if ((num_decimals == 0) && contains_digits) return INTEGER_TOKEN;
    return STRING_TOKEN;
}

exception ymlCreateKeyToken(Word* word, Token** token) {
    *token = (Token*)malloc(sizeof(Token));
    if (!*token) tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for key token.");
    (*token)->word = word;
    (*token)->type = ID_TOKEN;// | ymlDetectType(word);
    return SUCCESS;
}

exception ymlCreateValueToken(Word* word, Token** token) {
    *token = (Token*)malloc(sizeof(Token));
    if (!*token) tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for value token.");
    (*token)->word = word;
    (*token)->type = ymlDetectType(word);
    return SUCCESS;
}

exception ymlCreateListToken(Word* word, Token** token) {
    *token = (Token*)malloc(sizeof(Token));
    if (!*token) tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for list token.");
    (*token)->word = word;
    (*token)->type = LIST_TOKEN | ymlDetectType(word);
    return SUCCESS;
}

exception ymlCreateIndentToken(const flag type, Token** token) {
    *token = (Token*)malloc(sizeof(Token));
    if (!*token) tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for indent token.");
    (*token)->type = type;
    (*token)->word = 0;
    return SUCCESS;
}

exception ymlUpdateIndent(uint* indent, const Word* word, List* tokens, Stack* indent_stack) {
    if (word->indent != *indent) {
        Token* indent_token;
        if (word->indent > *indent) {
            ymlCreateIndentToken(OUT_TOKEN, &indent_token);
            stackPush(indent_stack, (void*)*indent);
            tekChainThrow(listAddItem(tokens, indent_token));
        } else if (word->indent < *indent) {
            uint prev_indent = 0;
            while (!stackPop(indent_stack, &prev_indent)) {
                ymlCreateIndentToken(IN_TOKEN, &indent_token);
                tekChainThrow(listAddItem(tokens, indent_token));
                if (prev_indent == word->indent) {
                    break;
                }
                if (prev_indent < word->indent) {
                    tekChainThrow(ymlThrowSyntax(word));
                }
            }
        }
        *indent = word->indent;
    }
    return SUCCESS;
}

exception ymlCreateTokensWithStack(const List* split_text, List* tokens, Stack* indent_stack) {
    const ListItem* item = split_text->data;
    uint indent = 0;
    tekChainThrow(stackPush(indent_stack, (void*)indent));
    while (item) {
        Word* word = (Word*)item->data;

        if (word->start[0] == ':') tekChainThrow(ymlThrowSyntax(word));

        Token* token = 0;
        if (word->start[0] == '-') {
            if (!item->next) tekChainThrow(ymlThrowSyntax(word));
            item = item->next;
            Word* next_word = (Word*)item->data;
            tekChainThrow(ymlCreateListToken(next_word, &token));
        } else {
            if (!item->next) {
                tekChainThrow(ymlCreateValueToken(word, &token));
            } else {
                const Word* next_word = (Word*)item->next->data;
                if (next_word->start[0] == ':') {
                    tekChainThrow(ymlCreateKeyToken(word, &token));
                    tekChainThrow(ymlUpdateIndent(&indent, word, tokens, indent_stack));
                    item = item->next;
                } else {
                    tekChainThrow(ymlCreateValueToken(word, &token));
                }
            }
        }
        tekChainThrow(listAddItem(tokens, token));
        item = item->next;
    }
    return SUCCESS;
}

exception ymlCreateTokens(const List* split_text, List* tokens) {
    Stack indent_stack;
    stackCreate(&indent_stack);
    const exception yml_exception = ymlCreateTokensWithStack(split_text, tokens, &indent_stack);
    stackDelete(&indent_stack);
    tekChainThrow(yml_exception);
    return SUCCESS;
}

exception ymlFromTokens(const List* tokens, YmlFile* yml) {
    List keys_list = {};
    listCreate(&keys_list);
    listAddItem(&keys_list, 0);
    char* key = 0;
    exception tek_exception = SUCCESS;

    const ListItem* item = tokens->data;
    while (item) {
        const Token* token = item->data;
        if (token->type == ID_TOKEN) {
            listPopItem(&keys_list, &key);
            if (key) free(key);
            ymlWordToString(token->word, &key);
            tek_exception = listAddItem(&keys_list, key);
            if (tek_exception) break;
            item = item->next;
            continue;
        }
        if (token->type == OUT_TOKEN) {
            listAddItem(&keys_list, 0);
            item = item->next;
            continue;
        }
        if (token->type == IN_TOKEN) {
            listPopItem(&keys_list, &key);
            if (key) free(key);
            item = item->next;
            continue;
        }

        YmlData* yml_data;
        if ((token->type & LIST_TOKEN) == LIST_TOKEN) {
            List* yml_list = (List*)malloc(sizeof(List));
            if (!yml_list) {
                tek_exception = MEMORY_EXCEPTION;
                tekExcept(tek_exception, "Failed to allocate memory for yml list.");
                break;
            }
            listCreate(yml_list);
            const ListItem* prev_item = item;
            while (item) {
                const Token* list_token = (Token*)item->data;
                if ((list_token->type & LIST_TOKEN) != LIST_TOKEN) break;
                YmlData* list_data;
                tek_exception = ymlCreateAutoDataToken(list_token, &list_data);
                if (tek_exception) break;
                tek_exception = listAddItem(yml_list, list_data);
                if (tek_exception) break;
                prev_item = item;
                item = item->next;
            }
            if (tek_exception) break;
            tek_exception = ymlCreateListData(yml_list, &yml_data);
            if (tek_exception) break;
            item = prev_item;
        } else {
            tek_exception = ymlCreateAutoDataToken(token, &yml_data);
            if (tek_exception) break;
        }
        
        if (!yml_data) {
            tek_exception = FAILURE;
            tekExcept(tek_exception, "Something fuckd..");
            break;
        }

        tek_exception = ymlSetList(yml, yml_data, &keys_list);
        if (tek_exception) break;
        item = item->next;
    }

    listDelete(&keys_list);
    tekChainThrow(tek_exception);
    return SUCCESS;
}

void ymlPrintData(YmlData* data) {
     if (data->type == STRING_DATA) printf("%s\n", (char*)data->value);
     if (data->type == INTEGER_DATA) printf("%ld\n", (long)data->value);
     if (data->type == FLOAT_DATA) {
         double number = 0;
         memcpy(&number, &data->value, sizeof(void*));
         printf("%f\n", number);
     }

}

exception ymlPrintIndent(YmlFile* yml, uint indent) {
    if (!yml) tekThrow(NULL_PTR_EXCEPTION, "Cannot print null pointer.");

    // create pointer to store list of values
    YmlData** values;

    // get number of values
    const uint num_items = yml->num_items;

    // get values from hashtable
    tekChainThrow(hashtableGetValues(yml, &values));

    const char** keys;
    tekChainThrow(hashtableGetKeys(yml, &keys));

    exception tek_exception = SUCCESS;

    // iterate over the values we got
    for (uint i = 0; i < num_items; i++) {
        for (uint i = 0; i < indent; i++) printf("  ");
        printf("%s: ", keys[i]);
        // if this is a pointer to another hashtable, we need to recursively print it
        if (values[i]->type == YML_DATA) {
            HashTable* hashtable = values[i]->value;
            printf("\n");
            tek_exception = ymlPrintIndent(hashtable, indent + 1);
            if (tek_exception) break;
        // otherwise, just print as normal
        } else {
            if (values[i]->type == LIST_DATA) {
                List* yml_list = values[i]->value;
                ListItem* item = yml_list->data;
                printf("\n");
                while (item) {
                    const YmlData* list_data = item->data;
                    for (uint i = 0; i <= indent; i++) printf("  ");
                    printf("- ");
                    ymlPrintData(list_data);
                    item = item->next;
                }
            } else {
                ymlPrintData(values[i]);
            }
        }
    }

    // clean up the array we used
    free(keys);
    free(values);
    tekChainThrow(tek_exception);
    return SUCCESS;
}

exception ymlPrint(YmlFile* yml) {
    tekChainThrow(ymlPrintIndent(yml, 0));
}

exception ymlReadFile(const char* filename, YmlFile* yml) {
    tekChainThrow(ymlCreate(yml));

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
    listCreate(&tokens);
    tekChainThrow(ymlSplitText(buffer, file_size, &split_text));
    tekChainThrow(ymlCreateTokens(&split_text, &tokens));

    tekChainThrow(ymlFromTokens(&tokens, yml));

//    const ListItem* item = tokens.data;
//    while (item) {
//        const Token* token = (Token*)item->data;
//        const Word* word = token->word;
//        switch (token->type) {
//            case OUT_TOKEN:
//                printf("out token");
//                break;
//            case IN_TOKEN:
//                printf("in token");
//                break;
//            case ID_TOKEN:
//                printf("id token  : ");
//                break;
//            case STRING_TOKEN:
//                printf("str token : ");
//                break;
//            case INTEGER_TOKEN:
//                printf("int token : ");
//                break;
//            case FLOAT_TOKEN:
//                printf("num token : ");
//                break;
//            case LIST_TOKEN:
//                printf("list token: ");
//                break;
//            default:
//                printf("UD token!");
//        }
//        if (word) {
//            for (uint i = 0; i < word->length; i++) {
//                printf("%c", word->start[i]);
//            }
//        }
//        printf("\n");
//        item = item->next;
//    }

    listFreeAllData(&split_text);
    listFreeAllData(&tokens);
    listDelete(&split_text);
    listDelete(&tokens);

    return SUCCESS;
}
