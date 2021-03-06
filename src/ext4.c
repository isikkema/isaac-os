#include <ext4.h>
#include <block.h>
#include <kmalloc.h>
#include <list.h>
#include <map.h>
#include <string.h>
#include <filepath.h>
#include <rs_int.h>
#include <printf.h>


#define GET_BLOCK_ADDR(block, sb)   (                   \
    (void*) (                                       \
        EXT4_SUPERBLOCK_OFFSET +                    \
        (block - 1) * EXT4_GET_BLOCKSIZE(sb)   \
    )                                               \
)

#define GET_INODE_ADDR(inum, sb, groups)    (                                           \
    GET_BLOCK_ADDR(                                                         \
        EXT4_COMBINE_VAL32(                                                 \
            groups[(inum - 1) / sb.s_inodes_per_group]            \
                .bg_inode_table_hi,                                         \
            groups[(inum - 1) / sb.s_inodes_per_group]            \
                .bg_inode_table                                             \
        ),                                                                   \
        sb                                                                   \
    ) +                                                                     \
    (u64) ((inum - 1) % sb.s_inodes_per_group) * sb.s_inode_size  \
)


Map* ext4_superblocks;
Map* ext4_groups_map;
Map* ext4_inode_caches;


bool ext4_init(VirtioDevice* block_device) {
    Ext4SuperBlock* ext4_sb;
    Ext4GroupDesc* ext4_groups;
    u32 num_groups;

    ext4_sb = kmalloc(sizeof(Ext4SuperBlock));

    if (!block_read_poll(block_device, ext4_sb, (void*) EXT4_SUPERBLOCK_OFFSET, sizeof(Ext4SuperBlock))) {
        printf("ext4_init: superblock read failed\n");

        kfree(ext4_sb);
        return false;
    }

    if (ext4_sb->s_magic != EXT4_MAGIC) {
        kfree(ext4_sb);
        return false;
    }

    num_groups = 
        (EXT4_COMBINE_VAL32(ext4_sb->s_blocks_count_hi, ext4_sb->s_blocks_count) - 1) /
        ext4_sb->s_blocks_per_group +
        1;

    ext4_groups = kmalloc(sizeof(Ext4GroupDesc) * num_groups);
    if (!block_read_poll(block_device, ext4_groups, (void*) EXT4_SUPERBLOCK_OFFSET + sizeof(Ext4SuperBlock), sizeof(Ext4GroupDesc) * num_groups)) {
        printf("ext4_init: groups read failed\n");
        
        kfree(ext4_groups);
        kfree(ext4_sb);
        return false;
    }

    ext4_superblocks = map_new();
    ext4_groups_map = map_new();
    ext4_inode_caches = map_new();

    map_insert(ext4_superblocks, (u64) block_device, ext4_sb);
    map_insert(ext4_groups_map, (u64) block_device, ext4_groups);

    if (!ext4_cache_inodes(block_device)) {
        printf("ext4_cache_inodes failed\n");

        kfree(ext4_sb);
        kfree(ext4_groups);
        map_free(ext4_superblocks);
        map_free(ext4_groups_map);
        map_free(ext4_inode_caches);

        return false;
    }

    return true;
}

