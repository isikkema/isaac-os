#include <map.h>
#include <kmalloc.h>
#include <list.h>
#include <rs_int.h>
#include <printf.h>


Map* map_new() {
    return kzalloc(sizeof(Map));
}

void map_free(Map* map) {
    MapNode* mnode;
    List* nodes_to_free;

    mnode = map->head;
    
    nodes_to_free = list_new();
    list_insert(nodes_to_free, mnode);

    while (nodes_to_free->head != NULL) {
        mnode = nodes_to_free->head->data;
        if (!list_remove(nodes_to_free, mnode)) {
            printf("map_free: warning: could not remove mnode from nodes_to_free list: 0x%08lx\n", (u64) mnode);
        }

        if (mnode == NULL) {
            continue;
        }

        list_insert(nodes_to_free, mnode->left);
        list_insert(nodes_to_free, mnode->right);

        kfree(mnode);
    }

    kfree(map);
}

void* map_get(Map* map, uint64_t key) {
    MapNode* mnode;

    mnode = map->head;

    while (mnode != NULL) {
        if (key == mnode->key) {
            return mnode->value;
        }

        if (key < mnode->key) {
            mnode = mnode->left;
        } else {
            mnode = mnode->right;
        }
    }

    return NULL;
}

bool map_insert(Map* map, uint64_t key, void* value) {
    MapNode* mnode;

    if (map->head == NULL) {
        mnode = kzalloc(sizeof(MapNode));
        mnode->key = key;
        mnode->value = value;

        map->head = mnode;
        return false;
    }

    mnode = map->head;
    while (true) {
        if (key == mnode->key) {
            mnode->value = value;
            return true;
        }

        if (key < mnode->key) {
            if (mnode->left == NULL) {
                mnode->left = kzalloc(sizeof(MapNode));
                mnode->left->key = key;
                mnode->left->value = value;

                return false;
            }

            mnode = mnode->left;
        } else {
            if (mnode->right == NULL) {
                mnode->right = kzalloc(sizeof(MapNode));
                mnode->right->key = key;
                mnode->right->value = value;

                return false;
            }

            mnode = mnode->right;
        }
    }

    // unreachable
}


void map_print(Map* map) {
    MapNode* mnode;
    List* nodes_to_print;

    mnode = map->head;
    
    nodes_to_print = list_new();
    list_insert(nodes_to_print, mnode);

    printf("map_print: root: 0x%08lx\n", (u64) mnode);

    while (nodes_to_print->head != NULL) {
        mnode = nodes_to_print->head->data;
        if (!list_remove(nodes_to_print, mnode)) {
            printf("map_print: warning: could not remove mnode from nodes_to_print list: 0x%08lx\n", (u64) mnode);
        }

        if (mnode == NULL) {
            continue;
        }

        list_insert(nodes_to_print, mnode->left);
        list_insert(nodes_to_print, mnode->right);

        printf(
            "map_print: 0x%08lx: left: 0x%08lx, right: 0x%08lx, key: %4d, value: 0x%08lx\n",
            (u64) mnode, (u64) mnode->left, (u64) mnode->right,
            mnode->key, (u64) mnode->value
        );
    }
}
