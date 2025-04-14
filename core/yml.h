#pragma once

#define YML_DATA     0
#define STRING_DATA  1
#define INTEGER_DATA 2
#define FLOAT_DATA   3
#define LIST_DATA    4

#include "../tekgl.h"
#include "../core/exception.h"
#include "../core/hashtable.h"

typedef struct YmlData {
    flag type;
    void* value;
} YmlData;

typedef HashTable YmlFile;

exception ymlCreate(YmlFile* yml);
exception ymlReadFile(const char* filename, YmlFile* yml);

exception ymlGetVA(YmlFile* yml, YmlData** data, ...);
/// @copydoc ymlGetVA
#define ymlGet(yml, data, ...) ymlGetVA(yml, data, __VA_ARGS__, 0)

exception ymlGetKeysVA(YmlFile* yml, char*** yml_keys, uint* num_keys, ...);
/// @copydoc ymlGetKeysVA
#define ymlGetKeys(yml, data, num_keys, ...) ymlGetKeysVA(yml, data, num_keys, __VA_ARGS__, 0);

exception ymlDataToString(const YmlData* yml_data, char** string);
exception ymlDataToInteger(const YmlData* yml_data, long* integer);
exception ymlDataToFloat(const YmlData* yml_data, double* number);

exception ymlListGetString(const YmlData* yml_list, uint index, char** string);
exception ymlListGetInteger(const YmlData* yml_list, uint index, long* integer);
exception ymlListGetFloat(const YmlData* yml_list, uint index, double* number);

exception ymlListToStringArray(const YmlData* yml_list, char*** array, uint* len_array);
exception ymlListToIntegerArray(const YmlData* yml_list, long** array, uint* len_array);
exception ymlListToFloatArray(const YmlData* yml_list, double** array, uint* len_array);

exception ymlSetVA(YmlFile* yml, YmlData* data, ...);
#define ymlSet(yml, data, ...) ymlSetVA(yml, data, __VA_ARGS__, 0)

exception ymlRemoveVA(YmlFile* yml, ...);
#define ymlRemove(yml, ...) ymlRemoveVA(yml, __VA_ARGS__, 0)

exception ymlDelete(YmlFile* yml);

exception ymlPrint(YmlFile* yml);
