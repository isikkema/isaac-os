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
ListNode* list_find(List* list, void* data);
ListNode* list_insert(List* list, void* data);
bool list_remove(List* list, ListNode* node);
