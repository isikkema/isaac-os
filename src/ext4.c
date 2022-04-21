#include <ext4.h>
#include <block.h>
#include <printf.h>


Ext4SuperBlock ext4_sb;


bool ext4_init() {
    if (!block_read_poll(&ext4_sb, (void*) EXT4_SUPERBLOCK_OFFSET, sizeof(Ext4SuperBlock))) {
        printf("ext4_init: superblock read failed\n");
        return false;
    }

    printf("ext4_init: sb: inodes_count: %d, blocks_count: %d, log_block_size: %d, magic: 0x%04x\n",
        ext4_sb.s_inodes_count, ext4_sb.s_blocks_count, ext4_sb.s_log_block_size, ext4_sb.s_magic
    );

    return true;
}
