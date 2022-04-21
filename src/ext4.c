#include <ext4.h>
#include <block.h>
#include <kmalloc.h>
#include <list.h>
#include <map.h>
#include <rs_int.h>
#include <printf.h>


#define GET_BLOCK_ADDR(block)   (                   \
    (void*) (                                       \
        EXT4_SUPERBLOCK_OFFSET +                    \
        (block - 1) * EXT4_GET_BLOCKSIZE(ext4_sb)   \
    )                                               \
)

// #define GET_INODE_ADDR(inum)    (
//     (void*) (
//         inum / ext4_sb.s_inodes_per_group
//     )
// )


Ext4SuperBlock ext4_sb;
Ext4GroupDesc* ext4_groups;


bool ext4_init() {
    u32 num_groups;

    if (!block_read_poll(&ext4_sb, (void*) EXT4_SUPERBLOCK_OFFSET, sizeof(Ext4SuperBlock))) {
        printf("ext4_init: superblock read failed\n");
        return false;
    }

    if (ext4_sb.s_magic != EXT4_MAGIC) {
        printf("ext4_init: magic 0x%04x != 0x%04x\n", ext4_sb.s_magic, EXT4_MAGIC);
        return false;
    }

    num_groups = 
        (EXT4_COMBINE_VAL32(ext4_sb.s_blocks_count_hi, ext4_sb.s_blocks_count) - 1) /
        ext4_sb.s_blocks_per_group +
        1;

    printf("ext4_init: blocks_count_lo: %d, blocks_count_hi: %d, blocks_per_group: %d\n",
        ext4_sb.s_blocks_count, ext4_sb.s_blocks_count_hi, ext4_sb.s_blocks_per_group
    );
    printf("ext4_init: num_groups: %d\n", num_groups);

    ext4_groups = kmalloc(sizeof(Ext4GroupDesc) * num_groups);
    if (!block_read_poll(ext4_groups, (void*) EXT4_SUPERBLOCK_OFFSET + sizeof(Ext4SuperBlock), sizeof(Ext4GroupDesc) * num_groups)) {
        printf("ext4_init: groups read failed\n");
        
        kfree(ext4_groups);
        ext4_groups = NULL;
        return false;
    }

    return true;
}

bool ext4_cache_cnode(List* nodes_to_cache, Map* inum_to_inode, Ext4CacheNode* cnode) {
    u32 i;
    Ext4ExtentHeader* extent_header;
    Ext4ExtentIndex* extent_idx;
    Ext4Extent* extent;

    if (!EXT4_IS_DIR(cnode->inode.i_mode)) {
        return false;
    }


    if (!(cnode->inode.i_flags & EXT4_EXTENTS_FL)) {
        printf("ext4_cache_cnode: extents must be enabled\n");
        return false;
    }

    // block = kzalloc(EXT4_GET_BLOCKSIZE(ext4_sb));

    extent_header = (Ext4ExtentHeader*) cnode->inode.i_block;

    printf("ext4_cache_cnode: inum: %d, eh: magic: 0x%04x, entries: %d, max: %d, depth: %d, generation: %d\n",
        cnode->entry.inode, extent_header->eh_magic,
        extent_header->eh_entries, extent_header->eh_max,
        extent_header->eh_depth, extent_header->eh_generation
    );

    for (i = 0; i < extent_header->eh_entries; i++) {

        // // Read DirEntries into block
        // num_read = ext4_read_zone(zone, Z_DIRECT, block, minix3_sb.block_size);
        // if (num_read != minix3_sb.block_size) {
        //     printf("minix3_init: zone %d read failed\n", zone);
        //     kfree(block);
        //     return false;
        // }

        // // For each DirEntry in block
        // for (dir_entry = block; (void*) (dir_entry + 1) <= block + minix3_sb.block_size; dir_entry++) {
        //     if (dir_entry->inode == 0) {
        //         continue;
        //     }

        //     // Check if we've handled this inode before (if this is a hard link)
        //     inode_ptr = map_get(inum_to_inode, dir_entry->inode);
        //     if (inode_ptr == NULL) {
        //         // If not, read the inode from the disk
        //         cached_flag = false;

        //         inode_ptr = &inode;
        //         if (!block_read_poll(inode_ptr, GET_INODE_ADDR(dir_entry->inode), sizeof(Minix3Inode))) {
        //             printf("minix3_init: root inode read failed\n");
        //             return false;
        //         }
        //     } else {
        //         // If so, use the cached inode
        //         cached_flag = true;
        //     }

        //     // New cache node
        //     child_cnode = kzalloc(sizeof(Minix3CacheNode));
        //     child_cnode->inode = *inode_ptr;
        //     child_cnode->entry.inode = dir_entry->inode;
        //     memcpy(child_cnode->entry.name, dir_entry->name, MINIX3_NAME_SIZE);
        //     child_cnode->children = list_new();

        //     printf("minix3_cache_cnode: inum: %4d, size: %d, name: %s\n", dir_entry->inode, child_cnode->inode.size, dir_entry->name);

        //     // Add new child_cnode to cnode's children list
        //     list_insert(cnode->children, child_cnode);

        //     // If we've already handled this inode before, we've already cached its children
        //     if (!cached_flag) {
        //         list_insert(nodes_to_cache, child_cnode);
        //         map_insert(inum_to_inode, dir_entry->inode, &child_cnode->inode);
        //     }
        // }
    }

    // kfree(block);
    return true;
}

// bool minix3_cache_inodes() {
//     Ext4Inode inode;
//     Ext4CacheNode* cnode;
//     List* nodes_to_cache;
//     Map* inum_to_inode;

