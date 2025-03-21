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

exception ymlSetVA(YmlFile* yml, YmlData* data, ...);
#define ymlSet(yml, data, ...) ymlSetVA(yml, data, __VA_ARGS__, 0)

exception ymlRemoveVA(YmlFile* yml, ...);
#define ymlRemove(yml, ...) ymlRemoveVA(yml, __VA_ARGS__, 0)

exception ymlDelete(YmlFile* yml);

exception ymlPrint(YmlFile* yml);
