#include <minix3.h>
#include <block.h>
#include <kmalloc.h>
#include <list.h>
#include <map.h>
#include <string.h>
#include <rs_int.h>
#include <printf.h>


SuperBlock sb;
Minix3CacheNode* minix3_inode_cache;


#define GET_INODE_ADDR(inum) (                  \
    (Inode*) (                                  \
        MINIX3_SUPERBLOCK_OFFSET +              \
        (u64) sb.block_size +                   \
        (u64) sb.imap_blocks * sb.block_size +  \
        (u64) sb.zmap_blocks * sb.block_size +  \
        (inum - 1) * sizeof(Inode)              \
    )                                           \
)

#define GET_ZONE_ADDR(zone) (               \
    (void*) ((u64) zone * sb.block_size)    \
)


bool minix3_init() {
    if (!block_read_poll(&sb, (void*) MINIX3_SUPERBLOCK_OFFSET, sizeof(SuperBlock))) {
        printf("minix3_init: superblock read failed\n");
        return false;
    }

    if (sb.magic != MINIX3_MAGIC) {
        printf("minix3_init: invalid magic: 0x%04x != 0x%04x\n", sb.magic, MINIX3_MAGIC);
        return false;
    }

    if (!minix3_cache_inodes()) {
        printf("minix3_init: minix3_cache_inodes failed\n");
        return false;
    }

    return true;
}

bool minix3_cache_cnode(List* nodes_to_cache, Map* inum_to_inode, Minix3CacheNode* cnode) {
    Minix3CacheNode* child_cnode;
    Inode inode;
    Inode* inode_ptr;
    u64 zone;
    void* block;
    DirEntry* dir_entry;
    bool cached_flag;
    u32 i;

    if (!S_ISDIR(cnode->inode.mode)) {
        return false;
    }

    block = kzalloc(sb.block_size);

    // todo: handle indirect zones

    for (i = 0; i < MINIX3_ZONES_PER_INODE - 3; i++) {
        zone = cnode->inode.zones[i];
        if (zone == 0) {
            continue;
        }

        // Read DirEntries into block
        if (!block_read_poll(block, GET_ZONE_ADDR(zone), sb.block_size)) {
            printf("minix3_init: zone %d read failed\n", zone);
            kfree(block);
            return false;
        }

        // For each DirEntry in block
        for (dir_entry = block; (void*) (dir_entry + 1) <= block + sb.block_size; dir_entry++) {
            if (dir_entry->inode == 0) {
                continue;
            }

            // Check if we've handled this inode before (if this is a hard link)
            inode_ptr = map_get(inum_to_inode, dir_entry->inode);
            if (inode_ptr == NULL) {
                // If not, read the inode from the disk
                cached_flag = false;

                inode_ptr = &inode;
                if (!block_read_poll(inode_ptr, GET_INODE_ADDR(dir_entry->inode), sizeof(Inode))) {
                    printf("minix3_init: root inode read failed\n");
                    return false;
                }
            } else {
                // If so, use the cached inode
                cached_flag = true;
            }

            // New cache node
            child_cnode = kzalloc(sizeof(Minix3CacheNode));
            child_cnode->inode = *inode_ptr;
            child_cnode->entry.inode = dir_entry->inode;
            memcpy(child_cnode->entry.name, dir_entry->name, MINIX3_NAME_SIZE);
            child_cnode->children = list_new();

            printf("minix3_cache_cnode: inum: %4d, name: %s\n", dir_entry->inode, dir_entry->name);

            // Add new child_cnode to cnode's children list
            list_insert(cnode->children, child_cnode);

            // If we've already handled this inode before, we've already cached its children
            if (!cached_flag) {
                list_insert(nodes_to_cache, child_cnode);
                map_insert(inum_to_inode, dir_entry->inode, &child_cnode->inode);
            }
        }
    }

    kfree(block);
    return true;
}

bool minix3_cache_inodes() {
    Inode inode;
    Minix3CacheNode* cnode;
    List* nodes_to_cache;
    Map* inum_to_inode;

    // Read root inode from disk
    if (!block_read_poll(&inode, GET_INODE_ADDR(MINIX3_ROOT_INODE), sizeof(Inode))) {
        printf("minix3_init: root inode read failed\n");
        return false;
    }

    // Use a list to tell us what we need to cache next.
    // This way, we don't need to use recursion.
    nodes_to_cache = list_new();

    // Use a map to determine if we've already cached an inode before
    inum_to_inode = map_new();

    // Create root cnode
    cnode = kzalloc(sizeof(Minix3CacheNode));
    cnode->inode = inode;
    cnode->entry.inode = MINIX3_ROOT_INODE;
    memcpy(cnode->entry.name, "/", strlen("/"));
    cnode->children = list_new();

    minix3_inode_cache = cnode;

    // Add root to data structures
    list_insert(nodes_to_cache, cnode);
    map_insert(inum_to_inode, MINIX3_ROOT_INODE, &cnode->inode);

    // Cache everything while there are things to cache
    while (nodes_to_cache->head != NULL) {
        cnode = nodes_to_cache->head->data;
        list_remove(nodes_to_cache, cnode);

        minix3_cache_cnode(nodes_to_cache, inum_to_inode, cnode);
    }

    list_free(nodes_to_cache);
    map_free(inum_to_inode);

    return true;
}

