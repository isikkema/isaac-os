#include <vfs.h>
#include <filepath.h>
#include <kmalloc.h>
#include <string.h>
#include <minix3.h>
#include <ext4.h>
#include <printf.h>


VfsCacheNode* vfs_cnode_cache;


bool vfs_init() {
    vfs_cnode_cache = kzalloc(sizeof(VfsCacheNode));
    vfs_cnode_cache->children = list_new();
    vfs_cnode_cache->name = kmalloc(strlen("/"));
    memcpy(vfs_cnode_cache->name, "/", strlen("/"));

    return true;
}

VfsCacheNode* _vfs_get_cnode(char* path, bool create, char* path_left) {
    List* path_names;
    List* sub_path_names;
    char* tmp;
    char* name;
    ListNode* name_it;
    ListNode* cnode_it;
    VfsCacheNode* current_cnode;
    VfsCacheNode* tmp_cnode;
    bool found_flag;

    path_names = filepath_split_path(path);

    if (strcmp(path_names->head->data, "/") != 0) {
        printf("vfs_mount: filepath must be absolute (%s)\n", path);

        list_free_data(path_names);
        list_free(path_names);

        return NULL;
    }

    name = path_names->head->data;
    list_remove(path_names, name);
    kfree(name);

    current_cnode = vfs_cnode_cache;
    for (name_it = path_names->head; name_it != NULL; name_it = name_it->next) {
        found_flag = false;
        name = name_it->data;

        for (cnode_it = current_cnode->children->head; cnode_it != NULL; cnode_it = cnode_it->next) {
            tmp_cnode = cnode_it->data;
            if (strcmp(tmp_cnode->name, name) == 0) {
                current_cnode = tmp_cnode;
                found_flag = true;
                break;
            }
        }

        if (!found_flag) {
            if (create) {
                tmp_cnode = kzalloc(sizeof(VfsCacheNode));
                tmp_cnode->children = list_new();
                tmp_cnode->name = kmalloc(strlen(name));
                memcpy(tmp_cnode->name, name, strlen(name));

                list_insert(current_cnode->children, tmp_cnode);
                current_cnode = tmp_cnode;

                // printf("_vfs_get_cnode: Added cnode for \"%s\"\n", name);
            } else {
                // printf("_vfs_get_cnode: no cnode named (%s) found in path (%s)\n", name, path);

                sub_path_names = list_new();
                sub_path_names->head = name_it;
                sub_path_names->last = path_names->last;

                // printf("_vfs_get_cnode: name_it: (%s)\n", name_it->data);
                tmp = filepath_join_paths(sub_path_names);
                // printf("_vfs_get_cnode: left: (%s)\n", tmp);
                memcpy(path_left, tmp, strlen(tmp));

                list_free_data(path_names);
                list_free(path_names);

                sub_path_names->head = NULL;
                sub_path_names->last = NULL;
                kfree(sub_path_names);

                return current_cnode;
            }
        }
    }

    list_free_data(path_names);
    list_free(path_names);

    return current_cnode;
}

VfsCacheNode* vfs_mount(VirtioDevice* block_device, char* path) {
    VfsCacheNode* cnode;

    cnode = _vfs_get_cnode(path, true, NULL);
    if (cnode == NULL) {
        printf("vfs_mount: _vfs_get_cnode failed\n");
        return NULL;
    }

    if (cnode->type != NT_NONE) {
        printf("vfs_mount: %s is already mounted\n", path);
        return NULL;
    }

    if (!minix3_init(block_device)) {
        printf("vfs_mount: minix3_init failed\n");
        return NULL;
    }

    // todo: figure out correct type
    cnode->type = 1;
    cnode->block_device = block_device;
    cnode->node = minix3_get_file(block_device, "/");

    return cnode;
}

VfsCacheNode* vfs_get_mount(char* path, char* path_left) {
    return _vfs_get_cnode(path, false, path_left);
}

size_t vfs_read_file(char* path, void* buf, size_t count) {
    VfsCacheNode* cnode;
    char* path_left;
    size_t num_read;

    path_left = kzalloc(strlen(path) + 2);
    path_left[0] = '/';
    cnode = vfs_get_mount(path, path_left + 1);
    if (cnode == NULL) {
        kfree(path_left);
        return -1UL;
    }

    switch (cnode->type) {
        case NT_MINIX3:
            num_read = minix3_read_file(cnode->block_device, path_left, buf, count);
            break;
        
        case NT_EXT4:
            num_read = ext4_read_file(path_left, buf, count);
            break;
        
        default:
            printf("vfs_read_file: unsupported type: %d\n", cnode->type);
            num_read = -1UL;
            break;
    }

    kfree(path_left);
    return num_read;
}
