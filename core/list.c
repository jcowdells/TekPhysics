#include "list.h"

#include <stdio.h>
#include <stdlib.h>

void listCreate(List* list) {
    list->data = 0;
    list->length = 0;
}

void listDelete(List* list) {
    ListItem* item = list->data;
    while (item) {
        ListItem* next = item->next;
        free(item);
        item = next;
    }
    list->data = 0;
    list->length = 0;
}

exception listAddItem(List* list, void* data) {
    ListItem* item = list->data;
    ListItem** item_ptr = &list->data;
    if (item) {
        while (item->next) {
            item = item->next;
        }
        item_ptr = &item->next;
    }
    *item_ptr = (ListItem*)malloc(sizeof(ListItem));
    if (!*item_ptr) tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for list.");
    (*item_ptr)->data = data;
    (*item_ptr)->next = 0;
    list->length++;
    return SUCCESS;
}

exception listInsertItem(List* list, const uint index, void* data) {
    if (index > list->length) tekThrow(MEMORY_EXCEPTION, "List index out of range.");
    if (index == list->length) return listAddItem(list, data);

    ListItem* item = list->data;
    ListItem* next = list->data;
    ListItem** item_ptr = &list->data;
    if (index) {
        uint count = 0;
        while (item->next) {
            if (++count >= index) break;
            item = item->next;
        }
        item_ptr = &item->next;
        next = item->next;
    }

    *item_ptr = (ListItem*)malloc(sizeof(ListItem));
    if (!*item_ptr) tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for list.");
    (*item_ptr)->data = data;
    (*item_ptr)->next = next;
    list->length++;
    return SUCCESS;
}

exception listGetItem(const List* list, const uint index, void** data) {
    if (index > list->length) tekThrow(MEMORY_EXCEPTION, "List index out of range.");

    ListItem* item = list->data;
    uint count = 0;
    while (item) {
        if (count++ >= index) break;
        item = item->next;
    }

    *data = item->data;

    return SUCCESS;
}

exception listPopItem(List* list, void** data) {
    ListItem* prev = 0;
    ListItem* item = list->data;
    if (!item) tekThrow(MEMORY_EXCEPTION, "List is empty.");
    while (item) {
        if (!item->next) break;
        prev = item;
        item = item->next;
    }

    *data = item->data;
    free(item);
    if (prev) {
        prev->next = 0;
    } else {
        list->data = 0;
    }
    list->length--;
    return SUCCESS;
}

exception listRemoveItem(List* list, const uint index, void** data) {
    ListItem* prev = 0;
    ListItem* item = list->data;
    if (!item) tekThrow(MEMORY_EXCEPTION, "List is empty.");
    if (index >= list->length) tekThrow(MEMORY_EXCEPTION, "List index out of range.");

    uint count = 0;
    while (item) {
        if (count++ >= index) break;
        prev = item;
        item = item->next;
    }

    *data = item->data;
    if (prev) {
        prev->next = item->next;
    } else {
        list->data = item->next;
    }
    free(item);
    list->length--;
    return SUCCESS;
}

void listPrint(const List* list) {
    ListItem* item = list->data;
    uint count = 0;
    printf("[");
    while (item) {
        if (count) printf(", ");
        printf("%p", item->data);
        item = item->next;
        count++;
    }
    printf("]\n");
}