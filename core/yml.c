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

/**
 * @brief Represents a word as part of a larger yml file.
 */
typedef struct Word {
    /** A pointer to the first character in the word. */
    const char* start;

    /** The number of characters in the word. */
    uint length;

    /** The indentation of the word in the file - the number of parents it has. */
    uint indent;

    /** The line number at which this word occurs in the file. */
    uint line;
} Word;

/**
 * @brief Represents a token which is an abstract symbol that can make up a yml structure.
 */
typedef struct Token {
    /** A pointer to a word that contains the data of the token. */
    Word* word;

    /** The type of token */
    flag type;
} Token;

/**
 * @brief Allocate memory to copy the content of a yml word into.
 * @note This function allocates memory which needs to be freed.
 * @param[in] word A pointer to the word that should be copied.
 * @param[out] string A pointer to a char pointer where the address of the new char array should be stored at.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 */
exception ymlWordToString(const Word* word, char** string) {
    *string = (char*)malloc(word->length + 1);
    if (!(*string)) tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for word string.");

    // copy in string data from token
    memcpy(*string, word->start, word->length);

    // add null terminator character
    (*string)[word->length] = 0;
    return SUCCESS;
}

/**
 * @brief Allocate a new YmlData struct given input data.
 * @note This function allocates memory which needs to be freed.
 * @param[in] param_name The input parameter that will be copied.
 * @param[out] yml_data A pointer to a YmlData pointer which will point to the new struct.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 */