char* minix3_deescape_name(char* name) {
    char* deescaped_name;
    char* tmp;
    char c;
    bool escape;
    u32 idx;
    u32 new_idx;

    tmp = kzalloc(strlen(name) + 1);
    new_idx = 0;
    escape = false;
    for (idx = 0; idx < (u32) strlen(name); idx++) {
        c = name[idx];

        if (c == '\\' && !escape) {
            escape = true;
            continue;
        } else {
            escape = false;
        }

        tmp[new_idx] = c;

        new_idx++;
    }

    deescaped_name = kzalloc(strlen(tmp) + 1);
    memcpy(deescaped_name, tmp, strlen(tmp));

    kfree(tmp);
    return deescaped_name;
}

List* minix3_split_path(char* path) {
    List* list;
    ListNode* it;
    char* chunk;
    u32 start_idx;
    u32 end_idx;
    bool escape;
    char pc;
    char c;
    u32 i;

    list = list_new();

    start_idx = 0;
    end_idx = strlen(path);
    escape = false;
    pc = '\0';
    for (i = 0; i < (u32) strlen(path); i++) {
        c = path[i];

        if (c == '/') {
            if (escape) {
                // treat as regular character
                escape = false;
            } else {
                if (pc == '/') {
                    // Many '/'s in a row. Ignore all but the first.
                    start_idx++;
                } else if (pc == '\0') {
                    // First character is '/'. This is an absolute path. Add a "/" as the first name.
                    chunk = kzalloc(2);
                    memcpy(chunk, "/", 1);

                    list_insert_after(list, list->last, chunk);

                    start_idx = i + 1;
                    end_idx = i + 1;
                } else {
                    // Copy from beginning or last '/' through here into list.
                    end_idx = i;

                    chunk = kzalloc(end_idx - start_idx + 1);
                    memcpy(chunk, path + start_idx, end_idx - start_idx);

                    list_insert_after(list, list->last, chunk);

                    start_idx = i + 1;
                    end_idx = i + 1;
                }
            }
        } else if (c == '\\') {
            if (escape) {
                // treat as regular character
                escape = false;
            } else {
                escape = true;
            }
        } else {
            if (escape) {
                printf("minix3_path_split: warning: cannot escape character (%c)\n", c);
                escape = false;
            } else {
                // Regular character. Do nothing
            }
        }

        pc = c;
    }

    end_idx = i;
    if (start_idx != end_idx) {
        chunk = kzalloc(end_idx - start_idx + 1);
        memcpy(chunk, path + start_idx, end_idx - start_idx);

        list_insert_after(list, list->last, chunk);
    }

    for (it = list->head; it != NULL; it = it->next) {
        chunk = it->data;
        it->data = minix3_deescape_name(chunk);
        kfree(chunk);
    }

    return list;
}

Minix3CacheNode* minix3_get_file(char* path) {
    List* path_names;
    char* name;
    ListNode* name_it;
    ListNode* cnode_it;
    Minix3CacheNode* current_cnode;
    Minix3CacheNode* tmp_cnode;
    bool found_flag;

    path_names = minix3_split_path(path);
    
    if (strcmp(path_names->head->data, "/") != 0) {
        printf("minix3_get_file: filepath must be absolute (%s)\n", path);

        list_free_data(path_names);
        list_free(path_names);

        return NULL;
    }

    name = path_names->head->data;
    list_remove(path_names, name);
    kfree(name);

    current_cnode = minix3_inode_cache;
    for (name_it = path_names->head; name_it != NULL; name_it = name_it->next) {
        found_flag = false;
        name = name_it->data;

        for (cnode_it = current_cnode->children->head; cnode_it != NULL; cnode_it = cnode_it->next) {
            tmp_cnode = cnode_it->data;
            if (strcmp(tmp_cnode->entry.name, name_it->data) == 0) {
                current_cnode = tmp_cnode;
                found_flag = true;
                break;
            }
        }

        if (!found_flag) {
            printf("minix3_get_file: no file named (%s) found in path (%s)\n", name, path);

            list_free_data(path_names);
            list_free(path_names);

            return NULL;
        }
    }

    list_free_data(path_names);
    list_free(path_names);

    return current_cnode;
}
