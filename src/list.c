#include <list.h>
#include <kmalloc.h>


List* list_new() {
    return kzalloc(sizeof(List));
}

ListNode* list_find(List* list, void* data) {
    ListNode* it;

    for (it = list->head; it != NULL; it = it->next) {
        if (it->data == data) {
            return it;
        }
    }

    return NULL;
}

ListNode* list_insert(List* list, void* data) {
    ListNode* new_node;

    new_node = kmalloc(sizeof(ListNode));
    new_node->data = data;
    new_node->next = list->head;

    list->head = new_node;

    return new_node;
}

bool list_remove(List* list, ListNode* node) {
    ListNode* it;
    ListNode* prev;

    if (node == list->head) {
        list->head = list->head->next;
        kfree(node);
        return true;
    }

    for (it = list->head; it != NULL; it = it->next) {
        if (it == node) {
            prev->next = it->next;
            break;
        }

        prev = it;
    }

    if (it == NULL) {
        return false;
    }

    kfree(it);
    return true;
}
