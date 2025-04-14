#include "list.h"

#include <stdio.h>
#include <stdlib.h>

void listCreate(List* list) {
    // just make sure that the list is empty
    list->data = 0;
    list->length = 0;
}

void listDelete(List* list) {
    // iterate over each item
    ListItem* item = list->data;

    // if item is 0, it is the last item of the list
    while (item) {
        // we need to save the pointer to next, so it isn't lost when item is freed
        ListItem* next = item->next;
        free(item);
        item = next;
    }
    // clear the list so it can't be accidentally used
    list->data = 0;
    list->length = 0;
}

void listFreeAllData(List* list) {
    ListItem* item = list->data;
    while (item) {
        // free each data ptr, and set it to 0 to avoid accidental usage
        free(item->data);
        item->data = 0;
        item = item->next;
    }
}

exception listAddItem(List* list, void* data) {
    // iterator variables
    // also get a pointer to next pointer so we can overwrite it
    ListItem* item = list->data;
    ListItem** item_ptr = &list->data;

    // item can be 0, this signals an empty list that we cannot iterate over
    if (item) {
        // when next is 0, this is the last item
        while (item->next) {
            item = item->next;
        }
        // update overwriting pointer
        item_ptr = &item->next;
    }

    // overwrite pointer with new item
    *item_ptr = (ListItem*)malloc(sizeof(ListItem));
    if (!*item_ptr) tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for list.");

    // fill new item with data
    (*item_ptr)->data = data;

    // next is 0, signalling no next item (this is now the last item)
    (*item_ptr)->next = 0;

    // increment size of list
    list->length++;
    return SUCCESS;
}

exception listInsertItem(List* list, const uint index, void* data) {
    // obviously we cant insert an item outside of the list
    if (index > list->length) tekThrow(LIST_EXCEPTION, "List index out of range.");

    // create some iterator variables
    ListItem* item = list->data;

    // at the start, the next item is the first item
    ListItem* next = list->data;

    // keep a pointer to the pointer to next so we can overwrite it
    ListItem** item_ptr = &list->data;

    // if index is 0, then we are inserting at the start
    // this means changing the root item of the list
    // no need to run this loop, as it will change 'next' incorrectly to the 1st item rather than 0th item
    if (index) {
        // counter variable
        uint count = 0;
        while (item->next) {
            // if we have reached the specified index, then stop looping
            if (++count >= index) break;
            item = item->next;
        }

        // update pointers
        item_ptr = &item->next;
        next = item->next;
    }

    // write new list item
    *item_ptr = (ListItem*)malloc(sizeof(ListItem));
    if (!*item_ptr) tekThrow(MEMORY_EXCEPTION, "Failed to allocate memory for list.");

    // fill item with data, using 'next' we stored
    (*item_ptr)->data = data;
    (*item_ptr)->next = next;

    // increment length
    list->length++;
    return SUCCESS;
}

exception listGetItem(const List* list, const uint index, void** data) {
    // make sure we are getting something that exists in the list
    if (index > list->length) tekThrow(LIST_EXCEPTION, "List index out of range.");

    // iterator variables
    ListItem* item = list->data;
    uint count = 0;
    while (item) {
        // if we have counted up to the index, stop
        if (count++ >= index) break;
        item = item->next;
    }

    // return data
    if (data) *data = item->data;
    return SUCCESS;
}

exception listPopItem(List* list, void** data) {
    // track current and previous item in loop
    ListItem* prev = 0;
    ListItem* item = list->data;

    // it item is 0, then there's nothing to pop
    if (!item) tekThrow(LIST_EXCEPTION, "List is empty.");
    while (item) {
        // loop until there is no next item
        if (!item->next) break;

        // keep track of previous item before moving on
        prev = item;
        item = item->next;
    }

    // return the original data before we free it
    if (data) *data = item->data;
    free(item);

    // if there is not a previous item, then we have just emptied the list completely
    // hence the two possibilities - editing one item or the actual list
    if (prev) {
        prev->next = 0;
    } else {
        list->data = 0;
    }

    // decrement length
    list->length--;
    return SUCCESS;
}

exception listRemoveItem(List* list, const uint index, void** data) {
    // track previous and current item
    ListItem* prev = 0;
    ListItem* item = list->data;

    // make sure that the list is not empty, and we are removing an existing item
    if (!item) tekThrow(LIST_EXCEPTION, "List is empty.");
    if (index >= list->length) tekThrow(LIST_EXCEPTION, "List index out of range.");

    // iterate over list
    uint count = 0;
    while (item) {
        // if we have hit the index to remove, stop looping
        if (count++ >= index) break;

        // update iterator variables
        prev = item;
        item = item->next;
    }

    // return data before we free it
    if (data) *data = item->data;

    // pass the next pointer to the previous item, so the list remains contiguous
    // could be either the previous item, or the root of the list
    if (prev) {
        prev->next = item->next;
    } else {
        list->data = item->next;
    }

    // free item, as it has been removed
    free(item);

    // decrement length
    list->length--;
    return SUCCESS;
}

void listPrint(const List* list) {
    // iterate over list
    ListItem* item = list->data;
    uint count = 0;
    printf("[");
    while (item) {
        // if not the first item, we need commas to separate items
        if (count) printf(", ");

        // print the item
        printf("%p", item->data);
        item = item->next;
        count++;
    }
    printf("]\n");
}