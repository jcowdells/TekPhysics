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

void hashtableDelete(const HashTable*);
exception hashtableCreate(HashTable*, unsigned int);
exception hashtableHash(HashTable*, const char*, unsigned int*);
exception hashtableCreateNode(const HashTable*, const char*, unsigned int, HashNode**);
exception hashtableGetNode(HashTable*, const char*, HashNode**);
exception hashtableGetKeys(const HashTable*, char***);
exception hashtableGetValues(const HashTable*, void***);
exception hashtableRehash(HashTable*, unsigned int);
flag hashtableTooFull(const HashTable*);
exception hashtableGet(HashTable*, const char*, void**);
exception hashtableSet(HashTable*, const char*, void*);
exception hashtableRemove(HashTable*, const char*);
void hashtablePrint(const HashTable*);
void hashtablePrintItems(const HashTable*);
void hashtablePrintInternal(const HashTable*);