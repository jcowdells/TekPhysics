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
exception hashtableHash(HashTable* hashtable, const char* key, unsigned int* hash);
exception hashtableCreateNode(const HashTable* hashtable, const char* key, unsigned int hash, HashNode** out_ptr);
exception hashtableGetNode(HashTable* hashtable, const char* key, HashNode** node_ptr);
exception hashtableGetKeys(const HashTable* hashtable, char*** keys);
exception hashtableGetValues(const HashTable* hashtable, void*** values);
exception hashtableRehash(HashTable* hashtable, unsigned int new_length);
flag hashtableTooFull(const HashTable* hashtable);
exception hashtableGet(HashTable* hashtable, const char* key, void** data);
exception hashtableSet(HashTable* hashtable, const char* key, void* data);
exception hashtableRemove(HashTable* hashtable, const char* key);
flag hashtableHasKey(HashTable* hashtable, const char* key);
void hashtablePrint(const HashTable* hashtable);
void hashtablePrintItems(const HashTable* hashtable);
void hashtablePrintInternal(const HashTable* hashtable);