//     // Read root inode from disk
//     if (!block_read_poll(&inode, GET_BLOCK_ADDR(EXT2_ROOT_INO), sizeof(Ext4Inode))) {
//         printf("minix3_init: root inode read failed\n");
//         return false;
//     }

//     // Use a list to tell us what we need to cache next.
//     // This way, we don't need to use recursion.
//     nodes_to_cache = list_new();

//     // Use a map to determine if we've already cached an inode before
//     inum_to_inode = map_new();

//     // Create root cnode
//     cnode = kzalloc(sizeof(Minix3CacheNode));
//     cnode->inode = inode;
//     cnode->entry.inode = MINIX3_ROOT_INODE;
//     memcpy(cnode->entry.name, "/", strlen("/"));
//     cnode->children = list_new();

//     minix3_inode_cache = cnode;

//     // Add root to data structures
//     list_insert(nodes_to_cache, cnode);
//     map_insert(inum_to_inode, MINIX3_ROOT_INODE, &cnode->inode);

//     // Cache everything while there are things to cache
//     while (nodes_to_cache->head != NULL) {
//         cnode = nodes_to_cache->head->data;
//         list_remove(nodes_to_cache, cnode);

//         minix3_cache_cnode(nodes_to_cache, inum_to_inode, cnode);
//     }

//     list_free(nodes_to_cache);
//     map_free(inum_to_inode);

//     return true;
// }

// size_t minix3_read_zone(uint32_t zone, Minix3ZoneType type, void* buf, size_t count) {
//     size_t num_read;
//     size_t total_read;
//     void* zone_addr;
//     u32* block;
//     size_t max_count;
//     u32 i;

//     if (zone == 0 || type < Z_DIRECT || type > Z_TRIPLY_INDIRECT) {
//         return -1UL;
//     }

//     if (count == 0) {
//         return 0;
//     }

//     max_count = 0;
//     switch (type) {
//         case Z_DIRECT:
//             max_count = (u64) minix3_sb.block_size;
//             break;
        
//         case Z_SINGLY_INDIRECT:
//             max_count = (u64) minix3_sb.block_size * minix3_sb.block_size;
//             break;

//         case Z_DOUBLY_INDIRECT:
//             max_count = (u64) minix3_sb.block_size * minix3_sb.block_size * minix3_sb.block_size;
//             break;

//         case Z_TRIPLY_INDIRECT:
//             max_count = (u64) minix3_sb.block_size * minix3_sb.block_size * minix3_sb.block_size * minix3_sb.block_size;
//             break;
//     }

//     if (count > max_count) {
//         count = max_count;
//     }

//     zone_addr = GET_ZONE_ADDR(zone);

//     // If direct, just read and return
//     if (type == Z_DIRECT) {
//         if (!block_read_poll(buf, zone_addr, count)) {
//             return -1UL;
//         }

//         return count;
//     }

//     // Otherwise, recursively read zones

//     // Read one full block of zone pointers
//     block = kmalloc(minix3_sb.block_size);
//     if (!block_read_poll(block, zone_addr, minix3_sb.block_size)) {
//         kfree(block);
//         return -1UL;
//     }

//     total_read = 0;
//     for (i = 0; i < minix3_sb.block_size / sizeof(u32); i++) {
//         // Skip unused zone pointers
//         if (block[i] == 0) {
//             continue;
//         }

//         num_read = minix3_read_zone(block[i], type - 1, buf + total_read, count - total_read);
//         if (num_read == -1UL) {
//             kfree(block);
//             return -1UL;
//         }

//         total_read += num_read;
        
//         if (total_read == count) {
//             break;
//         }
//     }

//     kfree(block);
//     return total_read;
// }

// size_t minix3_read_file(char* path, void* buf, size_t count) {
//     Minix3CacheNode* cnode;
//     u32 zone;
//     Minix3ZoneType type;
//     size_t zone_count;
//     size_t num_read;
//     size_t total_read;
//     u32 i;

//     if (count == 0) {
//         return 0;
//     }

//     cnode = minix3_get_file(path);
//     if (cnode == NULL) {
//         return 0;
//     }

//     if (count > cnode->inode.size) {
//         count = cnode->inode.size;
//     }

//     total_read = 0;
//     for (i = 0; i < MINIX3_ZONES_PER_INODE; i++) {
//         zone = cnode->inode.zones[i];
//         if (zone == 0) {
//             continue;
//         }

//         if (i < 7) {
//             type = Z_DIRECT;
//         } else if (i == 7) {
//             type = Z_SINGLY_INDIRECT;
//         } else if (i == 8) {
//             type = Z_DOUBLY_INDIRECT;
//         } else if (i == 9) {
//             type = Z_TRIPLY_INDIRECT;
//         }

//         zone_count = 0;
//         switch (type) {
//             case Z_DIRECT:
//                 zone_count = (u64) minix3_sb.block_size;
//                 break;
            
//             case Z_SINGLY_INDIRECT:
//                 zone_count = (u64) minix3_sb.block_size * minix3_sb.block_size;
//                 break;

//             case Z_DOUBLY_INDIRECT:
//                 zone_count = (u64) minix3_sb.block_size * minix3_sb.block_size * minix3_sb.block_size;
//                 break;

//             case Z_TRIPLY_INDIRECT:
//                 zone_count = (u64) minix3_sb.block_size * minix3_sb.block_size * minix3_sb.block_size * minix3_sb.block_size;
//                 break;
//         }

//         if (count - total_read < zone_count) {
//             zone_count = count - total_read;
//         }

//         num_read = minix3_read_zone(zone, type, buf + total_read, zone_count);
//         if (num_read == -1UL) {
//             return -1UL;
//         }

//         total_read += num_read;

//         if (total_read == count) {
//             break;
//         }
//     }

//     return total_read;
// }
