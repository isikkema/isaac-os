#pragma once

#include <stddef.h>
#include <stdbool.h>


#define KERNEL_HEAP_START_VADDR     0x120000000
#define KMALLOC_MINIMUM_NODE_SIZE   16UL


bool kmalloc_init(void);
void* kmalloc(size_t bytes);
void* kzalloc(size_t bytes);
void kfree(void* mem);
void coalesce_free_list(void);

void kmalloc_print(bool detailed);
