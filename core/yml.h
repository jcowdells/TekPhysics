#pragma once

#define YML_

typedef struct YmlTree {
    char* key;
    void* value;
    struct YmlTree* next;
} YmlTree;

typedef struct YmlList {

} YmlList;