#include "hashtable.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void hashtableDelete(HashTable* hashtable) {
    // iterate over every possible index in the hash table
    for (int i = 0; i < hashtable->length; i++) {
        HashNode* node_ptr = hashtable->internal[i];
        // loop through the associated linked list
        while (node_ptr) {
            // free the key as we allocated a copy
            if (node_ptr->key) free(node_ptr->key);
            // keep track of pointer so that we can free it without issues
            HashNode* temp_ptr = node_ptr->next;
            free(node_ptr);
            node_ptr = temp_ptr;
        }
    }
    // finally free the hashtable struct
    free(hashtable->internal);
}

exception hashtableCreate(HashTable* hashtable, const unsigned int length) {
    // check for random scenarios such as no hashtable or already allocated hashtable
    if (!hashtable) tekThrow(NULL_PTR_EXCEPTION, "Cannot create hashtable from null ptr.");
    if (hashtable->internal) hashtableDelete(hashtable);

    // allocate room for the internal array, and set some default values
    hashtable->internal = (HashNode**)calloc(length, sizeof(HashNode*));
    if (!hashtable->internal) tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for hashtable.");
    hashtable->length = length;
    hashtable->num_items = 0;
    return SUCCESS;
}

exception hashtableHash(HashTable* hashtable, const char* key, unsigned int* hash) {
    // make sure we dont have bogus data
    if (!hashtable || !key || !hash) tekThrow(NULL_PTR_EXCEPTION, "Cannot hash key from null ptr.");

    // simple hashing algortithm for strings
    // sum ascii values, and then modulo to stop it exceeding the maximum size of the internal array
    const size_t len_key = strlen(key);
    *hash = 0;
    for (size_t i = 0; i < len_key; i++) {
        *hash = (*hash + key[i]) % hashtable->length;
    }
    return SUCCESS;
}

exception hashtableCreateNode(const HashTable* hashtable, const char* key, unsigned int hash, HashNode** out_ptr) {
    // ensure there isn't a null ptr
    if (!hashtable) tekThrow(NULL_PTR_EXCEPTION, "Cannot create node from null ptr.");

    // make a temporary node
    HashNode node = {};

    // set next ptr to first item of internal array
    node.next = hashtable->internal[hash];

    // if node.next is null, then it means it is the first item of the linked list
    const int first = !node.next;

    // iterate over the list to find the final item
    HashNode* node_ptr = &node;
    while (node_ptr->next) node_ptr = node_ptr->next;
    node_ptr->next = (HashNode*)calloc(1, sizeof(HashNode));
    if (!node_ptr->next) tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for new node.");

    // mallocate some memory to store a copy of the key
    char* key_copy = (char*)malloc(strlen(key) + 1);
    if (!key_copy) tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for copy of key.");
    memcpy(key_copy, key, strlen(key) + 1);
    node_ptr->next->key = key_copy;

    // if this is the first item, update the actual array to indicate that there is a list here
    if (first) hashtable->internal[hash] = node_ptr->next;

    // return reference to the new node
    *out_ptr = node_ptr->next;
    return SUCCESS;
}

exception hashtableGetNode(HashTable* hashtable, const char* key, HashNode** node_ptr) {
    // ensure not a null ptr
    if (!hashtable) tekThrow(NULL_PTR_EXCEPTION, "Cannot get node from null ptr.");

    // find the hash index
    unsigned int hash;
    tekChainThrow(hashtableHash(hashtable, key, &hash));

    // iterate over the hashtable
    *node_ptr = hashtable->internal[hash];
    int found = 0;
    while (*node_ptr) {
        // if the key is equal to what we are searching for, then stop
        if (!strcmp(key, (*node_ptr)->key)) {
            found = 1;
            break;
        }
        *node_ptr = (*node_ptr)->next;
    }

    // if node pointer was not found, make sure we output this
    if (!found) *node_ptr = 0;
    return (found) ? SUCCESS : FAILURE;
}

exception hashtableGetKeys(const HashTable* hashtable, char*** keys) {
    // mallocate some memory to store a list of keys
    *keys = (char**)malloc(hashtable->num_items * sizeof(char*));
    if (!(*keys)) tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for list of keys.");

    // iterate over the internal array
    unsigned int count = 0;
    for (unsigned int i = 0; i < hashtable->length; i++) {
        // iterate over each linked list in the array
        const HashNode* node_ptr = hashtable->internal[i];
        while (node_ptr) {
            // add key to the list
            (*keys)[count++] = node_ptr->key;
            node_ptr = node_ptr->next;
        }
    }
    return SUCCESS;
}

exception hashtableGetValues(HashTable* hashtable, void*** values) {
    // allocate some memory to store a list of all values
    *values = (void**)malloc(hashtable->num_items * sizeof(void*));
    if (!(*values)) tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for list of values.");

    // iterate over each item in the array
    unsigned int count = 0;
    for (unsigned int i = 0; i < hashtable->length; i++) {
        // iterate over each value in the linked list
        HashNode* node_ptr = hashtable->internal[i];
        while (node_ptr) {
            // add to list
            (*values)[count++] = node_ptr->data;
            node_ptr = node_ptr->next;
        }
    }
    return SUCCESS;
}

