#pragma once


#include <stdint.h>
#include <stdbool.h>
#include <list.h>


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


typedef struct SuperBlock {
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
} SuperBlock;

typedef struct Inode {
    uint16_t mode;
    uint16_t nlinks;
    uint16_t uid;
    uint16_t gid;
    uint32_t size;
    uint32_t atime;
    uint32_t mtime;
    uint32_t ctime;
    uint32_t zones[10];
} Inode;

typedef struct DirEntry {
    uint32_t inode;
    char name[60];
} DirEntry;

typedef struct Minix3CacheNode {
    Inode inode;
    DirEntry entry;
    List* children;
    bool visited;
} Minix3CacheNode;


bool minix3_init();
bool minix3_cache_inodes();
