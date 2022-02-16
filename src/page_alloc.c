#include <page_alloc.h>
#include <symbols.h>
#include <printf.h>


PageAlloc page_alloc_data;


int page_alloc_init(void) {
    unsigned long heap_size;
    unsigned long pages;
    unsigned long bk_bytes;

    if (_HEAP_START & 0xFFF) {
        printf("_HEAP_START is not aligned! (0x%08x)\n", _HEAP_START);
        return 1;
    }

    heap_size = _HEAP_END - _HEAP_START;

    // bk_bytes + 4096*pages + extra = heap_size
    // bk_bytes = ceil(pages/4)
    // extra <= 4096 // see heap_size = 20481
    // 0.25*pages + 4096*pages + extra = heap_size
    // pages = floor(heap_size / 4096.25)

    pages = (heap_size / (PS_4K + 0.25));
    bk_bytes = (pages + 3) / 4;

    page_alloc_data.pages = (Page*) _HEAP_START;
    page_alloc_data.bk_bytes = (char*) (page_alloc_data.pages + pages);
    page_alloc_data.num_pages = pages;

    printf("bk_bytes:   %ld\n", bk_bytes);
    printf("pages:      %ld\n", pages);
    printf("heap_size:  %ld\n", heap_size);
    printf("used:       %ld\n", bk_bytes + pages*4096);
    printf("extra:      %ld\n", heap_size - (bk_bytes + pages*4096));

    printf("start:      0x%08x\n", _HEAP_START);
    printf("end:        0x%08x\n", _HEAP_END);
    printf("first page: 0x%08x\n", (unsigned long) page_alloc_data.pages);
    printf("bk_bytes:   0x%08x\n", (unsigned long) page_alloc_data.bk_bytes);
    printf("bk_end:     0x%08x\n", (unsigned long) page_alloc_data.bk_bytes + bk_bytes);

    return 0;
}

void zero_pages(void* pages) {
    printf("TODO\n");
}

void* page_alloc(int num_pages) {
    return NULL;
}

void* page_zalloc(int num_pages) {
    void* pages;
    
    pages = page_alloc(num_pages);
    zero_pages(pages);

    return pages;
}

void page_dealloc(void* pages) {
    printf("TODO\n");
}