exception hashtableRehash(HashTable* hashtable, unsigned int new_length) {
    // make room to copy the keys and values of the hash table
    char** keys = (char**)malloc(hashtable->num_items * sizeof(char*));
    if (!keys) tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for copy of keys.");
    void** values = (void**)malloc(hashtable->num_items * sizeof(void*));
    if (!values) {
        free(keys);
        tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for copy of values.");
    }

    // iterate over each item of the array
    unsigned int count = 0;
    for (unsigned int i = 0; i < hashtable->length; i++) {
        // iterate over each item in the linked list
        HashNode* node_ptr = hashtable->internal[i];
        while (node_ptr) {
            // find the length of the key and copy it to a new array, and store the pointer
            // also store the value in the values array
            size_t len_key = strlen(node_ptr->key) + 1;
            char* key_copy = (char*)malloc(len_key * sizeof(char));
            if (!key_copy) {
                for (unsigned int j = 0; j < count; j++) free(keys[j]);
                free(keys);
                free(values);
                tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for copy of key.");
            }
            memcpy(key_copy, node_ptr->key, len_key);
            keys[count] = key_copy;
            values[count] = node_ptr->data;
            count++;
            node_ptr = node_ptr->next;
        }
    }

    // create a new hash table with a different size
    tekChainThrow(hashtableCreate(hashtable, new_length));

    // iterate over each item in the internal array
    for (unsigned int i = 0; i < count; i++) {
        // iterate over each item in the linked list
        // add the record we copied
        exception tek_exception = hashtableSet(hashtable, keys[i], values[i]);

        // catch exception if it occurs
        if (tek_exception) {
            for (unsigned int j = 0; j < count; j++) free(keys[j]);
            free(keys);
            free(values);
            tekChainThrow(tek_exception);
        }
    }

    // free copies of keys
    for (unsigned int i = 0; i < count; i++) free(keys[i]);

    // free arrays
    free(keys);
    free(values);
    return SUCCESS;
}

flag hashtableTooFull(HashTable* hashtable) {
    // return true if the hashtable is more than 75% full
    return (4 * hashtable->num_items) >= (3 * hashtable->length);
}

exception hashtableGet(HashTable* hashtable, const char* key, void** data) {
    // find the index of the internal array
    unsigned int hash;
    tekChainThrow(hashtableHash(hashtable, key, &hash));

    // retrieve the node in the linked list
    HashNode* node_ptr;
    tekChainThrow(hashtableGetNode(hashtable, key, &node_ptr));

    // return the data stored at that node
    *data = node_ptr->data;
    return SUCCESS;
}

exception hashtableSet(HashTable* hashtable, const char* key, void* data) {
    // check if hashtable needs to be rehashed before we add the item
    if (hashtableTooFull(hashtable)) {
        tekChainThrow(hashtableRehash(hashtable, hashtable->length * 2));
    }

    // create a hash index
    unsigned int hash;
    tekChainThrow(hashtableHash(hashtable, key, &hash));

    // get the node where the data is stored
    HashNode* node_ptr;

    // if there is no node, then create one
    if (hashtableGetNode(hashtable, key, &node_ptr)) {
        tekChainThrow(hashtableCreateNode(hashtable, key, hash, &node_ptr));
    }

    // set the data and update num of items
    node_ptr->data = data;
    hashtable->num_items += 1;
    return 1;
}

exception hashtableRemove(HashTable* hashtable, const char* key) {
    // get the hash index
    unsigned int hash;
    tekChainThrow(hashtableHash(hashtable, key, &hash));

    // iterate and keep track of current and previous nodes
    HashNode* prev_ptr = 0;
    HashNode* node_ptr = hashtable->internal[hash];
    int found = FAILURE;
    while (node_ptr) {
        // if we found the key to remove
        if (!strcmp(key, node_ptr->key)) {
            found = SUCCESS;

            // if there is a previous pointer, then push the next pointer down the line
            if (prev_ptr) {
                prev_ptr->next = node_ptr->next;
            // otherwise, there will be a new root node
            } else {
                hashtable->internal[hash] = node_ptr->next;
            }

            // free the node and the copy of the key
            if (node_ptr->key) free(node_ptr->key);
            free(node_ptr);

            // update number of items
            hashtable->num_items -= 1;
            break;
        }

        // update pointers
        prev_ptr = node_ptr;
        node_ptr = node_ptr->next;
    }
    return found;
}

void hashtablePrint(HashTable* hashtable) {
    printf("HashNode* internal  = %p\n", hashtable->internal);
    printf("unsigned int length = %u\n", hashtable->length);
}

void hashtablePrintItems(HashTable* hashtable) {
    printf("{\n");
    for (unsigned int i = 0; i < hashtable->length; i++) {
        HashNode* node_ptr = hashtable->internal[i];
        while (node_ptr) {
            printf("    \"%s\" = %ld (hash=%d)\n", node_ptr->key, (long)node_ptr->data, i);
            node_ptr = node_ptr->next;
        }
    }
    printf("}\n");
}

void hashtablePrintInternal(HashTable* hashtable) {
    printf("{\n");
    for (unsigned int i = 0; i < hashtable->length; i++) {
        printf("    internal[%u] = %p\n", i, hashtable->internal[i]);
    }
    printf("}\n");
}