#pragma once


enum PageSize {
    PS_4K = 1 << 12,
    PS_2M = 1 << 21,
    PS_1G = 1 << 30
};

typedef struct Page {
    char data[PS_4K];
} Page;

typedef struct PageAlloc {
    Page* pages;
    char* bk_bytes;
    int num_pages;
    int num_bk_bytes;
} PageAlloc;


extern PageAlloc page_alloc_data;


int page_alloc_init(void);
void* page_alloc(int num_pages);
void* page_zalloc(int num_pages);
void page_dealloc(void* pages);

void print_allocs();
