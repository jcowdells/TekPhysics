#pragma once

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
#define ymlGet(yml, data, ...) ymlGetVA(yml, data, __VA_ARGS__, 0)

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
