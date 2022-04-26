#pragma once


#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <list.h>
#include <virtio.h>


#define EXT4_MAGIC              0xef53
#define EXT4_SUPERBLOCK_OFFSET  1024UL
#define EXT2_N_BLOCKS           15
#define EXT4_EXTENTS_FL         0x00080000
#define EXT3_EXTENT_MAGIC       0xf30a
#define EXT2_NAME_LEN           255
#define EXT2_LABEL_LEN          16
#define EXT4_DET_NAMELEN        0xde00

#define EXT2_BAD_INO             1 /* Bad blocks inode */
#define EXT2_ROOT_INO            2 /* Root inode */
#define EXT4_USR_QUOTA_INO       3 /* User quota inode */
#define EXT4_GRP_QUOTA_INO       4 /* Group quota inode */
#define EXT2_BOOT_LOADER_INO     5 /* Boot loader inode */
#define EXT2_UNDEL_DIR_INO       6 /* Undelete directory inode */
#define EXT2_RESIZE_INO          7 /* Reserved group descriptors inode */
#define EXT2_JOURNAL_INO         8 /* Journal inode */
#define EXT2_EXCLUDE_INO         9 /* The "exclude" inode, for snapshots */
#define EXT4_REPLICA_INO        10 /* Used by non-upstream feature */

#define EXT2_FT_UNKNOWN     0 /* 0b000 */
#define EXT2_FT_REG_FILE    1 /* 0b001 */
#define EXT2_FT_DIR         2 /* 0b010 */
#define EXT2_FT_CHRDEV      3 /* 0b011 */
#define EXT2_FT_BLKDEV      4 /* 0b100 */
#define EXT2_FT_FIFO        5 /* 0b101 */
#define EXT2_FT_SOCK        6 /* 0b110 */
#define EXT2_FT_SYMLINK     7 /* 0b111 */
#define EXT2_FT_MAX         8

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

#define EXT4_COMBINE_VAL16(hi, lo)  (((uint64_t) hi << 16) | lo)
#define EXT4_COMBINE_VAL32(hi, lo)  (((uint64_t) hi << 32) | lo)

#define EXT4_GET_BLOCKSIZE(sb)      (1024UL << sb.s_log_block_size)


