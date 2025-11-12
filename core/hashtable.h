#pragma once

#include "exception.h"
#include "../tekgl.h"

typedef struct HashNode {
    char* key;
    void* data;
    struct HashNode* next;
} HashNode;

typedef struct HashTable {
    HashNode** internal;
    unsigned int length;
    unsigned int num_items;
} HashTable;

void hashtableDelete(const HashTable* hashtable);
exception hashtableCreate(HashTable* hashtable, unsigned int length);
exception hashtableGetKeys(const HashTable* hashtable, char*** keys);
exception hashtableGetValues(const HashTable* hashtable, void*** values);
exception hashtableGet(HashTable* hashtable, const char* key, void** data);
exception hashtableSet(HashTable* hashtable, const char* key, void* data);
exception hashtableRemove(HashTable* hashtable, const char* key);
flag hashtableHasKey(HashTable* hashtable, const char* key);
void hashtablePrint(const HashTable* hashtable);
void hashtablePrintItems(const HashTable* hashtable);
void hashtablePrintInternal(const HashTable* hashtable);