#define YML_CREATE_FUNC(func_name, func_type, param_name, yml_type, write_method) \
exception func_name(func_type param_name, YmlData** yml_data) { \
    *yml_data = (YmlData*)malloc(sizeof(YmlData)); \
    if (!(*yml_data)) tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for yml data."); \
    (*yml_data)->type = yml_type; \
    \
    /* write_method is the code which is responsible for setting the YmlData's value */ \
    /* It is unique for each data type. */ \
    write_method; \
    return SUCCESS; \
} \

/**
 * @brief Allocate a new YmlData struct using a token.
 * @note  This function allocates memory which needs to be freed.
 * @param[in] token A pointer to the token to be used.
 * @param[out] yml_data A pointer to a YmlData pointer which will point to the new struct.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 * @throws YML_EXCEPTION if converting from a number token which is outside size range.
 */
#define YML_CREATE_TOKEN_FUNC(func_name, yml_type, write_method) \
exception func_name(const Token* token, YmlData** yml_data) { \
    *yml_data = (YmlData*)malloc(sizeof(YmlData)); \
    if (!(*yml_data)) tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for yml data."); \
    (*yml_data)->type = yml_type; \
    \
    /* This function allocates a string which can be used in the write_method code to convert to many types */\
    char* string; \
    tekChainThrowThen(ymlWordToString(token->word, &string), { \
        free(*yml_data); \
    }); \
    write_method; \
    return SUCCESS; \
} \

/// @copybrief YML_CREATE_FUNC
/// @note Create a YmlData struct using a string.
YML_CREATE_FUNC(ymlCreateStringData, const char*, string, STRING_DATA, {
    const uint len_string = strlen(string) + 1;
    char* string_copy = (char*)malloc(len_string);
    if (!string_copy) tekThrowThen(MEMORY_EXCEPTION, "Failed to allocate memory for string data.", {
        free(*yml_data);
    });
    memcpy(string_copy, string, len_string);
    (*yml_data)->value = string_copy;
});

/// @copybrief YML_CREATE_TOKEN_FUNC
/// @note Create a YmlData struct using a string token.
YML_CREATE_TOKEN_FUNC(ymlCreateStringDataToken, STRING_DATA, {
    (*yml_data)->value = string;
});

/// @copybrief YML_CREATE_FUNC
/// @note Create a YmlData struct using an integer.
YML_CREATE_FUNC(ymlCreateIntegerData, const long, integer, INTEGER_DATA, {
    (*yml_data)->value = (void*)integer;
});

/// @copybrief YML_CREATE_TOKEN_FUNC
/// @note Create a YmlData struct using an integer token.
YML_CREATE_TOKEN_FUNC(ymlCreateIntegerDataToken, INTEGER_DATA, {
    // convert string into a base-10 number
    const long integer = strtol(string, NULL, 10);
    const int error = errno;

    // string is not directly used, just an intermediary to get the integer value from the token.
    // therefore it needs to be freed.
    free(string);
    if (error == ERANGE) tekThrowThen(YML_EXCEPTION, "Integer is either too small or large.", {
        free(*yml_data);
    });
    (*yml_data)->value = (void*)integer;
});

/// @copybrief YML_CREATE_FUNC
/// @note Create a YmlData struct using a float.
YML_CREATE_FUNC(ymlCreateFloatData, const double, number, FLOAT_DATA, {
    // memcpy used because you can't cast a double to a void*
    memcpy(&(*yml_data)->value, &number, sizeof(void*));
});

/// @copybrief YML_CREATE_TOKEN_FUNC
/// @note Create a YmlData struct using a float token.
YML_CREATE_TOKEN_FUNC(ymlCreateFloatDataToken, FLOAT_DATA, {
    const double number = strtod(string, NULL);
    const int error = errno;

    // free string, as it is no longer needed once the number has been extracted
    free(string);
    if (error == ERANGE) tekThrowThen(YML_EXCEPTION, "Number is either too small or too large", {
        free(*yml_data);
    });
    memcpy(&(*yml_data)->value, &number, sizeof(void*));
});

/**
 * @brief A function to automatically allocate a YmlData struct based on the type of the token.
 *
 * This function allocates memory which needs to be freed.
 *
 * @param[in] token The token to be used.
 * @param[out] yml_data A pointer to a YmlData pointer which will point to the new struct.
 * @throws MEMORY_EXCEPTION if the subsequent call to allocate YmlData fails.
 * @throws YML_EXCEPTION if token data is a number that is outside the relevant size boundary.
 * @throws YML_EXCEPTION if token is of an unknown type.
 */
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

/**
 * @brief Copy the string data of a YmlData struct into a newly allocated buffer.
 *
 * @note This function allocates memory which needs to be freed.
 *
 * @param[in] yml_data A pointer to a YmlData struct to be used.
 * @param[out] string A pointer to a string to write the YmlData into.
 * @throws YML_EXCEPTION if the YmlData is not a string.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 */
exception ymlDataToString(const YmlData* yml_data, char** string) {
    // make sure that the data is actually of a string
    if (yml_data->type != STRING_DATA) tekThrow(YML_EXCEPTION, "Data is not of string type.");

    const char* yml_string = yml_data->value;
    const uint len_string = strlen(yml_string) + 1;
    *string = (char*)malloc(len_string * sizeof(char));
    if (!(*string)) tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for string.");

    memcpy(*string, yml_string, len_string);
    return SUCCESS;
}

/**
 * @brief Copy the integer data of a YmlData struct into an existing integer.
 *
 * @param[in] yml_data A pointer to a YmlData struct to be used.
 * @param[out] integer A pointer to a long integer to write the YmlData into.
 * @throws YML_EXCEPTION if the YmlData is not an integer.
 */
exception ymlDataToInteger(const YmlData* yml_data, long* integer) {
    // make sure that the data is actually of an integer
    if (yml_data->type != INTEGER_DATA) tekThrow(YML_EXCEPTION, "Data is not of integer type.");
    *integer = (long)yml_data->value;
    return SUCCESS;
}

/**
 * @brief Copy the double data of a YmlData struct into an existing double.
 *
 * @param[in] yml_data A pointer to a YmlData struct to be used.
 * @param[out] number A pointer to a double to write the YmlData into.
 * @throws YML_EXCEPTION if the YmlData is not a double.
 */
exception ymlDataToFloat(const YmlData* yml_data, double* number) {
    // make sure that the data is actually of a double
    if (yml_data->type != FLOAT_DATA) tekThrow(YML_EXCEPTION, "Data is not of double precision floating point type.");
    memcpy(number, &yml_data->value, sizeof(void*));
    return SUCCESS;
}

/**
 * @brief Copy the data of a YmlData list at a certain index.
 * @note This function may allocate memory which needs to be freed. (if using strings)
 * @param[in] yml_list A pointer to a YmlData struct to be used.
 * @param[in] index The index of the list where the data is stored.
 * @param[out] param_name A pointer to a variable to write the YmlData into.
 * @throws YML_EXCEPTION if the YmlData is not the correct type at that index.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 */
#define YML_GET_LIST_FUNC(func_name, func_type, param_name, convert_func) \
exception func_name(const YmlData* yml_list, const uint index, func_type param_name) { \
    if (yml_list->type != LIST_DATA) tekThrow(YML_EXCEPTION, "Data is not of list type."); \
    YmlData* yml_data; \
    tekChainThrow(listGetItem(yml_list->value, index, &yml_data)); \
    tekChainThrow(convert_func(yml_data, param_name)); \
    return SUCCESS; \
} \

/// @copydoc YML_GET_LIST_FUNC
/// @note Get string from YmlData list. Will allocate new memory.
YML_GET_LIST_FUNC(ymlListGetString, char**, string, ymlDataToString);

/// @copydoc YML_GET_LIST_FUNC
/// @note Get integer from YmlData list.
YML_GET_LIST_FUNC(ymlListGetInteger, long*, integer, ymlDataToInteger);

/// @copydoc YML_GET_LIST_FUNC
/// @note Get float from YmlData list.
YML_GET_LIST_FUNC(ymlListGetFloat, double*, number, ymlDataToFloat);

/**
 * @brief A helper function to fill an existing array by converting a List of YmlData strings.
 *
 * This function requires an array with the same size as the list to already be allocated.
 *
 * @param[in] yml_list A list containing YmlData structs to be converted.
 * @param[out] array An already allocated array of char pointers.
 * @throws YML_EXCEPTION if any of the items in the list are not strings.
 * @throws MEMORY_EXCEPTION if memory could not be allocated to store any of the strings.
 */
#define YML_ARRAY_LOOP_FUNC(func_name, func_type, convert_func) \
exception func_name(const List* yml_list, func_type* array) { \
    const ListItem* item = yml_list->data; \
    uint index = 0; \
    while (item) { \
        const YmlData* yml_data = (YmlData*)item->data; \
        func_type value = 0; \
        tekChainThrow(convert_func(yml_data, &value)); \
        array[index++] = value; \
        item = item->next; \
    } \
    return SUCCESS; \
} \

/// @copydoc YML_ARRAY_LOOP_FUNC
/// @note Converts a list of YmlData strings into an already allocated array.
YML_ARRAY_LOOP_FUNC(ymlListToStringArrayLoop, char*, ymlDataToString);

/// @copydoc YML_ARRAY_LOOP_FUNC
/// @note Converts a list of YmlData integers into an already allocated array.
YML_ARRAY_LOOP_FUNC(ymlListToIntegerArrayLoop, long, ymlDataToInteger);

/// @copydoc YML_ARRAY_LOOP_FUNC
/// @note Converts a list of YmlData integers into an already allocated array.
YML_ARRAY_LOOP_FUNC(ymlListToFloatArrayLoop, double, ymlDataToFloat);

/**
 * @brief Copy the data of a YmlData list into a newly allocated array.
 * @note This function allocated memory which needs to be freed.
 * @param[in] yml_list A pointer to a YmlData struct to be used.
 * @param[out] array A pointer to store the pointer to the start of the array.
 * @param[out] len_array A pointer to an integer to store the length of the array.
 * @throws YML_EXCEPTION if the YmlData is not a list.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 */
#define YML_ARRAY_FUNC(func_name, func_type, loop_func) \
exception func_name(const YmlData* yml_list, func_type** array, uint* len_array) { \
    /* Set array length to 0 initially, so if function fails, potential for loops can't iterate over the non-existent data */ \
    *len_array = 0; \
    \
    /* Non-list data cannot be iterated over by loop_func */ \
    if (yml_list->type != LIST_DATA) tekThrow(YML_EXCEPTION, "Data is not of list type."); \
    \
    /* Cast the yml_data to a List* so that it can be used by list functions */\
    const List* internal_list = yml_list->value; \
    \
    /* Allocate an array that the loop_func can fill */ \
    *array = (long*)malloc(sizeof(long) * internal_list->length); \
    if (!(*array)) tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for array."); \
    \
    tekChainThrowThen(loop_func(internal_list, *array), { \
        free(*array); \
    }); \
    \
    /* Update length of array to real size if all is well. */ \
    *len_array = internal_list->length; \
    return SUCCESS; \
} \

/// @copydoc YML_ARRAY_FUNC
/// @note Copy a YmlData list of strings into an array. Allocates memory that needs to be freed.
YML_ARRAY_FUNC(ymlListToStringArray, char*, ymlListToStringArrayLoop);

/// @copydoc YML_ARRAY_FUNC
/// @note Copy a YmlData list of integers into an array. Allocates memory that needs to be freed.
YML_ARRAY_FUNC(ymlListToIntegerArray, long, ymlListToIntegerArrayLoop);

/// @copydoc YML_ARRAY_FUNC
/// @note Copy a YmlData list of floats into an array. Allocates memory that needs to be freed.
YML_ARRAY_FUNC(ymlListToFloatArray, double, ymlListToFloatArrayLoop);

/**
 * @brief Allocate a YmlData struct given a list.
 * @note This function allocates memory which needs to be freed. The list is only stored as a reference, and not copied. The list should therefore not be deleted once stored in the YmlData.
 * @param list A pointer to the list for which the YmlData should reference.
 * @param yml_data A pointer to a pointer which should point to the allocated YmlData.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 */
exception ymlCreateListData(List* list, YmlData** yml_data) {
    // allocate some memory for yml data
    *yml_data = (YmlData*)malloc(sizeof(YmlData));
    if (!(*yml_data)) tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for list data.");
    (*yml_data)->type = LIST_DATA;
    (*yml_data)->value = (void*)list;
    return SUCCESS;
}

/**
 * @brief A function to free a YmlData struct based on its type.
 * @note This function attempts to free allocated memory: only YmlData structs created by the other functions should be freed with this function.
 * @param yml_data A pointer to the YmlData to free.
 */
void ymlDeleteData(YmlData* yml_data) {
    if (yml_data->value) {
        switch (yml_data->type) {
            case STRING_DATA:
                // for string data, we need to deallocate the copy of the original string
                free(yml_data->value);
                break;
            case LIST_DATA:
                // for list data, we need to deallocate each item in the list first
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

/**
 * @brief Free a previously allocated YML file.
 * @note This function should only be called after ymlCreate() or similar, as it will attempt to free memory.
 * @param yml A pointer to the YML file to free.
 * @throws NULL_PTR_EXCEPTION if yml is NULL
 * @throws MEMORY_EXCEPTION if there is an error with copying the yml keys in order to iterate over them and free at each key.
 */
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
            tekChainBreak(tek_exception);
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

/**
 * @brief Instantiate a YmlFile struct.
 * @param yml A pointer to an existing but empty YmlFile struct.
 * @throws MEMORY_EXCEPTION if malloc() fails
 */
exception ymlCreate(YmlFile* yml) {
    // simply need to create the hashtable which the yml file needs to work
    // this is just the root hash table
    tekChainThrow(hashtableCreate(yml, YML_HASHTABLE_SIZE));
    return SUCCESS;
}

// Auto convert variadic arguments into a list
// Using macro to avoid passing around a va_list.
#define VA_ARGS_TO_LIST(list_name, va_name, va_type, final_param) \
va_list va_name; \
va_start(va_name, final_param); \
va_type __va_item; \
List list_name = {}; \
listCreate(&list_name); \
while ((__va_item = va_arg(va_name, va_type))) { \
    listAddItem(&list_name, __va_item); \
} \
va_end(va_name);

/**
 * @brief Takes a list of keys in order and returns the data stored under that key.
 * If we had a YAML file that looked something like this
 * @code
 * cpu:
 *   clock_speed: 3.8GHz
 * @endcode
 * In order to access the CPU's clock speed, we would pass in a list of:
 * @code
 * keys = {"cpu", "clock_speed"}
 * @endcode
 * @note This function does NOT copy the data, once yml is freed, the YmlData is also gone.
 * @param[in] yml A pointer to the yml file to be read from.
 * @param[out] data A pointer to an existing but empty YmlData struct to copy into.
 * @param[in] keys A list of keys used to access the correct data.
 * @throws YML_EXCEPTION If the list points to an invalid key.
 */
exception ymlGetList(YmlFile* yml, YmlData** data, const List* keys) {
    *data = 0;
    HashTable* hashtable = yml;
    YmlData* loop_data;

    const ListItem* key = keys->data;
    while (key) {
        // say we had something like
        // key: "value"
        // and i tried to do yml[key][something]
        // obviously that can't work, so don't allow access if not a hashtable
        if (!hashtable)
            tekThrow(YML_EXCEPTION, "Invalid key - inaccessible type.");

        // get the data stored in the hashtable, and make sure that it is a valid key
        const exception tek_exception = hashtableGet(hashtable, key->data, &loop_data);
        if (tek_exception)
            tekThrow(YML_EXCEPTION, "Invalid key - key does not exist.");

        // if the data stored is type 'yml data', then a hashtable is stored there
        // set this as the new hashtable
        if (loop_data->type == YML_DATA)
            hashtable = loop_data->value;

        key = key->next;
    }

    *data = loop_data;
    return SUCCESS;
}

/// @copydoc ymlGetList
exception ymlGetVA(YmlFile* yml, YmlData** data, ...) {
    // separate method. should be called from a macro ymlGet
    // the macro will force the final argument to be null

    // start variadic arguments
    VA_ARGS_TO_LIST(keys_list, keys, const char*, data);

    // get item and throw any exceptions if they occurred
    const exception tek_exception = ymlGetList(yml, data, &keys_list);
    listDelete(&keys_list);

    tekChainThrow(tek_exception);
    return SUCCESS;
}

/**
 * @brief Takes a list of keys in order and returns the names of all keys under that name.
 * If we had a YAML file that looked something like this
 * @code
 * computer:
 *   cpu:
 *      manufacturer: intel
 *      clock_speed: 3.8GHz
 *      overclocked: false
 * @endcode
 * In order to access keys under cpu, we would use
 * @code
 * keys = {"computer", "cpu"}
 * @endcode
 * This would return an array of
 * @code
 * {"manufacturer", "clock_speed", "overclocked"}
 * @endcode
 * @note This function will allocate an array, overwriting the pointer specified. This array needs to be freed.
 * @param[in] yml A pointer to the yml file to be read from.
 * @param[out] yml_keys A pointer to an existing but empty char* array to copy into.
 * @param[out] num_keys The number of keys that were written into the array.
 * @param[in] keys A list of keys in order to access the correct data.
 * @throws YML_EXCEPTION If the list points to an invalid key.
 */
exception ymlGetKeysList(YmlFile* yml, char*** yml_keys, uint* num_keys, const List* keys) {
    YmlData* yml_data;
    tekChainThrow(ymlGetList(yml, &yml_data, keys));
    if (yml_data->type != YML_DATA) tekThrow(YML_EXCEPTION, "Data has no keys.");
    const HashTable* hashtable = yml_data->value;
    tekChainThrow(hashtableGetKeys(hashtable, yml_keys));
    *num_keys = hashtable->num_items;
    return SUCCESS;
}

/// @copydoc ymlGetKeysList
exception ymlGetKeysVA(YmlFile* yml, char*** yml_keys, uint* num_keys, ...) {
    // separate method. should be called from a macro ymlGet
    // the macro will force the final argument to be null

    // start variadic arguments
    VA_ARGS_TO_LIST(keys_list, keys, const char*, num_keys);

    // get item and throw any exceptions if they occurred
    const exception tek_exception = ymlGetKeysList(yml, yml_keys, num_keys, &keys_list);
    listDelete(&keys_list);

    tekChainThrow(tek_exception);
    return SUCCESS;
}

/**
 * @brief Takes a list of keys in order and sets the data stored under that key.
 * If we had a YAML file that looked something like this
 * @code
 * cpu:
 *   clock_speed: 3.8GHz
 * @endcode
 * In order to set the CPU's clock speed, we would pass in a list of:
 * @code
 * keys = {"cpu", "clock_speed"}
 * @endcode
 * @param[in] yml A pointer to the yml file to be read from.
 * @param[out] data A pointer to an existing YmlData struct to set at the specified key.
 * @param[in] keys A list of keys used to access the correct data.
 * @throws YML_EXCEPTION If the list points to an invalid key.
 */
exception ymlSetList(YmlFile* yml, YmlData* data, const List* keys) {
    // set up some variables
    const ListItem* key = keys->data;

    HashTable* hashtable = yml;
    YmlData* loop_data;

    // if the next key is 0, we don't want to process this
    // hence the use of the separate key and next_key variables
    while (key->next) {
        // get the data stored at current key
        const exception tek_exception = hashtableGet(hashtable, key->data, &loop_data);

        // if there is another hash table at that key
        if (!tek_exception && (loop_data->type == YML_DATA)) {
            // update searched hash table and increment key
            hashtable = loop_data->value;
            key = key->next;

            // go to next iteration
            continue;
        }

        // if there isn't a hashtable there, assume that one should be created
        HashTable* next_hashtable = (HashTable*)malloc(sizeof(HashTable));
        if (!next_hashtable) tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for new hash table.");

        // init the hashtable if no memory errors happened
        tekChainThrowThen(hashtableCreate(next_hashtable, YML_HASHTABLE_SIZE), {
            free(next_hashtable);
        });

        // create a yml data struct to store the hashtable pointer
        YmlData* yml_data = (YmlData*)malloc(sizeof(YmlData));
        if (!yml_data) tekThrowThen(MEMORY_EXCEPTION, "Failed to allocate memory for new yml data.", {
            hashtableDelete(next_hashtable);
            free(next_hashtable);
        });

        // fill with necessary data if all went well
        yml_data->type = YML_DATA;
        yml_data->value = next_hashtable;

        // delete the old data that was stored here, in the appropriate way
        if (loop_data && !tek_exception) {
            if (loop_data->type == YML_DATA) {
                tekChainThrowThen(ymlDelete(loop_data->value), {
                    hashtableDelete(next_hashtable);
                    free(next_hashtable);
                    free(yml_data);
                });
            } else {
                ymlDeleteData(loop_data);
            }
        }

        // set the hashtable at this index
        tekChainThrowThen(hashtableSet(hashtable, key->data, yml_data), {
            hashtableDelete(next_hashtable);
            free(next_hashtable);
            free(yml_data);
        });

        // update hashtable pointer and increment to the next key
        hashtable = next_hashtable;
        key = key->next;
    }

    // check for errors, and then set the data at the correct key
    tekChainThrow(hashtableSet(hashtable, key->data, data));
    return SUCCESS;
}

/// @copydoc ymlSetList
exception ymlSetVA(YmlFile* yml, YmlData* data, ...) {
    // separate method. should be called from a macro ymlGet
    // the macro will force the final argument to be null

    VA_ARGS_TO_LIST(keys_list, keys, const char*, data)

    // get item and throw any exceptions if they occurred
    const exception tek_exception = ymlSetList(yml, data, &keys_list);
    listDelete(&keys_list);
    
    return tek_exception;
}

/**
 * @brief Takes a list of keys in order and deletes the data stored under that key.
 * If we had a YAML file that looked something like this
 * @code
 * cpu:
 *   clock_speed: 3.8GHz
 * @endcode
 * In order to delete the CPU's clock speed, we would pass in a list of:
 * @code
 * keys = {"cpu", "clock_speed"}
 * @endcode
 * @param[in] yml A pointer to the yml file to be read from.
 * @param[in] keys A list of keys used to access the correct data.
 * @throws YML_EXCEPTION If the list points to an invalid key.
 */
exception ymlRemoveList(YmlFile* yml, const List* keys) {
    const ListItem* key = keys->data;
    const char* prev_key = 0;

    // initial hashtable pointer
    HashTable* hashtable = yml;

    // place to store data we get during the loop
    YmlData* loop_data = 0;

    while (key) {
        // say we had something like
        // key: "value"
        // and i tried to do yml[key][something]
        // obviously that can't work, so don't allow access if not a hashtable
        if (!hashtable)
            tekThrow(YML_EXCEPTION, "Invalid key - inaccessible type.");

        // get the data stored in the hashtable, and make sure that it is a valid key
        const exception tek_exception = hashtableGet(hashtable, key->data, &loop_data);
        if (tek_exception)
            tekThrow(tek_exception, "Invalid key - key does not exist.");

        // if the data stored is type 'yml data', then a hashtable is stored there
        // set this as the new hashtable
        if (loop_data->type == YML_DATA)
            hashtable = loop_data->value;

        prev_key = key->data;
        key = key->next;
    }

    // delete the old data that was stored here, in the appropriate way
    if (loop_data) {
        if (loop_data->type == YML_DATA) {
            tekChainThrow(ymlDelete(loop_data->value));
        } else {
            ymlDeleteData(loop_data);
        }
    }
    tekChainThrow(hashtableRemove(hashtable, prev_key));
    return SUCCESS;
}

/// @copydoc ymlRemoveList
exception ymlRemoveVA(YmlFile* yml, ...) {
    // separate method. should be called from a macro ymlRemove
    // the macro will force the final argument to be null

    // start variadic arguments
    VA_ARGS_TO_LIST(keys_list, keys, const char*, yml);

    // get item and throw any exceptions if they occurred
    const exception tek_exception = ymlRemoveList(yml, &keys_list);
    listDelete(&keys_list);
    
    return tek_exception;
}

/**
 * @brief Determine if a character is whitespace.
 * @param[in] c The character to check for whitespace.
 * @return 1 if whitespace, 0 if not.
 */
int isWhitespace(const char c) {
    switch (c) {
        case 0x00: // null terminator
        case 0x20: // space
        case 0x09: // tab
        case 0x0D: // carriage return
        case 0x0A: // line feed
            return 1;
        default:
            return 0;
    }
}

/**
 * @brief Split a buffer of text into individual words
 * @param[in] buffer A buffer of text to split into words
 * @param[in] buffer_size The number of characters in the text buffer
 * @param[out] split_text A pointer to an existing list to append each split Word into.
 * @throws MEMORY_EXCEPTION If malloc() fails
 */
exception ymlSplitText(const char* buffer, const uint buffer_size, List* split_text) {
    int start_index = -1;  // marker of where a word starts
    flag trace_indent = 1; // whether to keep track of the indent
    flag inside_marks = 0; // whether this is "inside speech marks"
    flag allow_next = 0;   // whether this is an escaped character
    uint indent = 0;       // the number of spaces of indent
    uint line = 1;         // current line number
    for (int i = 0; i < buffer_size; i++) {
        // logic to allow for escape characters, if \ skip next, if skipped next dont skip again
        if (buffer[i] == '\\') {
            allow_next = 1;
            continue;
        }
        if (allow_next) {
            allow_next = 0;
            continue;
        }

        // if speech mark detected, then flip whether inside marks
        if (buffer[i] == '"') {
            inside_marks = (flag)!inside_marks;
        }

        if (isWhitespace(buffer[i]) || buffer[i] == ':') {
            // if whitespace is detected outside of speech marks, a word has ended
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

            // if a colon is detected, then an identifier just ended
            if (buffer[i] == ':' && !inside_marks) {
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
            // when whitespace is detected, restart the starting position of current word
            if (!inside_marks) {
                start_index = i;
            }
        }
    }
    return SUCCESS;
}

/**
 * @brief Throw a yml exception, and create a custom message based on the word and line number.
 * @param[in] word The word that is responsible for the syntax error
 * @throws YML_EXCEPTION Which is the whole point of the function
 */
exception ymlThrowSyntax(const Word* word) {
    // utility to create customised error messages

    char error_message[E_MESSAGE_SIZE];
    sprintf(error_message, "YML syntax error at line %u, in '", word->line);
    const uint len_message = strlen(error_message);

    // error messages have limited size, make sure that we truncate to this size
    const uint len_copy = E_MESSAGE_SIZE - len_message - 2 > word->length ? word->length : E_MESSAGE_SIZE - len_message - 2;
    memcpy(error_message + len_message, word->start, len_copy);

    // finish the message with '\0, to complete the opened quotation mark, and null terminate the string
    error_message[len_message + len_copy] = '\'';
    error_message[len_message + len_copy + 1] = 0;

    // no need to mallocate because tekThrow should copy for us :D
    tekThrow(YML_EXCEPTION, error_message);
}

/**
 * @brief Detect the word type
 * @param[in] word The word to detect its type
 * @return FLOAT_TOKEN if the word contains digits and a single decimal point, and potentially starting with '-'
 * @return INTEGER_TOKEN if the word only contains digits, and potentially starting with '-'
 * @return STRING_TOKEN otherwise
 */
flag ymlDetectType(Word* word) {
    // if the word starts and ends in '"', it's a string
    if ((word->start[0] == '"') && (word->start[word->length - 1] == '"')) {
        if (word->length <= 1) return STRING_TOKEN;
        word->start += 1;
        word->length -= 2;
        return STRING_TOKEN;
    }

    // count letters, numbers and decimal points
    flag contains_digits = 0;
    uint num_decimals = 0;
    flag contains_letters = 0;
    for (uint i = 0; i < word->length; i++) {
        const char c = word->start[i];
        if ((i == 0) && (c == '-')) continue; // allow numbers to start with a negative sign
        if (c == '.') num_decimals += 1;
        else if ((c >= '0') && (c <= '9')) contains_digits = 1;
        else {
            contains_letters = 1;
            break;
        }
        if (num_decimals >= 2) break;
    }

    // if the word contains anything other than numbers and a single decimal place, assume string
    if (contains_letters) return STRING_TOKEN;
    if (num_decimals > 1) return STRING_TOKEN;

    // based on number of decimals (0 or 1) decides whether it's an integer or float
    if ((num_decimals == 1) && contains_digits) return FLOAT_TOKEN;
    if ((num_decimals == 0) && contains_digits) return INTEGER_TOKEN;

    // a single dot with no digits is also a string
    return STRING_TOKEN;
}

/**
 * @brief Create a token given a word.
 * @note This function allocates memory that needs to be freed.
 * @param[in] word The word to create a token from.
 * @param[out] token A pointer to the token that will be created.
 */
#define TOKEN_CREATE_FUNC(func_name, token_type) \
exception func_name(Word* word, Token** token) { \
    *token = (Token*)malloc(sizeof(Token)); \
    if (!*token) tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for token."); \
    (*token)->word = word; \
    (*token)->type = token_type; \
    return SUCCESS; \
} \

/// @copydoc TOKEN_CREATE_FUNC
/// @note Returns a new token with a type of ID_TOKEN
TOKEN_CREATE_FUNC(ymlCreateKeyToken, ID_TOKEN);

/// @copydoc TOKEN_CREATE_FUNC
/// @note Returns a new token with an auto-detected type
TOKEN_CREATE_FUNC(ymlCreateValueToken, ymlDetectType(word));

/// @copydoc TOKEN_CREATE_FUNC
/// @note Returns a new token with an auto-detected type + LIST_TOKEN
TOKEN_CREATE_FUNC(ymlCreateListToken, LIST_TOKEN | ymlDetectType(word));

/**
 * @brief Create an indent token given a type.
 * @note This function allocates memory that needs to be freed.
 * @param[in] type The type of indent token to create.
 * @param[out] token A pointer to the token that will be created.
 */
exception ymlCreateIndentToken(const flag type, Token** token) {
    *token = (Token*)malloc(sizeof(Token));
    if (!*token) tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for indent token.");
    (*token)->type = type;
    (*token)->word = 0;
    return SUCCESS;
}

/**
 * @brief Update the current indent we are at while parsing a yml file.
 * This works not by tracking the number of spaces, but the number of times that the number of spaces increased since the start of the file.
 * For a file such as
 * @code
 * depth1:
 *   depth2:
 *      depth3:
 *          depth4: "something"
 * @endcode
 * The depth stack would look something like
 * @code
 * {0, 2, 5, 9}
 * @endcode
 * Representing the number of spaces at each indent. The size of the stack - 1 (which is 3 here) represents the current indent of the file.
 * If we updated the file to look like
 * @code
 * depth1:
 *   depth2:
 *      depth3:
 *          depth4: "something"
 *        depth5: "bad formatting"
 * @endcode
 * The indent stack could be popped repeatedly to find that the current number of spaces (7) never occurred before, and so this formatting is invalid.
 * @param indent The current indent.
 * @param word The word that should be used to check if the indent has changed.
 * @param tokens The list of tokens that should gain an indent token if the indent has changed.
 * @param indent_stack The indent stack - a stack storing each level of indent in the order that it occurred.
 * @return
 */
exception ymlUpdateIndent(uint* indent, const Word* word, List* tokens, Stack* indent_stack) {
    // essentially, if the indent changes, we need to look at the indent stack
    if (word->indent != *indent) {
        Token* indent_token;
        // if the indent is more, add this as a new indent to the stack
        if (word->indent > *indent) {
            ymlCreateIndentToken(OUT_TOKEN, &indent_token);
            stackPush(indent_stack, (void*)*indent);
            tekChainThrow(listAddItem(tokens, indent_token));
        // otherwise, keep popping until we either find the indent, or realise its a bad indent
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

/**
 * @brief A function responsible for parsing a Word List, and converting this into a series of tokens that can be processed into a yml data structure.
 * @param split_text A Word List containing a processed yml file.
 * @param tokens A list of tokens to fill with data.
 * @param indent_stack An already existing but empty stack that can be used by the function to store the indentation of the file.
 * @throws YML_EXCEPTION If a syntax error is detected.
 * @throws MEMORY_EXCEPTION If malloc() fails.
 */
exception ymlCreateTokensStack(const List* split_text, List* tokens, Stack* indent_stack) {
    const ListItem* item = split_text->data;

    // start the indent stack at 0 indent
    uint indent = 0;
    tekChainThrow(stackPush(indent_stack, (void*)indent));
    while (item) {
        Word* word = (Word*)item->data;

        // if an unpaired : appears, this is a syntax error
        if ((word->start[0] == ':') && (word->length == 1))
            tekChainThrow(ymlThrowSyntax(word));

        Token* token = 0;

        // if there is a -, this signals a list item
        if ((word->start[0] == '-') && (word->length == 1)) {
            // there should be a word after this that is the actual list data
            if (!item->next)
               tekChainThrow(ymlThrowSyntax(word));
            item = item->next;
            Word* next_word = (Word*)item->data;
            tekChainThrow(ymlCreateListToken(next_word, &token));
        } else {
            // last item should always be a value, an identifier cant identify nothing
            if (!item->next) {
                tekChainThrow(ymlCreateValueToken(word, &token));
            } else {
                const Word* next_word = (Word*)item->next->data;
                // if next word is ':', this is an identifier
                if ((next_word->start[0] == ':') && (next_word->length == 1)) {
                    tekChainThrow(ymlCreateKeyToken(word, &token));
                    tekChainThrow(ymlUpdateIndent(&indent, word, tokens, indent_stack));

                    // skip over the ':', not needed data
                    item = item->next;
                // otherwise, its a value
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

/**
 * @copybrief ymlCreateTokensStack
 * @param split_text A Word List containing a processed yml file.
 * @param tokens A list of tokens to fill with data.
 * @throws YML_EXCEPTION If there is a syntax error
 * @throws MEMORY_EXCEPTION If malloc() fails.
 */
exception ymlCreateTokens(const List* split_text, List* tokens) {
    Stack indent_stack;
    stackCreate(&indent_stack);

    // ymlCreateTokensStack operates with a while loop.
    // Creating the indent stack outside of the function allows us to return at any point in the loop, and not worry about freeing the stack.
    // The stack creation and freeing is all this function does.
    const exception tek_exception = ymlCreateTokensStack(split_text, tokens, &indent_stack);
    stackDelete(&indent_stack);
    return tek_exception;
}

/**
 * @brief Helper function to turn a list of tokens into a single YmlData struct.
 * @note This function allocates memory that needs to be freed.
 * @param prev_item A pointer to prev_item in main function.
 * @param item A pointer to item in main function.
 * @param yml_list A list of YmlData that will be used in the creation of the YmlData.
 * @param yml_data A pointer to the YmlData.
 * @throws YML_EXCEPTION if a syntax error is detected.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 */
exception ymlFromTokensListAddList(const ListItem** prev_item, const ListItem** item, List* yml_list, YmlData** yml_data) {
    while (*item) {
        // loop until we find a non list item in the tokens
        const Token* list_token = (Token*)(*item)->data;
        if ((list_token->type & LIST_TOKEN) != LIST_TOKEN) break;

        // attempt to fill list with yml data from tokens.
        YmlData* list_data;
        tekChainThrow(ymlCreateAutoDataToken(list_token, &list_data));
        tekChainThrowThen(listAddItem(yml_list, list_data), {
            ymlDeleteData(list_data);
        });

        *prev_item = *item;
        *item = (*item)->next;
    }

    tekChainThrowThen(ymlCreateListData(yml_list, yml_data), {
        ymlDeleteData(*yml_data);
    });

    return SUCCESS;
}

/**
 * @brief A function to read a series of tokens into YmlData structs.
 * @param tokens A list of tokens to be converted.
 * @param yml A pointer to the yml file.
 * @param keys_list A preallocated, empty list that can be used to store the keys needed to access the yml file.
 * @throws YML_EXCEPTION if a syntax error is detected.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 */
exception ymlFromTokensList(const List* tokens, YmlFile* yml, List* keys_list) {
    // when an ID is encountered, the last item is swapped with the new id
    // therefore, this initial null value is needed to allow for this swap
    listAddItem(keys_list, 0);
    char* key = 0;
    //exception tek_exception = SUCCESS;

    const ListItem* item = tokens->data;
    while (item) {
        const Token* token = item->data;
        // using ymlSetList requires a list of keys, in the order of access, so:
        // material:
        //   color: "red"
        // is accessed with the list ["material", "color"]

        // to begin, we should check for control tokens
        // if there is an ID, swap out the last item
        if (token->type == ID_TOKEN) {
            tekChainThrow(listPopItem(keys_list, &key));
            if (key) free(key);
            tekChainThrow(ymlWordToString(token->word, &key));
            tekChainThrow(listAddItem(keys_list, key));
            item = item->next;
            continue;
        }
        // if going "out", or extending the depth of the data, add a null to signify another layer of depth
        if (token->type == OUT_TOKEN) {
            tekChainThrow(listAddItem(keys_list, 0));
            item = item->next;
            continue;
        }
        // if going "in", or reducing the depth, pop the item to signify a smaller depth
        if (token->type == IN_TOKEN) {
            tekChainThrow(listPopItem(keys_list, &key));
            if (key) free(key);
            item = item->next;
            continue;
        }
        // for any control token, we should skip to processing the next item
        // no further processing needs to be done for them

        // now start creating the yml data to write at this location
        YmlData* yml_data;

        // if the token is a list, we could be adding several values under the same key
        // this could require going through several items in the token list
        if ((token->type & LIST_TOKEN) == LIST_TOKEN) {
            List* yml_list = (List*)malloc(sizeof(List));
            if (!yml_list) tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for yml list.");
            listCreate(yml_list);

            // store a pointer to prev_item
            // if we iterate to the next item, and it is not part of the list, then the inner loop will end
            // however, the outer loop will always iterate to the next item, skipping this item
            // for this reason, we need to try and leave the iterator in a good position for the loop
            const ListItem* prev_item = item;

            // use external function to add all items of list
            // if it fails, then we need to free the data it removed, otherwise we will lose pointer to it.
            tekChainThrowThen(ymlFromTokensListAddList(&prev_item, &item, yml_list, &yml_data), {
                const ListItem* del_item = yml_list->data;
                while (del_item) {
                    YmlData* del_data = del_item->data;
                    ymlDeleteData(del_data);
                    del_item = del_item->next;
                }
                listDelete(yml_list);
                free(yml_list);
            });
            item = prev_item;
        // non list tokens should just be added normally
        } else
            tekChainThrow(ymlCreateAutoDataToken(token, &yml_data));

        // either list or not, something should have been written to yml_data that we can add to the structure
        if (!yml_data)
            tekThrow(YML_EXCEPTION, "Failed to create yml data.")

        tekChainThrow(ymlSetList(yml, yml_data, keys_list));
        item = item->next;
    }
    return SUCCESS;
}

/**
 * @copybrief ymlFromTokensList
 * @param tokens
 * @param yml
 * @throws YML_EXCEPTION if a syntax error is detected.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 */
exception ymlFromTokens(const List* tokens, YmlFile* yml) {
    List keys_list = {};
    listCreate(&keys_list);

    // use pre-made list to run helper function
    // this means the list can be properly freed if something goes wrong, even if we return from the while loop early.
    const exception tek_exception = ymlFromTokensList(tokens, yml, &keys_list);
    if (tek_exception) {
        listFreeAllData(&keys_list);
        listDelete(&keys_list);
    }

    return tek_exception;
}

/**
 * @brief Format and print YmlData
 * @note This function will print with a newline character at the end.
 * @param data The YmlData to print
 */
void ymlPrintData(const YmlData* data) {
     if (data->type == STRING_DATA) printf("%s\n", (char*)data->value);
     if (data->type == INTEGER_DATA) printf("%ld\n", (long)data->value);
     if (data->type == FLOAT_DATA) {
        // cannot cast a void* to double, so need to memcpy
         double number = 0;
         memcpy(&number, &data->value, sizeof(void*));
         printf("%f\n", number);
     }

}

/**
 * @brief A helper function to be called recursively, tracking the current indentation that we are printing at.
 * @param yml The yml file to be printed.
 * @param indent The current indentation.
 * @throws MEMORY_EXCEPTION If failed to allocate memory for keys/values to print.
 */
exception ymlPrintIndent(YmlFile* yml, uint indent) {
    if (!yml) tekThrow(NULL_PTR_EXCEPTION, "Cannot print null pointer.");

    const char** keys;
    tekChainThrow(hashtableGetKeys(yml, &keys));

    YmlData** values;
    tekChainThrow(hashtableGetValues(yml, &values));

    const uint num_items = yml->num_items;
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
            // if a list, print each item separately
            if (values[i]->type == LIST_DATA) {
                List* yml_list = values[i]->value;
                ListItem* item = yml_list->data;
                printf("\n");
                while (item) {
                    const YmlData* list_data = item->data;
                    // format so that it sits underneath the key, indented correctly
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

/**
 * @brief Formats and prints yml file.
 * @param yml The yml file to print
 * @throws MEMORY_EXCEPTION if failed to allocate memory for keys/values to print
 */
exception ymlPrint(YmlFile* yml) {
    tekChainThrow(ymlPrintIndent(yml, 0));
    return SUCCESS;
}

/**
 * @brief Take a filename and read its content into a yml data structure.
 * @param filename The filename of the yml file to be read.
 * @param yml A pointer to an existing but empty YmlFile struct to read the data into.
 * @return
 */
exception ymlReadFile(const char* filename, YmlFile* yml) {
    tekChainThrow(ymlCreate(yml));

    // allocate enough memory and read the file into it
    uint file_size;
    tekChainThrow(getFileSize(filename, &file_size))
    char* buffer = (char*)malloc(file_size * sizeof(char));
    if (!buffer) tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory to read yml file into.");
    tekChainThrow(readFile(filename, file_size, buffer));

    List split_text;
    listCreate(&split_text);

    // split the text into atomic units, the supposed keys and values
    tekChainThrowThen(ymlSplitText(buffer, file_size, &split_text), {
        listFreeAllData(&split_text);
        listDelete(&split_text);
        free(buffer);
    });

    List tokens;
    listCreate(&tokens);

    // check the split text and convert to useable tokens
    tekChainThrowThen(ymlCreateTokens(&split_text, &tokens), {
        listFreeAllData(&split_text);
        listFreeAllData(&tokens);
        listDelete(&split_text);
        listDelete(&tokens);
        free(buffer);
    });

    // build yml data from tokens
    tekChainThrowThen(ymlFromTokens(&tokens, yml), {
        listFreeAllData(&split_text);
        listFreeAllData(&tokens);
        listDelete(&split_text);
        listDelete(&tokens);
        ymlDelete(yml);
        free(buffer);
    });

    listFreeAllData(&split_text);
    listFreeAllData(&tokens);
    listDelete(&split_text);
    listDelete(&tokens);
    free(buffer);

    return SUCCESS;
}
