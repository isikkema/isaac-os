#pragma once


#include <stdbool.h>


typedef struct ListNode {
    void* data;
    struct ListNode* next;
} ListNode;

typedef struct List {
    ListNode* head;
} List;


List* list_new();
void list_free(List* list);
ListNode* list_find(List* list, void* data);
ListNode* list_insert(List* list, void* data);
ListNode* list_insert_after(ListNode* node, void* data);
bool list_remove(List* list, void* data);
