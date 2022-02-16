#include <page_alloc.h>
#include <symbols.h>
#include <printf.h>


#define IS_TAKEN(pageid)            (page_alloc_data.bk_bytes[pageid / 4] & (0b10 << 2*(pageid % 4)))
#define IS_LAST(pageid)             (page_alloc_data.bk_bytes[pageid / 4] & (0b01 << 2*(pageid % 4)))
#define IS_TAKEN_AND_LAST(pageid)   ((page_alloc_data.bk_bytes[pageid / 4] >> 2*(pageid % 4)) & 0b11 == 0b11)

// TODO:    SET_TAKEN
//          SET_LAST
//          SET_TAKEN_AND_LAST
//          UNSET...

#define GET_PAGEID(page)            (((Page*) page) - page_alloc_data.pages)


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

int get_num_pages(int pageid) {
    printf("TODO\n");
    return -1;
}

void zero_pages(void* pages) {
    int pageid;
    int num_pages;
    int i;

    pageid = GET_PAGEID(pages);
    num_pages = 0;
    while (1) {
        if (!IS_TAKEN(pageid)) {
            printf("Expected pageid %d to be taken, but it isn't\n", pageid);
            return;
        }

        num_pages++;

        if (IS_LAST(pageid)) {
            break;
        }

        pageid++;
    }

    for (i = 0; i < num_pages * PS_4K / 8; i++) {
        ((unsigned long*) pages)[i] = 0UL;
    }
}

void* page_alloc(int num_pages) {
    int i, j;
    int num_found;
    int found_flag;

    num_found = 0;
    found_flag = 0;
    for (i = 0; i < page_alloc_data.num_pages; i++) {
        if (!IS_TAKEN(i)) {
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
            page_alloc_data.bk_bytes[j / 4] &= ~(0b11 << (2 * (j % 4)));
            page_alloc_data.bk_bytes[j / 4] |= 0b10 << (2 * (j % 4));
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

    pageid = GET_PAGEID(pages);

    while (1) {
        if (!IS_TAKEN(pageid)) {
            printf("Expected pageid %d to be taken, but it isn't\n", pageid);
            return;
        }

        page_alloc_data.bk_bytes[pageid / 4] &= ~(0b10 << (2 * (pageid % 4)));

        if (IS_LAST(pageid)) {
            break;
        }

        pageid++;
    }
}

void print_allocs() {
    int i;
    int num_found;

    num_found = 0;
    for (i = 0; i < page_alloc_data.num_pages; i++) {
        if (IS_TAKEN(i)) {
            num_found++;
        } else if (num_found > 0) {
            printf("Corruption at %d\n", i);
            num_found = 0;
        }
        

        if (IS_LAST(i)) {
            if (num_found > 0) {
                printf("pageid: %05d --- address: 0x%08x --- pages: %02d\n", i-num_found+1, page_alloc_data.pages + (i-num_found+1), num_found);
            }

            num_found = 0;
        }
    }

    printf("done.\n");
}
