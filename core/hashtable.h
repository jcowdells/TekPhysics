#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "exception.h"

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

void hashtable_free(HashTable*);
int hashtable_create(HashTable*, unsigned int);
int hashtable_hash(HashTable*, const char*, unsigned int*);
int hashtable_create_node(const HashTable*, const char*, unsigned int, HashNode**);
int hashtable_get_node(HashTable*, const char*, HashNode**);
int hashtable_get_keys(const HashTable*, char***);
int hashtable_get_values(HashTable*, void***);
int hashtable_rehash(HashTable*, unsigned int);
int hashtable_too_full(HashTable*);
int hashtable_get(HashTable*, const char*, void**);
int hashtable_set(HashTable*, const char*, void*);
int hashtable_delete(HashTable*, const char*);
void hashtable_print(HashTable*);
void hashtable_print_items(HashTable*);
void hashtable_print_internal(HashTable*);
int main(int, char**);