typedef struct ext2_super_block {
/*000*/    uint32_t    s_inodes_count;        /* Inodes count */
    uint32_t    s_blocks_count;        /* Blocks count */
    uint32_t    s_r_blocks_count;    /* Reserved blocks count */
    uint32_t    s_free_blocks_count;    /* Free blocks count */
/*010*/    uint32_t    s_free_inodes_count;    /* Free inodes count */
    uint32_t    s_first_data_block;    /* First Data Block */
    uint32_t    s_log_block_size;    /* Block size */
    uint32_t    s_log_cluster_size;    /* Allocation cluster size */
/*020*/    uint32_t    s_blocks_per_group;    /* # Blocks per group */
    uint32_t    s_clusters_per_group;    /* # Fragments per group */
    uint32_t    s_inodes_per_group;    /* # Inodes per group */
    uint32_t    s_mtime;        /* Mount time */
/*030*/    uint32_t    s_wtime;        /* Write time */
    uint16_t    s_mnt_count;        /* Mount count */
    uint16_t    s_max_mnt_count;    /* Maximal mount count */
    uint16_t    s_magic;        /* Magic signature */
    uint16_t    s_state;        /* File system state */
    uint16_t    s_errors;        /* Behaviour when detecting errors */
    uint16_t    s_minor_rev_level;    /* minor revision level */
/*040*/    uint32_t    s_lastcheck;        /* time of last check */
    uint32_t    s_checkinterval;    /* max. time between checks */
    uint32_t    s_creator_os;        /* OS */
    uint32_t    s_rev_level;        /* Revision level */
/*050*/    uint16_t    s_def_resuid;        /* Default uid for reserved blocks */
    uint16_t    s_def_resgid;        /* Default gid for reserved blocks */
    /*
     * These fields are for EXT2_DYNAMIC_REV superblocks only.
     *
     * Note: the difference between the compatible feature set and
     * the incompatible feature set is that if there is a bit set
     * in the incompatible feature set that the kernel doesn't
     * know about, it should refuse to mount the filesystem.
     *
     * e2fsck's requirements are more strict; if it doesn't know
     * about a feature in either the compatible or incompatible
     * feature set, it must abort and not try to meddle with
     * things it doesn't understand...
     */
    uint32_t    s_first_ino;        /* First non-reserved inode */
    uint16_t   s_inode_size;        /* size of inode structure */
    uint16_t    s_block_group_nr;    /* block group # of this superblock */
    uint32_t    s_feature_compat;    /* compatible feature set */
/*060*/    uint32_t    s_feature_incompat;    /* incompatible feature set */
    uint32_t    s_feature_ro_compat;    /* readonly-compatible feature set */
/*068*/    uint8_t    s_uuid[16];        /* 128-bit uuid for volume */
/*078*/    uint8_t    s_volume_name[EXT2_LABEL_LEN];    /* volume name, no NUL? */
/*088*/    uint8_t    s_last_mounted[64];    /* directory last mounted on, no NUL? */
/*0c8*/    uint32_t    s_algorithm_usage_bitmap; /* For compression */
    /*
     * Performance hints.  Directory preallocation should only
     * happen if the EXT2_FEATURE_COMPAT_DIR_PREALLOC flag is on.
     */
    uint8_t    s_prealloc_blocks;    /* Nr of blocks to try to preallocate*/
    uint8_t    s_prealloc_dir_blocks;    /* Nr to preallocate for dirs */
    uint16_t    s_reserved_gdt_blocks;    /* Per group table for online growth */
    /*
     * Journaling support valid if EXT2_FEATURE_COMPAT_HAS_JOURNAL set.
     */
/*0d0*/    uint8_t    s_journal_uuid[16];    /* uuid of journal superblock */
/*0e0*/    uint32_t    s_journal_inum;        /* inode number of journal file */
    uint32_t    s_journal_dev;        /* device number of journal file */
    uint32_t    s_last_orphan;        /* start of list of inodes to delete */
/*0ec*/    uint32_t    s_hash_seed[4];        /* HTREE hash seed */
/*0fc*/    uint8_t    s_def_hash_version;    /* Default hash version to use */
    uint8_t    s_jnl_backup_type;    /* Default type of journal backup */
    uint16_t    s_desc_size;        /* Group desc. size: INCOMPAT_64BIT */
/*100*/    uint32_t    s_default_mount_opts;    /* default EXT2_MOUNT_* flags used */
    uint32_t    s_first_meta_bg;    /* First metablock group */
    uint32_t    s_mkfs_time;        /* When the filesystem was created */
/*10c*/    uint32_t    s_jnl_blocks[17];    /* Backup of the journal inode */
/*150*/    uint32_t    s_blocks_count_hi;    /* Blocks count high 32bits */
    uint32_t    s_r_blocks_count_hi;    /* Reserved blocks count high 32 bits*/
    uint32_t    s_free_blocks_hi;    /* Free blocks count */
    uint16_t    s_min_extra_isize;    /* All inodes have at least # bytes */
    uint16_t    s_want_extra_isize;    /* New inodes should reserve # bytes */
/*160*/    uint32_t    s_flags;        /* Miscellaneous flags */
    uint16_t    s_raid_stride;        /* RAID stride in blocks */
    uint16_t    s_mmp_update_interval;  /* # seconds to wait in MMP checking */
    uint64_t    s_mmp_block;        /* Block for multi-mount protection */
/*170*/    uint32_t    s_raid_stripe_width;    /* blocks on all data disks (N*stride)*/
    uint8_t    s_log_groups_per_flex;    /* FLEX_BG group size */
    uint8_t    s_checksum_type;    /* metadata checksum algorithm */
    uint8_t    s_encryption_level;    /* versioning level for encryption */
    uint8_t    s_reserved_pad;        /* Padding to next 32bits */
    uint64_t    s_kbytes_written;    /* nr of lifetime kilobytes written */
/*180*/    uint32_t    s_snapshot_inum;    /* Inode number of active snapshot */
    uint32_t    s_snapshot_id;        /* sequential ID of active snapshot */
    uint64_t    s_snapshot_r_blocks_count; /* active snapshot reserved blocks */
/*190*/    uint32_t    s_snapshot_list;    /* inode number of disk snapshot list */

    uint32_t    s_error_count;        /* number of fs errors */
    uint32_t    s_first_error_time;    /* first time an error happened */
    uint32_t    s_first_error_ino;    /* inode involved in first error */
/*1a0*/    uint64_t    s_first_error_block;    /* block involved in first error */
    uint8_t    s_first_error_func[32];    /* function where error hit, no NUL? */
/*1c8*/    uint32_t    s_first_error_line;    /* line number where error happened */
    uint32_t    s_last_error_time;    /* most recent time of an error */
/*1d0*/    uint32_t    s_last_error_ino;    /* inode involved in last error */
    uint32_t    s_last_error_line;    /* line number where error happened */
    uint64_t    s_last_error_block;    /* block involved of last error */
/*1e0*/    uint8_t    s_last_error_func[32];    /* function where error hit, no NUL? */

/*200*/    uint8_t    s_mount_opts[64];    /* default mount options, no NUL? */
/*240*/    uint32_t    s_usr_quota_inum;    /* inode number of user quota file */
    uint32_t    s_grp_quota_inum;    /* inode number of group quota file */
    uint32_t    s_overhead_clusters;    /* overhead blocks/clusters in fs */
/*24c*/    uint32_t    s_backup_bgs[2];    /* If sparse_super2 enabled */
/*254*/    uint8_t    s_encrypt_algos[4];    /* Encryption algorithms in use  */
/*258*/    uint8_t    s_encrypt_pw_salt[16];    /* Salt used for string2key algorithm */
/*268*/    uint32_t    s_lpf_ino;        /* Location of the lost+found inode */
    uint32_t  s_prj_quota_inum;    /* inode for tracking project quota */
/*270*/    uint32_t    s_checksum_seed;    /* crc32c(orig_uuid) if csum_seed set */
/*274*/    uint8_t    s_wtime_hi;
    uint8_t    s_mtime_hi;
    uint8_t    s_mkfs_time_hi;
    uint8_t    s_lastcheck_hi;
    uint8_t    s_first_error_time_hi;
    uint8_t    s_last_error_time_hi;
    uint8_t    s_first_error_errcode;
    uint8_t    s_last_error_errcode;
/*27c*/ uint16_t    s_encoding;        /* Filename charset encoding */
    uint16_t    s_encoding_flags;    /* Filename charset encoding flags */
    uint32_t    s_reserved[95];        /* Padding to the end of the block */
/*3fc*/    uint32_t    s_checksum;        /* crc32c(superblock) */
} Ext4SuperBlock;

