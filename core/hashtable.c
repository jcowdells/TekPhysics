#include "hashtable.h"
#include "exception.h"

void hashtable_free(const HashTable* hashtable) {
    for (int i = 0; i < hashtable->length; i++) {
        HashNode* node_ptr = hashtable->internal[i];
        while (node_ptr) {
            if (node_ptr->key) free(node_ptr->key);
            HashNode* temp_ptr = node_ptr->next;
            free(node_ptr);
            node_ptr = temp_ptr;
        }
    }
    free(hashtable->internal);
}

int hashtable_create(HashTable* hashtable, const unsigned int length) {
    if (!hashtable) tekThrow(NULL_PTR_EXCEPTION, "");
    if (hashtable->internal) hashtable_free(hashtable);

    hashtable->internal = (HashNode**)calloc(length, sizeof(HashNode*));
    if (!hashtable->internal) return MEMORY_EXCEPTION;
    hashtable->length = length;
    hashtable->num_items = 0;
    return SUCCESS;
}

int hashtable_hash(HashTable* hashtable, const char* key, unsigned int* hash) {
    if (!hashtable || !key || !hash) return NULL_PTR_EXCEPTION;
    const size_t len_key = strlen(key);
    *hash = 0;
    for (size_t i = 0; i < len_key; i++) {
        *hash = (*hash + key[i]) % hashtable->length;
    }
    return SUCCESS;
}

int hashtable_create_node(const HashTable* hashtable, const char* key, unsigned int hash, HashNode** out_ptr) {
    if (!hashtable) return NULL_PTR_EXCEPTION;
    HashNode node = {};
    node.next = hashtable->internal[hash];
    const int first = !node.next;
    HashNode* node_ptr = &node;
    while (node_ptr->next) node_ptr = node_ptr->next;
    node_ptr->next = (HashNode*)calloc(1, sizeof(HashNode));
    if (!node_ptr->next) return MEMORY_EXCEPTION;
    char* key_copy = (char*)malloc(strlen(key) + 1);
    if (!key_copy) return 0;
    memcpy(key_copy, key, strlen(key) + 1);
    node_ptr->next->key = key_copy;
    if (first) hashtable->internal[hash] = node_ptr->next;
    *out_ptr = node_ptr->next;
    return SUCCESS;
}

int hashtable_get_node(HashTable* hashtable, const char* key, HashNode** node_ptr) {
    if (!hashtable) return NULL_PTR_EXCEPTION;
    unsigned int hash; hashtable_hash(hashtable, key, &hash);
    *node_ptr = hashtable->internal[hash];
    int found = 0;
    while (*node_ptr) {
        if (!strcmp(key, (*node_ptr)->key)) {
            found = 1;
            break;
        }
        *node_ptr = (*node_ptr)->next;
    }
    if (!found) *node_ptr = 0;
    return (found) ? SUCCESS : FAILURE;
}

int hashtable_get_keys(const HashTable* hashtable, char*** keys) {
    *keys = (char**)malloc(hashtable->num_items * sizeof(char*));
    if (!(*keys)) return 0;
    unsigned int count = 0;
    for (unsigned int i = 0; i < hashtable->length; i++) {
        const HashNode* node_ptr = hashtable->internal[i];
        while (node_ptr) {
            (*keys)[count++] = node_ptr->key;
            node_ptr = node_ptr->next;
        }
    }
    return 1;
}

int hashtable_get_values(HashTable* hashtable, void*** values) {
    *values = (void**)malloc(hashtable->num_items * sizeof(void*));
    if (!(*values)) return 0;
    unsigned int count = 0;
    for (unsigned int i = 0; i < hashtable->length; i++) {
        HashNode* node_ptr = hashtable->internal[i];
        while (node_ptr) {
            (*values)[count++] = node_ptr->data;
            node_ptr = node_ptr->next;
        }
    }
    return 1;
}

int hashtable_rehash(HashTable* hashtable, unsigned int new_length) {
    char** keys = (char**)malloc(hashtable->num_items * sizeof(char*));
    if (!keys) return 0;
    void** values = (void**)malloc(hashtable->num_items * sizeof(void*));
    if (!values) {
        free(keys);
        return 0;
    }
    unsigned int count = 0;
    for (unsigned int i = 0; i < hashtable->length; i++) {
        HashNode* node_ptr = hashtable->internal[i];
        while (node_ptr) {
            size_t len_key = strlen(node_ptr->key) + 1;
            char* key_copy = (char*)malloc(len_key * sizeof(char));
            if (!key_copy) {
                for (unsigned int j = 0; j < count; j++) free(keys[j]);
                free(keys);
                free(values);
                return 0;
            }
            memcpy(key_copy, node_ptr->key, len_key);
            keys[count] = key_copy;
            values[count] = node_ptr->data;
            count++;
            node_ptr = node_ptr->next;
        }
    }
    hashtable_create(hashtable, new_length);
    for (unsigned int i = 0; i < count; i++) {
        if (!hashtable_set(hashtable, keys[i], values[i])) {
            for (unsigned int j = 0; j < count; j++) free(keys[j]);
            free(keys);
            free(values);
            return 0;
        }
    }
    for (unsigned int i = 0; i < count; i++) free(keys[i]);
    free(keys);
    free(values);
    return 1;
}