bool ext4_cache_cnode(VirtioDevice* block_device, List* nodes_to_cache, Map* inum_to_inode, Ext4CacheNode* cnode) {
    Ext4ExtentHeader* extent_header;
    size_t num_read;
    size_t size;
    void* buf;
    Ext4DirEntry* dir_entry;
    Ext4DirEntryTail* dir_tail;
    Ext4Inode* inode_ptr;
    Ext4Inode inode;
    Ext4CacheNode* child_cnode;
    bool cached_flag;
    Ext4SuperBlock* sb_ptr;
    Ext4SuperBlock ext4_sb;
    Ext4GroupDesc* ext4_groups;

    if (!S_ISDIR(cnode->inode.i_mode)) {
        return false;
    }

    if (!(cnode->inode.i_flags & EXT4_EXTENTS_FL)) {
        printf("ext4_cache_cnode: extents must be enabled\n");
        return false;
    }

    extent_header = (Ext4ExtentHeader*) cnode->inode.i_block;

    if (extent_header->eh_magic != EXT3_EXTENT_MAGIC) {
        printf("ext4_cache_cnode: magic 0x%04x != 0x%04x\n", extent_header->eh_magic, EXT3_EXTENT_MAGIC);
        return false;
    }

    sb_ptr = map_get(ext4_superblocks, (u64) block_device);
    if (sb_ptr == NULL) {
        printf("ext4_cache_cnode: no superblock for block device 0x%08lx\n", (u64) block_device);
        return false;
    }

    ext4_sb = *sb_ptr;

    ext4_groups = map_get(ext4_groups_map, (u64) block_device);
    if (ext4_groups == NULL) {
        printf("ext4_cache_cnode: no groups for block device 0x%08lx\n", (u64) block_device);
        return false;
    }

    size = EXT4_COMBINE_VAL32(cnode->inode.i_size_high, cnode->inode.i_size);
    buf = kmalloc(size);

    // Read file into buf
    num_read = ext4_read_extent(block_device, extent_header, buf, size);
    if (num_read != size) {
        printf("ext4_cache_cnode: ext4_read_extent failed: num_read: %ld\n", num_read);
        
        kfree(buf);
        return false;
    }

    // For each DirEntry in file
    for (dir_entry = buf; (void*) dir_entry + dir_entry->rec_len <= buf + size - sizeof(Ext4DirEntryTail); dir_entry = (void*) dir_entry + dir_entry->rec_len) {
        if (dir_entry->inode == 0) {
            continue;
        }

        // Check if we've handled this inode before (if this is a hard link)
        inode_ptr = map_get(inum_to_inode, dir_entry->inode);
        if (inode_ptr == NULL) {
            // If not, read the inode from the disk
            cached_flag = false;

            inode_ptr = &inode;
            if (!block_read_poll(block_device, inode_ptr, GET_INODE_ADDR(dir_entry->inode, ext4_sb, ext4_groups), sizeof(Ext4Inode))) {
                printf("ext4_cache_cnode: inode read failed\n");
                return false;
            }
        } else {
            // If so, use the cached inode
            cached_flag = true;
        }

        // New cache node
        child_cnode = kzalloc(sizeof(Ext4CacheNode));
        child_cnode->inode = *inode_ptr;
        child_cnode->entry.inode = dir_entry->inode;
        memcpy(child_cnode->entry.name, dir_entry->name, EXT2_NAME_LEN);
        child_cnode->children = list_new();

        // Add new child_cnode to cnode's children list
        list_insert(cnode->children, child_cnode);

        // If we've already handled this inode before, we've already cached its children
        if (!cached_flag) {
            list_insert(nodes_to_cache, child_cnode);
            map_insert(inum_to_inode, dir_entry->inode, &child_cnode->inode);
        }
    }

    dir_tail = (Ext4DirEntryTail*) dir_entry;
    if (dir_tail->det_reserved_name_len != EXT4_DET_NAMELEN) {
        printf("ext4_cache_cnode: dir entry tail name len 0x%04x != 0x%04x\n", dir_tail->det_reserved_name_len, EXT4_DET_NAMELEN);
        
        kfree(buf);
        return false;
    }

    kfree(buf);
    return true;
}

bool ext4_cache_inodes(VirtioDevice* block_device) {
    Ext4Inode inode;
    Ext4CacheNode* cnode;
    List* nodes_to_cache;
    Map* inum_to_inode;
    Ext4SuperBlock* sb_ptr;
    Ext4SuperBlock ext4_sb;
    Ext4GroupDesc* ext4_groups;

    sb_ptr = map_get(ext4_superblocks, (u64) block_device);
    if (sb_ptr == NULL) {
        printf("ext4_cache_inodes: no superblock for block device 0x%08lx\n", (u64) block_device);
        return false;
    }

    ext4_sb = *sb_ptr;

    ext4_groups = map_get(ext4_groups_map, (u64) block_device);
    if (ext4_groups == NULL) {
        printf("ext4_cache_inodes: no groups for block device 0x%08lx\n", (u64) block_device);
        return false;
    }

    // Read root inode from disk
    if (!block_read_poll(block_device, &inode, GET_INODE_ADDR(EXT2_ROOT_INO, ext4_sb, ext4_groups), sizeof(Ext4Inode))) {
        printf("ext4_cache_inodes: root inode read failed\n");
        return false;
    }

    // Use a list to tell us what we need to cache next.
    // This way, we don't need to use recursion.
    nodes_to_cache = list_new();

    // Use a map to determine if we've already cached an inode before
    inum_to_inode = map_new();

    // Create root cnode
    cnode = kzalloc(sizeof(Ext4CacheNode));
    cnode->inode = inode;
    cnode->entry.inode = EXT2_ROOT_INO;
    memcpy(cnode->entry.name, "/", strlen("/"));
    cnode->children = list_new();

    map_insert(ext4_inode_caches, (u64) block_device, cnode);

    // Add root to data structures
    list_insert(nodes_to_cache, cnode);
    map_insert(inum_to_inode, EXT2_ROOT_INO, &cnode->inode);

    // Cache everything while there are things to cache
    while (nodes_to_cache->head != NULL) {
        cnode = nodes_to_cache->head->data;
        list_remove(nodes_to_cache, cnode);

        ext4_cache_cnode(block_device, nodes_to_cache, inum_to_inode, cnode);
    }

    list_free(nodes_to_cache);
    map_free(inum_to_inode);

    return true;
}