typedef struct ext4_group_desc {
    uint32_t bg_block_bitmap;        /* Blocks bitmap block */
    uint32_t bg_inode_bitmap;      /* Inodes bitmap block */
    uint32_t bg_inode_table;         /* Inodes table block */
    uint16_t bg_free_blocks_count;      /* Free blocks count */
    uint16_t bg_free_inodes_count;      /* Free inodes count */
    uint16_t bg_used_dirs_count;     /* Directories count */
    uint16_t bg_flags;               /* EXT4_BG_flags (INODE_UNINIT, etc) */
    uint32_t bg_exclude_bitmap_lo;      /* Exclude bitmap for snapshots */
    uint16_t bg_block_bitmap_csum_lo;/* crc32c(s_uuid+grp_num+bitmap) LSB */
    uint16_t bg_inode_bitmap_csum_lo;/* crc32c(s_uuid+grp_num+bitmap) LSB */
    uint16_t bg_itable_unused;       /* Unused inodes count */
    uint16_t bg_checksum;            /* crc16(sb_uuid+group+desc) */
    uint32_t bg_block_bitmap_hi;     /* Blocks bitmap block MSB */
    uint32_t bg_inode_bitmap_hi;     /* Inodes bitmap block MSB */
    uint32_t bg_inode_table_hi;      /* Inodes table block MSB */
    uint16_t bg_free_blocks_count_hi;/* Free blocks count MSB */
    uint16_t bg_free_inodes_count_hi;/* Free inodes count MSB */
    uint16_t bg_used_dirs_count_hi;  /* Directories count MSB */
    uint16_t bg_itable_unused_hi;    /* Unused inodes count MSB */
    uint32_t bg_exclude_bitmap_hi;   /* Exclude bitmap block MSB */
    uint16_t bg_block_bitmap_csum_hi;/* crc32c(s_uuid+grp_num+bitmap) MSB */
    uint16_t bg_inode_bitmap_csum_hi;/* crc32c(s_uuid+grp_num+bitmap) MSB */
    uint32_t bg_reserved;
} Ext4GroupDesc;

