#include <list.h>
#include <kmalloc.h>


List* list_new() {
    return kzalloc(sizeof(List));
}

void list_free(List* list) {
    ListNode* it;
    ListNode* nit;

    it = list->head;
    while (it != NULL) {
        nit = it->next;

        kfree(it);

        it = nit;
    }

    kfree(list);
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

ListNode* list_insert_after(ListNode* node, void* data) {
    ListNode* new_node;

    new_node = kmalloc(sizeof(ListNode));
    new_node->data = data;
    new_node->next = node->next;

    node->next = new_node;

    return new_node;
}

bool list_remove(List* list, void* data) {
    ListNode* it;
    ListNode* prev;

    it = NULL;
    if (list->head != NULL && list->head->data == data) {
        it = list->head;
        list->head = list->head->next;
        kfree(it);
        return true;
    }

    for (it = list->head; it != NULL; it = it->next) {
        if (it->data == data) {
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
