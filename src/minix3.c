#include <minix3.h>
#include <block.h>
#include <printf.h>


SuperBlock sb;


bool minix3_init() {
    if (!block_read_poll(&sb, MINIX3_SUPERBLOCK_ADDR, sizeof(SuperBlock))) {
        printf("minix3_init: superblock read failed\n");
        return false;
    }

    printf(
        "minix3_init: num_inodes: %d, imap_blocks: %d, zmap_blocks: %d, first_data_zone: %d, log_zone_size: %d, max_size: %d, num_zones: %d, magic: 0x%x, block_size: %d, disk_version: %d\n",
        sb.num_inodes, sb.imap_blocks, sb.zmap_blocks,
        sb.first_data_zone, sb.log_zone_size,
        sb.max_size, sb.num_zones, sb.magic,
        sb.block_size, sb.disk_version
    );

    return true;
}