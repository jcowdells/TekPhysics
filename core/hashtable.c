#include "hashtable.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Delete a hashtable, freeing all the memory that the hashtable functions allocated.
 * @note This does not free any user allocated memory, for example pointers that are stored in the hashtable.
 * @param[in] hashtable A pointer to the hashtable to delete.
 */
void hashtableDelete(const HashTable* hashtable) {
    if (!hashtable) return;

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

/**
 * @brief Create a hashtable. The length is relatively unimportant, setting it too small just means more rehashes.
 * @param hashtable[out] A pointer to an empty hashtable struct.
 * @param length[in] The starting size of the hashtable. If this size is filled by 75%, the hashtable will double its size.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 */
exception hashtableCreate(HashTable* hashtable, const unsigned int length) {
    // check for random scenarios such as no hashtable or already allocated hashtable
    if (!hashtable) tekThrow(NULL_PTR_EXCEPTION, "Cannot create hashtable from null ptr.");
    //if (hashtable->internal) hashtableDelete(hashtable);

    // allocate room for the internal array, and set some default values
    hashtable->internal = (HashNode**)calloc(length, sizeof(HashNode*));
    if (!hashtable->internal) tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for hashtable.");
    hashtable->length = length;
    hashtable->num_items = 0;
    return SUCCESS;
}

/**
 * @brief Create a hash index for a hashtable given a string. Essentially works by summing ASCII values of each character, modulo length of internal array.
 * @param[in] hashtable A pointer to the hashtable.
 * @param[in] key The string key to hash with.
 * @param[out] hash The returned hash value.
 * @throws HASHTABLE_EXCEPTION if the hashtable is empty.
 */
static exception hashtableHash(HashTable* hashtable, const char* key, unsigned int* hash) {
    // make sure we dont have bogus data
    if (!hashtable || !key || !hash) tekThrow(NULL_PTR_EXCEPTION, "Cannot hash key from null ptr.");
    if (!hashtable->length) tekThrow(HASHTABLE_EXCEPTION, "Cannot get hash index for an empty hashtable.");
    // simple hashing algortithm for strings
    // sum ascii values, and then modulo to stop it exceeding the maximum size of the internal array
    const size_t len_key = strlen(key);
    *hash = 0;
    for (size_t i = 0; i < len_key; i++) {
        *hash = (*hash + key[i]) % hashtable->length;
    }
    return SUCCESS;
}

/**
 * @brief Create a node for a hashtable, and insert it into the hashtable correctly.
 * @param[in] hashtable A pointer to the hashtable.
 * @param[in] key The key which names the node.
 * @param[in] hash The hash index at which to store the node.
 * @param[out] out_ptr A pointer to store a reference to the created node for further use.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 */
static exception hashtableCreateNode(const HashTable* hashtable, const char* key, const unsigned int hash, HashNode** out_ptr) {
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

/**
 * @brief Check if a node exists in the hashtable, and if so return a pointer to this node.
 * @note The retrieved node is not a copy, and will be freed once the hashtable is freed.
 * @param[in] hashtable A pointer to the hashtable.
 * @param[in] key The string key at which the node is stored.
 * @param[out] node_ptr A pointer to store a pointer to the node if found.
 * @throws NULL_PTR_EXCEPTION if hashtable is NULL
 * @returns SUCCESS if the node was found, FAILURE if it was not.
 */
static exception hashtableGetNode(HashTable* hashtable, const char* key, HashNode** node_ptr) {
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

/**
 * @brief Return a list of keys that make up the hashtable.
 * @note The function will allocate an array at the pointer specified that needs to be freed. The length of this array will be hashtable.num_items. (hashtable.length specifies the length of the internal array).
 * @param hashtable A pointer to the hashtable.
 * @param keys A pointer to an array of char arrays that will be allocated and filled with keys.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 */
exception hashtableGetKeys(const HashTable* hashtable, char*** keys) {
    if (!hashtable) tekThrow(NULL_PTR_EXCEPTION, "Cannot get keys from null ptr.");

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

/**
 * @brief Return a list of values stored in the hashtable.
 * @note The function will allocate memory to store the values in, which needs to be freed. The order of the values will match the order of keys retrieved by hashtableGetKeys.
 * @param hashtable A pointer to the hashtable.
 * @param values A pointer to an array of void pointers that will be filled with the values contained in the hashtable.
 * @throws MEMORY_EXCEPTION if could not allocate array to store values.
 */
exception hashtableGetValues(const HashTable* hashtable, void*** values) {
    if (!hashtable) tekThrow(NULL_PTR_EXCEPTION, "Cannot get values from null ptr.");

    // allocate some memory to store a list of all values
    *values = (void**)malloc(hashtable->num_items * sizeof(void*));
    if (!(*values)) tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for list of values.");

    // iterate over each item in the array
    unsigned int count = 0;
    for (unsigned int i = 0; i < hashtable->length; i++) {
        // iterate over each value in the linked list
        const HashNode* node_ptr = hashtable->internal[i];
        while (node_ptr) {
            // add to list
            (*values)[count++] = node_ptr->data;
            node_ptr = node_ptr->next;
        }
    }
    return SUCCESS;
}

/**
 * @brief Resize the hashtable to be a new size, copying the items into new locations.
 * @note By the nature of how hashtables work, the order of items from hashtableGetKeys/Values will change after rehashing.
 * @param hashtable A pointer to the hashtable.
 * @param new_length The new length required by the hashtable.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 */
static exception hashtableRehash(HashTable* hashtable, const unsigned int new_length) {
    if (!hashtable) tekThrow(NULL_PTR_EXCEPTION, "Cannot rehash from null ptr.");

    // make room to copy the keys and values of the hash table
    char** keys = (char**)calloc(hashtable->num_items, sizeof(char*));
    if (!keys) tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for copy of keys.");
    void** values = (void**)calloc(hashtable->num_items, sizeof(void*));
    if (!values) {
        free(keys);
        tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for copy of values.");
    }

    // iterate over each item of the array
    unsigned int count = 0;
    for (unsigned int i = 0; i < hashtable->length; i++) {
        // iterate over each item in the linked list
        const HashNode* node_ptr = hashtable->internal[i];
        while (node_ptr) {
            // find the length of the key and copy it to a new array, and store the pointer
            // also store the value in the values array
            const size_t len_key = strlen(node_ptr->key) + 1;
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
        const exception tek_exception = hashtableSet(hashtable, keys[i], values[i]);

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

/**
 * @brief Returns whether a hashtable is more than 75% full, at which point the number of collisions makes the hashtable slower, requiring the linked lists to be traversed more often.
 * @param hashtable A pointer to the hashtable.
 * @return 1 if the internal array is more than 75% full, 0 otherwise
 */
static flag hashtableTooFull(const HashTable* hashtable) {
    if (!hashtable) tekThrow(NULL_PTR_EXCEPTION, "Cannot check if null ptr is full.");
    // return true if the hashtable is more than 75% full
    return (flag)((4 * hashtable->num_items) >= (3 * hashtable->length));
}

/**
 *
 * @param hashtable A pointer to the hashtable.
 * @param key The key to the wanted data.
 * @param data A pointer that will be filled with the stored data.
 * @throws HASHTABLE_EXCEPTION if the hashtable is empty.
 */
exception hashtableGet(HashTable* hashtable, const char* key, void** data) {
    if (!hashtable) tekThrow(NULL_PTR_EXCEPTION, "Cannot get from null ptr.");

    // find the index of the internal array
    unsigned int hash;
    tekChainThrow(hashtableHash(hashtable, key, &hash));

    // retrieve the node in the linked list
    HashNode* node_ptr;
    exception tek_exception = hashtableGetNode(hashtable, key, &node_ptr);

    if (tek_exception) {
        if (tek_exception == FAILURE) return FAILURE;
        else tekChainThrow(tek_exception);
    }

    // return the data stored at that node
    *data = node_ptr->data;
    return SUCCESS;
}

/**
 * @brief Set the value of the hashtable at a certain key.
 * @param hashtable A pointer to the hashtable.
 * @param key The key to set the data at.
 * @param data The actual data to set.
 * @throws MEMORY_EXCEPTION if malloc() fails.
 */
exception hashtableSet(HashTable* hashtable, const char* key, void* data) {
    if (!hashtable) tekThrow(NULL_PTR_EXCEPTION, "Cannot set null ptr..");

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
        hashtable->num_items += 1;
    }

    // set the data and update num of items
    node_ptr->data = data;
    return SUCCESS;
}

/**
 * @brief Remove the data stored at a certain key in the hashtable.
 * @note This does not free the pointer stored at this key, data stored here should be freed first.
 * @param hashtable A pointer to the hashtable.
 * @param key The key to remove.
 * @throws HASHTABLE_EXCEPTION if the hashtable is empty.
 * @throws FAILURE if the key does not exist.
 */
exception hashtableRemove(HashTable* hashtable, const char* key) {
    if (!hashtable) tekThrow(NULL_PTR_EXCEPTION, "Cannot remove from null ptr.");

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

/**
 * @brief Check whether an item exists in a hashtable.
 * @param hashtable A pointer to the hashtable.
 * @param key A key to an item to check
 * @return 1 if the item exists, 0 if it does not or an error occurred while looking for it
 */
flag hashtableHasKey(HashTable* hashtable, const char* key) {
    // attempt to get node, if fails then there is no node.
    // kinda large overhead from writing exception buffer and whatnot
    HashNode* hash_node = 0;
    const exception hashtable_result =  hashtableGetNode(hashtable, key, &hash_node);
    if (hashtable_result == SUCCESS) return 1;
    return 0;
}

/**
 * @brief Print the data about a hashtable, which will print something like
 * @code
 * HashNode* internal  = 0x123456789
 * unsigned int length = 100
 * @endcode
 * @param hashtable A pointer to a hashtable
 */
void hashtablePrint(const HashTable* hashtable) {
    // print
    printf("HashNode* internal  = %p\n", hashtable->internal);
    printf("unsigned int length = %u\n", hashtable->length);
}

/**
 * @brief Print a hashtable in a nice format, something like
 * @code
 * {
 *     "key_1": 0x12987123
 *     "key_2": 0x12312344
 *     "key_3": 0x11245453
 * }
 * @endcode
 * @param hashtable A pointer to a hashtable.
 */
void hashtablePrintItems(const HashTable* hashtable) {
    printf("%p\n", hashtable);
    if (!hashtable) { // if no pointer, ofc cannot print anything
        printf("hashtable = NULL !");
        return;
    }
    if (!hashtable->internal) { // no data
        printf("hashtable->internal = NULL !");
        return;
    }
    printf("{\n");
    for (unsigned int i = 0; i < hashtable->length; i++) { // loop through and print each item in the internal array
        const HashNode* node_ptr = hashtable->internal[i];
        while (node_ptr) {
            printf("    \"%s\" = %ld (hash=%d)\n", node_ptr->key, (long)node_ptr->data, i);
            node_ptr = node_ptr->next;
        }
    }
    printf("}\n");
}

/**
 * @brief Print out the internal array of the hashtable, which will be a mix of 0s and pointers to linked lists.
 * @note This is completely useless! (Except for when I was debugging this)
 * @param hashtable A pointer to a hashtable.
 */
void hashtablePrintInternal(const HashTable* hashtable) {
    printf("{\n");
    // code predating the "typedef uint" in tekgl.h!!!
    // estimated time of writing: january 2025
    for (unsigned int i = 0; i < hashtable->length; i++) {
        printf("    internal[%u] = %p\n", i, hashtable->internal[i]);
    }
    printf("}\n");
}
