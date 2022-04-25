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
void* vfs_get_file(char* path);
size_t vfs_read_file(char* path, void* buf, size_t count);
