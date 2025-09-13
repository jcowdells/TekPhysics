#pragma once

#include "exception.h"
#include "../tekgl.h"

#define foreach(item, list, run) \
item = list->data; \
while (item) { \
    run; \
    item = item->next; \
} \

typedef struct ListItem {
    void* data;
    struct ListItem* next;
} ListItem;

typedef struct List {
    ListItem* data;
    uint length;
} List;

void listCreate(List* list);
void listDelete(List* list);
void listFreeAllData(const List* list);
exception listAddItem(List* list, void* data);
exception listInsertItem(List* list, uint index, void* data);
exception listGetItem(const List* list, uint index, void** data);
exception listPopItem(List* list, void** data);
exception listRemoveItem(List* list, uint index, void** data);
exception listMoveItem(List* list, uint old_index, uint new_index);
void listPrint(const List* list);