typedef struct ext2_inode_large {
/*00*/    uint16_t    i_mode;        /* File mode */
    uint16_t    i_uid;        /* Low 16 bits of Owner Uid */
    uint32_t    i_size;        /* Size in bytes */
    uint32_t    i_atime;    /* Access time */
    uint32_t    i_ctime;    /* Inode Change time */
/*10*/    uint32_t    i_mtime;    /* Modification time */
    uint32_t    i_dtime;    /* Deletion Time */
    uint16_t    i_gid;        /* Low 16 bits of Group Id */
    uint16_t    i_links_count;    /* Links count */
    uint32_t    i_blocks;    /* Blocks count */
/*20*/    uint32_t    i_flags;    /* File flags */
    union {
        struct {
            uint32_t    l_i_version; /* was l_i_reserved1 */
        } linux1;
        struct {
            uint32_t  h_i_translator;
        } hurd1;
    } osd1;                /* OS dependent 1 */
/*28*/    uint32_t    i_block[EXT2_N_BLOCKS];/* Pointers to blocks */
/*64*/    uint32_t    i_generation;    /* File version (for NFS) */
    uint32_t    i_file_acl;    /* File ACL */
    uint32_t    i_size_high;
/*70*/    uint32_t    i_faddr;    /* Fragment address */
    union {
        struct {
            uint16_t    l_i_blocks_hi;
            uint16_t    l_i_file_acl_high;
            uint16_t    l_i_uid_high;    /* these 2 fields    */
            uint16_t    l_i_gid_high;    /* were reserved2[0] */
            uint16_t    l_i_checksum_lo; /* crc32c(uuid+inum+inode) */
            uint16_t    l_i_reserved;
        } linux2;
        struct {
            uint8_t    h_i_frag;    /* Fragment number */
            uint8_t    h_i_fsize;    /* Fragment size */
            uint16_t    h_i_mode_high;
            uint16_t    h_i_uid_high;
            uint16_t    h_i_gid_high;
            uint32_t    h_i_author;
        } hurd2;
    } osd2;                /* OS dependent 2 */
/*80*/    uint16_t    i_extra_isize;
    uint16_t    i_checksum_hi;    /* crc32c(uuid+inum+inode) */
    uint32_t    i_ctime_extra;    /* extra Change time (nsec << 2 | epoch) */
    uint32_t    i_mtime_extra;    /* extra Modification time (nsec << 2 | epoch) */
    uint32_t    i_atime_extra;    /* extra Access time (nsec << 2 | epoch) */
/*90*/    uint32_t    i_crtime;    /* File creation time */
    uint32_t    i_crtime_extra;    /* extra File creation time (nsec << 2 | epoch)*/
    uint32_t    i_version_hi;    /* high 32 bits for 64-bit version */
/*9c*/    uint32_t   i_projid;       /* Project ID */
} Ext4Inode;

typedef struct ext2_dir_entry {
   uint32_t  inode;                /* Offset 0, Inode number */
   uint16_t  rec_len;              /* Offset 4, Directory entry length */
   uint16_t  file_type_name_len;   /* Offset 6, file type and name length */
   char   name[EXT2_NAME_LEN];  /* Offset 8, File name */
} Ext4DirEntry;

typedef struct ext2_dir_entry_tail {
   uint32_t  det_reserved_zero1;    /* Pretend to be unused */
   uint16_t  det_rec_len;           /* 12 */
   uint16_t  det_reserved_name_len; /* 0xDE00, fake namelen/filetype */
   uint32_t  det_checksum;          /* crc32c(uuid+inode+dirent) */
} Ext4DirEntryTail;

typedef struct extent_header {
    uint16_t  eh_magic;      /* probably will support different formats */
    uint16_t  eh_entries;       /* number of valid entries */
    uint16_t  eh_max;        /* capacity of store in entries */
    uint16_t  eh_depth;      /* has tree real underlying blocks? */
    uint32_t  eh_generation; /* generation of the tree */
} Ext4ExtentHeader;

typedef struct extent_index {
    uint32_t  ei_block;   /* index covers logical blocks from 'block' */
    uint32_t  ei_leaf;    /* pointer to the physical block of the next */
    uint16_t  ei_leaf_hi;    /* high 16 bits of physical block */
    uint16_t  ei_unused;  /* Not used */
} Ext4ExtentIndex;

typedef struct extent {
    uint32_t  ee_block;    /* first logical block extent covers */
    uint16_t  ee_len;      /* number of blocks covered by extent */
    uint16_t  ee_start_hi; /* high 16 bits of physical block */
    uint32_t  ee_start;    /* low 32 bigs of physical block */
} Ext4Extent;

typedef struct Ext4CacheNode {
    Ext4Inode inode;
    Ext4DirEntry entry;
    List* children;
} Ext4CacheNode;


bool ext4_init(VirtioDevice* block_device);
bool ext4_cache_inodes(VirtioDevice* block_device);
Ext4CacheNode* ext4_get_file(VirtioDevice* block_device, char* path);
size_t ext4_read_extent(VirtioDevice* block_device, Ext4ExtentHeader* extent_header, void* buf, size_t filesize);
size_t ext4_read_file(VirtioDevice* block_device, char* path, void* buf, size_t count);
