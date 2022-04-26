#include <minix3.h>
#include <block.h>
#include <kmalloc.h>
#include <list.h>
#include <map.h>
#include <string.h>
#include <filepath.h>
#include <rs_int.h>
#include <printf.h>


Map* minix3_superblocks;
Map* minix3_inode_caches;


#define GET_INODE_ADDR(inum, sb) (                  \
    (Minix3Inode*) (                                  \
        MINIX3_SUPERBLOCK_OFFSET +              \
        (u64) sb.block_size +                   \
        (u64) sb.imap_blocks * sb.block_size +  \
        (u64) sb.zmap_blocks * sb.block_size +  \
        (inum - 1) * sizeof(Minix3Inode)              \
    )                                           \
)

#define GET_ZONE_ADDR(zone, sb) (               \
    (void*) ((u64) zone * sb.block_size)    \
)


bool minix3_init(VirtioDevice* block_device) {
    Minix3SuperBlock* sb;

    sb = kmalloc(sizeof(Minix3SuperBlock));

    if (!block_read_poll(block_device, sb, (void*) MINIX3_SUPERBLOCK_OFFSET, sizeof(Minix3SuperBlock))) {
        printf("minix3_init: superblock read failed\n");
        return false;
    }

    if (sb->magic != MINIX3_MAGIC) {
        printf("minix3_init: invalid magic: 0x%04x != 0x%04x\n", sb->magic, MINIX3_MAGIC);
        return false;
    }

    minix3_superblocks = map_new();
    minix3_inode_caches = map_new();

    map_insert(minix3_superblocks, (u64) block_device, sb);

    if (!minix3_cache_inodes(block_device)) {
        printf("minix3_init: minix3_cache_inodes failed\n");
        return false;
    }

    return true;
}

