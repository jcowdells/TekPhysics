#pragma once

#include "exception.h"
#include "../tekgl.h"

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
exception listAddItem(List* list, void* data);
exception listInsertItem(List* list, uint index, void* data);
exception listGetItem(const List* list, uint index, void** data);
exception listPopItem(List* list, void** data);
exception listRemoveItem(List* list, uint index, void** data);
void listPrint(const List* list);