int hashtable_too_full(HashTable* hashtable) {
    return (4 * hashtable->num_items) >= (3 * hashtable->length);
}

int hashtable_get(HashTable* hashtable, const char* key, void** data) {
    unsigned int hash;
    if (!hashtable_hash(hashtable, key, &hash)) return 0;
    HashNode* node_ptr;
    if (!hashtable_get_node(hashtable, key, &node_ptr)) return 0;
    *data = node_ptr->data;
    return 1;
}

int hashtable_set(HashTable* hashtable, const char* key, void* data) {
    if (hashtable_too_full(hashtable)) {
        if (!hashtable_rehash(hashtable, hashtable->length * 2)) return 0;
    }
    unsigned int hash;
    if (!hashtable_hash(hashtable, key, &hash)) return 0;
    HashNode* node_ptr;
    if (!hashtable_get_node(hashtable, key, &node_ptr))
        hashtable_create_node(hashtable, key, hash, &node_ptr);
    node_ptr->data = data;
    hashtable->num_items += 1;
    return 1;
}

int hashtable_delete(HashTable* hashtable, const char* key) {
    unsigned int hash;
    if (!hashtable_hash(hashtable, key, &hash)) return 0;
    HashNode* prev_ptr = 0;
    HashNode* node_ptr = hashtable->internal[hash];
    int found = 0;
    while (node_ptr) {
        if (!strcmp(key, node_ptr->key)) {
            found = 1;
            if (prev_ptr) {
                prev_ptr->next = node_ptr->next;
            } else {
                hashtable->internal[hash] = node_ptr->next;
            }
            if (node_ptr->key) free(node_ptr->key);
            free(node_ptr);
            hashtable->num_items -= 1;
            break;
        }
        prev_ptr = node_ptr;
        node_ptr = node_ptr->next;
    }
    return found;
}

void hashtable_print(HashTable* hashtable) {
    printf("HashNode* internal  = %p\n", hashtable->internal);
    printf("unsigned int length = %u\n", hashtable->length);
}

void hashtable_print_items(HashTable* hashtable) {
    printf("{\n");
    for (unsigned int i = 0; i < hashtable->length; i++) {
        HashNode* node_ptr = hashtable->internal[i];
        while (node_ptr) {
            printf("    \"%s\" = %d (hash=%d)\n", node_ptr->key, (int)node_ptr->data, i);
            node_ptr = node_ptr->next;
        }
    }
    printf("}\n");
}

void hashtable_print_internal(HashTable* hashtable) {
    printf("{\n");
    for (unsigned int i = 0; i < hashtable->length; i++) {
        printf("    internal[%u] = %p\n", i, hashtable->internal[i]);
    }
    printf("}\n");
}

int main(int argc, char** argv) {
    char* keys[] = {
        "apple", "orange", "grapes", "car", "bus",
        "table", "chair", "laptop", "phone", "bottle",
        "camera", "pencil", "eraser", "paper", "notebook",
        "guitar", "violin", "drums", "flute", "trumpet",
        "ocean", "river", "mountain", "desert", "valley",
        "forest", "jungle", "island", "beach", "hill",
        "pizza", "burger", "pasta", "salad", "bread",
        "house", "apartment", "castle", "cottage", "tent",
        "train", "airplane", "bicycle", "motorcycle", "boat",
        "shoes", "socks", "shirt", "pants", "jacket"
    };

    HashTable hashtable = {};
    hashtable_create(&hashtable, 1);

    hashtable_print(&hashtable);

    for (int i = 0; i < 50; i++) {
        hashtable_set(&hashtable, keys[i], (void*)i);
    }

    hashtable_print_internal(&hashtable);
    hashtable_print_items(&hashtable);

    int result = hashtable_get(&hashtable, "apple", &item);
    printf("hashtable[\"apple\"] = %p (success=%s)\n", item, (result) ? "true" : "false");

    hashtable_delete(&hashtable, "apple");
    hashtable_print_items(&hashtable);

    result = hashtable_rehash(&hashtable, 100);
    printf("rehashing... (success=%s)\n", (result) ? "true" : "false");

    hashtable_print(&hashtable);
    hashtable_print_internal(&hashtable);
    hashtable_print_items(&hashtable);

    for (int i = 0; i < 50; i += 2) {
        hashtable_delete(&hashtable, keys[i]);
    }
    hashtable_print_items(&hashtable);

    hashtable_free(&hashtable);
    return 0;
}