Ext4CacheNode* ext4_get_file(VirtioDevice* block_device, char* path) {
    List* path_names;
    char* name;
    ListNode* name_it;
    ListNode* cnode_it;
    Ext4CacheNode* current_cnode;
    Ext4CacheNode* tmp_cnode;
    bool found_flag;

    current_cnode = map_get(ext4_inode_caches, (u64) block_device);
    if (current_cnode == NULL) {
        printf("ext4_get_file: no root cnode for block device: 0x%08lx\n", (u64) block_device);
        return NULL;
    }

    path_names = filepath_split_path(path);
    
    if (strcmp(path_names->head->data, "/") != 0) {
        printf("ext4_get_file: filepath must be absolute (%s)\n", path);

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
            printf("ext4_get_file: no file named (%s) found in path (%s)\n", name, path);

            list_free_data(path_names);
            list_free(path_names);

            return NULL;
        }
    }

    list_free_data(path_names);
    list_free(path_names);

    return current_cnode;
}

size_t ext4_read_extent(VirtioDevice* block_device, Ext4ExtentHeader* extent_header, void* buf, size_t filesize) {
    Ext4Extent* extent;
    Ext4ExtentIndex* extent_index;
    size_t count;
    size_t offset;
    size_t num_read;
    size_t total_read;
    Ext4ExtentHeader* block;
    void* block_addr;
    u32 i;
    Ext4SuperBlock* sb_ptr;
    Ext4SuperBlock ext4_sb;

    sb_ptr = map_get(ext4_superblocks, (u64) block_device);
    if (sb_ptr == NULL) {
        printf("ext4_read_extent: no superblock for block device: 0x%08lx\n", (u64) block_device);
        return -1UL;
    }

    ext4_sb = *sb_ptr;

    total_read = 0;

    // If leaf, just read and return
    if (extent_header->eh_depth == 0) {
        for (i = 0; i < extent_header->eh_entries; i++) {
            extent = (void*) extent_header + sizeof(Ext4ExtentHeader) + i * sizeof(Ext4Extent);

            block_addr = GET_BLOCK_ADDR(EXT4_COMBINE_VAL32(extent->ee_start_hi, extent->ee_start), ext4_sb);

            offset = extent->ee_block * EXT4_GET_BLOCKSIZE(ext4_sb);
            count = extent->ee_len * EXT4_GET_BLOCKSIZE(ext4_sb);
            if (offset + count > filesize) {
                count = filesize - offset;
            }

            if (!block_read_poll(block_device, buf + offset, block_addr, count)) {
                printf("ext4_read_extent: extent leaf read failed\n");
                return -1UL;
            }

            total_read += count;
        }

        return total_read;
    }

    count = EXT4_GET_BLOCKSIZE(ext4_sb);
    block = kmalloc(count);
    for (i = 0; i < extent_header->eh_entries; i++) {
        extent_index = (void*) extent_header + sizeof(Ext4ExtentHeader) + i * sizeof(Ext4ExtentIndex);

        block_addr = GET_BLOCK_ADDR(EXT4_COMBINE_VAL32(extent_index->ei_leaf_hi, extent_index->ei_leaf), ext4_sb);

        if (!block_read_poll(block_device, block, block_addr, count)) {
            printf("ext4_read_extent: extent index read failed\n");

            kfree(block);
            return -1UL;
        }

        if (block->eh_depth >= extent_header->eh_depth) {
            printf("ext4_read_extent: child depth >= parent depth: %d >= %d\n", block->eh_depth, extent_header->eh_depth);
            
            kfree(block);
            return -1UL;
        }

        num_read = ext4_read_extent(block_device, block, buf, filesize);
        if (num_read == -1UL) {
            kfree(block);
            return -1UL;
        }

        total_read += num_read;
    }

    kfree(block);
    return total_read;
}

size_t ext4_read_file(VirtioDevice* block_device, char* path, void* buf, size_t count) {
    Ext4CacheNode* cnode;
    Ext4ExtentHeader* extent_header;
    size_t filesize;

    cnode = ext4_get_file(block_device, path);
    if (cnode == NULL) {
        return 0;
    }

    if (!(cnode->inode.i_flags & EXT4_EXTENTS_FL)) {
        printf("ext4_read_file: extents must be enabled\n");
        return 0;
    }

    filesize = EXT4_COMBINE_VAL32(cnode->inode.i_size_high, cnode->inode.i_size);
    if (count > filesize || count == 0) {
        count = filesize;
    }

    extent_header = (Ext4ExtentHeader*) cnode->inode.i_block;

    return ext4_read_extent(block_device, extent_header, buf, count);
}

size_t ext4_get_filesize(VirtioDevice* block_device, char* path) {
    Ext4CacheNode* cnode;

    cnode = ext4_get_file(block_device, path);
    if (cnode == NULL) {
        return -1UL;
    }

    return EXT4_COMBINE_VAL32(cnode->inode.i_size_high, cnode->inode.i_size);
}
