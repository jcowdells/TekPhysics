#pragma once

#include "../tekgl.h"

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