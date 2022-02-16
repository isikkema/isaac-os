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

    page_alloc_data.pages           = (Page*) _HEAP_START;
    page_alloc_data.bk_bytes        = (char*) (page_alloc_data.pages + pages);
    page_alloc_data.num_pages       = pages;
    page_alloc_data.num_bk_bytes    = bk_bytes;

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
    int i, j;
    char bk_byte;
    int num_found;
    int found_flag;

    num_found = 0;
    found_flag = 0;
    for (i = 0; i < page_alloc_data.num_pages; i++) {
        bk_byte = page_alloc_data.bk_bytes[i / 4];
        bk_byte = (bk_byte >> (2 * (i % 4)));
        if (!(bk_byte & 0b10)) {
            num_found++;
        } else {
            num_found = 0;
        }

        if (num_found == num_pages) {
            found_flag = 1;
            break;
        }
    }

    if (found_flag) {
        page_alloc_data.bk_bytes[i / 4] |= 0b11 << (2 * (i % 4));
        for (j = i-1; j > i - num_found; j--) {
            page_alloc_data.bk_bytes[i / 4] &= (~0b11) << (2 * (i % 4));
            page_alloc_data.bk_bytes[i / 4] |= 0b10 << (2 * (i % 4));
        }

        return page_alloc_data.pages + (i - num_found + 1);
    }

    return NULL;
}

void* page_zalloc(int num_pages) {
    void* pages;
    
    pages = page_alloc(num_pages);
    zero_pages(pages);

    return pages;
}

void page_dealloc(void* pages) {
    int pageid;
    char bk_byte;

    pageid = ((unsigned long) pages - (unsigned long) page_alloc_data.pages) / PS_4K;

    while (1) {
        bk_byte = page_alloc_data.bk_bytes[pageid / 4];
        bk_byte >>= 2 * (pageid % 4);
        if (!(bk_byte & 0b10)) {
            printf("Expected page #%d to be taken, but it isn't\n", pageid);
            return;
        }

        page_alloc_data.bk_bytes[pageid / 4] &= (~0b10) << (2 * (pageid % 4));

        if (bk_byte & 0b01) {
            break;
        }
    }
}
