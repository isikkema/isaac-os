#pragma once


#include <list.h>
#include <virtio.h>
#include <stddef.h>


typedef enum VfsCacheNodeType {
    NT_NONE = 0,
    NT_MINIX3,
    NT_EXT4
} VfsCacheNodeType;

typedef struct VfsCacheNode {
    void* node;
    char* name;
    List* children;
    VirtioDevice* block_device;
    VfsCacheNodeType type;
} VfsCacheNode;


bool vfs_init();
VfsCacheNode* vfs_mount(VirtioDevice* block_device, char* path);
VfsCacheNode* vfs_get_mount(char* path, char* path_left);
size_t vfs_read_file(char* path, void* buf, size_t count);