bool minix3_cache_cnode(VirtioDevice* block_device, List* nodes_to_cache, Map* inum_to_inode, Minix3CacheNode* cnode) {
    Minix3SuperBlock* sb_ptr;
    Minix3SuperBlock minix3_sb;
    Minix3CacheNode* child_cnode;
    Minix3Inode inode;
    Minix3Inode* inode_ptr;
    u64 zone;
    void* block;
    Minix3DirEntry* dir_entry;
    bool cached_flag;
    size_t num_read;
    u32 i;

    if (!S_ISDIR(cnode->inode.mode)) {
        return false;
    }

    sb_ptr = map_get(minix3_superblocks, (u64) block_device);
    if (sb_ptr == NULL) {
        printf("minix3_cache_cnode: no superblock for block device: 0x%08lx\n", (u64) block_device);
        return false;
    }

    minix3_sb = *sb_ptr;

    block = kzalloc(minix3_sb.block_size);

    for (i = 0; i < MINIX3_ZONES_PER_INODE; i++) {
        zone = cnode->inode.zones[i];
        if (zone == 0) {
            continue;
        }

        // Read DirEntries into block
        num_read = minix3_read_zone(block_device, zone, Z_DIRECT, block, minix3_sb.block_size);
        if (num_read != minix3_sb.block_size) {
            printf("minix3_init: zone %d read failed\n", zone);
            kfree(block);
            return false;
        }

        // For each DirEntry in block
        for (dir_entry = block; (void*) (dir_entry + 1) <= block + minix3_sb.block_size; dir_entry++) {
            if (dir_entry->inode == 0) {
                continue;
            }

            // Check if we've handled this inode before (if this is a hard link)
            inode_ptr = map_get(inum_to_inode, dir_entry->inode);
            if (inode_ptr == NULL) {
                // If not, read the inode from the disk
                cached_flag = false;

                inode_ptr = &inode;
                if (!block_read_poll(block_device, inode_ptr, GET_INODE_ADDR(dir_entry->inode, minix3_sb), sizeof(Minix3Inode))) {
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

bool minix3_cache_inodes(VirtioDevice* block_device) {
    Minix3SuperBlock* sb_ptr;
    Minix3SuperBlock minix3_sb;
    Minix3Inode inode;
    Minix3CacheNode* cnode;
    List* nodes_to_cache;
    Map* inum_to_inode;

    sb_ptr = map_get(minix3_superblocks, (u64) block_device);
    if (sb_ptr == NULL) {
        printf("minix3_cache_cnode: no superblock for block device: 0x%08lx\n", (u64) block_device);
        return false;
    }

    minix3_sb = *sb_ptr;

    // Read root inode from disk
    if (!block_read_poll(block_device, &inode, GET_INODE_ADDR(MINIX3_ROOT_INODE, minix3_sb), sizeof(Minix3Inode))) {
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

    map_insert(minix3_inode_caches, (u64) block_device, cnode);

    // Add root to data structures
    list_insert(nodes_to_cache, cnode);
    map_insert(inum_to_inode, MINIX3_ROOT_INODE, &cnode->inode);

    // Cache everything while there are things to cache
    while (nodes_to_cache->head != NULL) {
        cnode = nodes_to_cache->head->data;
        list_remove(nodes_to_cache, cnode);

        minix3_cache_cnode(block_device, nodes_to_cache, inum_to_inode, cnode);
    }

    list_free(nodes_to_cache);
    map_free(inum_to_inode);

    return true;
}

Minix3CacheNode* minix3_get_file(VirtioDevice* block_device, char* path) {
    List* path_names;
    char* name;
    ListNode* name_it;
    ListNode* cnode_it;
    Minix3CacheNode* current_cnode;
    Minix3CacheNode* tmp_cnode;
    bool found_flag;

    current_cnode = map_get(minix3_inode_caches, (u64) block_device);
    if (current_cnode == NULL) {
        printf("minix3_get_file: no root cnode for block device: 0x%08lx\n", (u64) block_device);
        return NULL;
    }

    path_names = filepath_split_path(path);
    
    if (strcmp(path_names->head->data, "/") != 0) {
        printf("minix3_get_file: filepath must be absolute (%s)\n", path);

        list_free_data(path_names);
        list_free(path_names);

        return NULL;
    }

    name = path_names->head->data;
    list_remove(path_names, name);
    kfree(name);

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

size_t minix3_read_zone(VirtioDevice* block_device, uint32_t zone, Minix3ZoneType type, void* buf, size_t count) {
    Minix3SuperBlock* sb_ptr;
    Minix3SuperBlock minix3_sb;
    size_t num_read;
    size_t total_read;
    void* zone_addr;
    u32* block;
    size_t max_count;
    u32 i;

    if (zone == 0 || type < Z_DIRECT || type > Z_TRIPLY_INDIRECT) {
        return -1UL;
    }

    if (count == 0) {
        return 0;
    }

    sb_ptr = map_get(minix3_superblocks, (u64) block_device);
    if (sb_ptr == NULL) {
        printf("minix3_read_zone: no superblock for block device: 0x%08lx\n", (u64) block_device);
        return -1UL;
    }

    minix3_sb = *sb_ptr;

    max_count = 0;
    switch (type) {
        case Z_DIRECT:
            max_count = (u64) minix3_sb.block_size;
            break;
        
        case Z_SINGLY_INDIRECT:
            max_count = (u64) minix3_sb.block_size * minix3_sb.block_size;
            break;

        case Z_DOUBLY_INDIRECT:
            max_count = (u64) minix3_sb.block_size * minix3_sb.block_size * minix3_sb.block_size;
            break;

        case Z_TRIPLY_INDIRECT:
            max_count = (u64) minix3_sb.block_size * minix3_sb.block_size * minix3_sb.block_size * minix3_sb.block_size;
            break;
    }

    if (count > max_count) {
        count = max_count;
    }

    zone_addr = GET_ZONE_ADDR(zone, minix3_sb);

    // If direct, just read and return
    if (type == Z_DIRECT) {
        if (!block_read_poll(block_device, buf, zone_addr, count)) {
            return -1UL;
        }

        return count;
    }

    // Otherwise, recursively read zones

    // Read one full block of zone pointers
    block = kmalloc(minix3_sb.block_size);
    if (!block_read_poll(block_device, block, zone_addr, minix3_sb.block_size)) {
        kfree(block);
        return -1UL;
    }

    total_read = 0;
    for (i = 0; i < minix3_sb.block_size / sizeof(u32); i++) {
        // Skip unused zone pointers
        if (block[i] == 0) {
            continue;
        }

        num_read = minix3_read_zone(block_device, block[i], type - 1, buf + total_read, count - total_read);
        if (num_read == -1UL) {
            kfree(block);
            return -1UL;
        }

        total_read += num_read;
        
        if (total_read == count) {
            break;
        }
    }

    kfree(block);
    return total_read;
}

size_t minix3_read_file(VirtioDevice* block_device, char* path, void* buf, size_t count) {
    Minix3SuperBlock* sb_ptr;
    Minix3SuperBlock minix3_sb;
    Minix3CacheNode* cnode;
    u32 zone;
    Minix3ZoneType type;
    size_t zone_count;
    size_t num_read;
    size_t total_read;
    u32 i;

    if (count == 0) {
        return 0;
    }

    sb_ptr = map_get(minix3_superblocks, (u64) block_device);
    if (sb_ptr == NULL) {
        printf("minix3_read_zone: no superblock for block device: 0x%08lx\n", (u64) block_device);
        return -1UL;
    }

    minix3_sb = *sb_ptr;

    cnode = minix3_get_file(block_device, path);
    if (cnode == NULL) {
        return 0;
    }

    if (count > cnode->inode.size) {
        count = cnode->inode.size;
    }

    total_read = 0;
    for (i = 0; i < MINIX3_ZONES_PER_INODE; i++) {
        zone = cnode->inode.zones[i];
        if (zone == 0) {
            continue;
        }

        if (i < 7) {
            type = Z_DIRECT;
        } else if (i == 7) {
            type = Z_SINGLY_INDIRECT;
        } else if (i == 8) {
            type = Z_DOUBLY_INDIRECT;
        } else if (i == 9) {
            type = Z_TRIPLY_INDIRECT;
        }

        zone_count = 0;
        switch (type) {
            case Z_DIRECT:
                zone_count = (u64) minix3_sb.block_size;
                break;
            
            case Z_SINGLY_INDIRECT:
                zone_count = (u64) minix3_sb.block_size * minix3_sb.block_size;
                break;

            case Z_DOUBLY_INDIRECT:
                zone_count = (u64) minix3_sb.block_size * minix3_sb.block_size * minix3_sb.block_size;
                break;

            case Z_TRIPLY_INDIRECT:
                zone_count = (u64) minix3_sb.block_size * minix3_sb.block_size * minix3_sb.block_size * minix3_sb.block_size;
                break;
        }

        if (count - total_read < zone_count) {
            zone_count = count - total_read;
        }

        num_read = minix3_read_zone(block_device, zone, type, buf + total_read, zone_count);
        if (num_read == -1UL) {
            return -1UL;
        }

        total_read += num_read;

        if (total_read == count) {
            break;
        }
    }

    return total_read;
}
