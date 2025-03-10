#pragma once

#include "../tekgl.h"
#include "../core/exception.h"

typedef struct YmlData {
    flag type;
    void* value;
} YmlData;

typedef struct YmlTree {
    char* key;
    YmlData data;
} YmlTree;

typedef struct YmlList {
    YmlData data;
    struct YmlList* next;
} YmlList;

typedef YmlList* YmlFile;

exception ymlReadFile(const char* filename, YmlFile* yml);