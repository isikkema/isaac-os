#pragma once


#include <stdbool.h>


#define FREE_BLOCKS_SIZE (8)


enum PageSize {
    PS_4K = 1 << 12,
    PS_2M = 1 << 21,
    PS_1G = 1 << 30
};

typedef struct Page {
    char data[PS_4K];
} Page;

typedef struct FreeBlock {
    int pageid;
    int num_pages;
} FreeBlock;

typedef struct PageAlloc {
    FreeBlock free_blocks[FREE_BLOCKS_SIZE];
    Page* pages;
    char* bk_bytes;
    int num_pages;
    int num_bk_bytes;
} PageAlloc;


extern PageAlloc page_alloc_data;


bool page_alloc_init(void);
void* page_alloc(int num_pages);
void* page_zalloc(int num_pages);
void page_dealloc(void* pages);

void print_allocs(bool detailed);
