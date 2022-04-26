#pragma once


#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <list.h>
#include <virtio.h>


#define MINIX3_MAGIC                0x4d5a
#define MINIX3_SUPERBLOCK_OFFSET    1024UL
#define MINIX3_ROOT_INODE           1
#define MINIX3_ZONES_PER_INODE      10
#define MINIX3_NAME_SIZE            60

#define S_IFMT      0170000
#define S_IFSOCK    0140000
#define S_IFLNK     0120000
#define S_IFREG     0100000
#define S_IFBLK     0060000
#define S_IFDIR     0040000
#define S_IFCHR     0020000
#define S_IFIFO     0010000
#define S_ISUID     0004000
#define S_ISGID     0002000
#define S_ISVTX     0001000

#define S_ISLNK(m)  (((m) & S_IFMT) == S_IFLNK)
#define S_ISREG(m)  (((m) & S_IFMT) == S_IFREG)
#define S_ISDIR(m)  (((m) & S_IFMT) == S_IFDIR)
#define S_ISCHR(m)  (((m) & S_IFMT) == S_IFCHR)
#define S_ISBLK(m)  (((m) & S_IFMT) == S_IFBLK)
#define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)
#define S_ISSOCK(m) (((m) & S_IFMT) == S_IFSOCK)

#define S_IRWXU 00700
#define S_IRUSR 00400
#define S_IWUSR 00200
#define S_IXUSR 00100

#define S_IRWXG 00070
#define S_IRGRP 00040
#define S_IWGRP 00020
#define S_IXGRP 00010

#define S_IRWXO 00007
#define S_IROTH 00004
#define S_IWOTH 00002
#define S_IXOTH 00001


typedef struct Minix3SuperBlock {
    uint32_t num_inodes;
    uint16_t pad0;
    uint16_t imap_blocks;
    uint16_t zmap_blocks;
    uint16_t first_data_zone;
    uint16_t log_zone_size;
    uint16_t pad1;
    uint32_t max_size;
    uint32_t num_zones;
    uint16_t magic;
    uint16_t pad2;
    uint16_t block_size;
    uint8_t disk_version;
} Minix3SuperBlock;

typedef struct Minix3Inode {
    uint16_t mode;
    uint16_t nlinks;
    uint16_t uid;
    uint16_t gid;
    uint32_t size;
    uint32_t atime;
    uint32_t mtime;
    uint32_t ctime;
    uint32_t zones[10];
} Minix3Inode;

typedef struct Minix3DirEntry {
    uint32_t inode;
    char name[60];
} Minix3DirEntry;

typedef struct Minix3CacheNode {
    Minix3Inode inode;
    Minix3DirEntry entry;
    List* children;
} Minix3CacheNode;

typedef enum Minix3ZoneType {
    Z_DIRECT = 0,
    Z_SINGLY_INDIRECT = 1,
    Z_DOUBLY_INDIRECT = 2,
    Z_TRIPLY_INDIRECT = 3
} Minix3ZoneType;


bool minix3_init(VirtioDevice* block_device);
bool minix3_cache_inodes(VirtioDevice* block_device);
Minix3CacheNode* minix3_get_file(VirtioDevice* block_device, char* path);
size_t minix3_read_zone(VirtioDevice* block_device, uint32_t zone, Minix3ZoneType type, void* buf, size_t count);
size_t minix3_read_file(VirtioDevice* block_device, char* path, void* buf, size_t count);
