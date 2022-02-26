#include <page_alloc.h>
#include <symbols.h>
#include <printf.h>


#define IS_TAKEN(pageid)            (page_alloc_data.bk_bytes[pageid / 4] & (0b10 << 2*(pageid % 4)))
#define IS_LAST(pageid)             (page_alloc_data.bk_bytes[pageid / 4] & (0b01 << 2*(pageid % 4)))
#define IS_TAKEN_AND_LAST(pageid)   (((page_alloc_data.bk_bytes[pageid / 4] >> 2*(pageid % 4)) & 0b11) == 0b11)

#define SET_TAKEN(pageid)           (page_alloc_data.bk_bytes[pageid / 4] |= 0b10 << 2*(pageid % 4))
#define SET_LAST(pageid)            (page_alloc_data.bk_bytes[pageid / 4] |= 0b01 << 2*(pageid % 4))
#define SET_TAKEN_AND_LAST(pageid)  (page_alloc_data.bk_bytes[pageid / 4] |= 0b11 << 2*(pageid % 4))

#define UNSET_TAKEN(pageid)         (page_alloc_data.bk_bytes[pageid / 4] &= ~(0b10 << 2*(pageid % 4)))
#define UNSET_LAST(pageid)          (page_alloc_data.bk_bytes[pageid / 4] &= ~(0b01 << 2*(pageid % 4)))

#define GET_PAGEID(page)            (((Page*) page) - page_alloc_data.pages)


PageAlloc page_alloc_data;


bool page_alloc_init(void) {
    unsigned long heap_size;
    int num_pages;
    int num_bk_bytes;

    if (_HEAP_START & 0xFFF) {
        printf("_HEAP_START is not aligned! (0x%08x)\n", _HEAP_START);
        return false;
    }

    heap_size = _HEAP_END - _HEAP_START;

    // bk_bytes + 4096*pages + extra = heap_size
    // bk_bytes = ceil(pages/4)
    // extra <= 4096 // see heap_size = 20481
    // 0.25*pages + 4096*pages + extra = heap_size
    // pages = floor(heap_size / 4096.25)

    num_pages = heap_size / (PS_4K + 0.25);
    num_bk_bytes = (num_pages + 3) / 4;

    page_alloc_data.pages           = (Page*) _HEAP_START;                          // Putting pages at the start and bk_bytes after pages
    page_alloc_data.bk_bytes        = (char*) (page_alloc_data.pages + num_pages);
    page_alloc_data.num_pages       = num_pages;
    page_alloc_data.num_bk_bytes    = num_bk_bytes;

    return true;
}

int get_num_pages(int pageid) {
    int num_pages;

    num_pages = 1;
    for (; pageid < page_alloc_data.num_pages; pageid++) {
        if (IS_TAKEN_AND_LAST(pageid)) {
            return num_pages;
        } else if (IS_TAKEN(pageid)) {
            num_pages++;
        } else if (num_pages == 1) {
            return 0;
        } else {
            printf("Corruption at pageid %d\n", pageid);
            return -1;
        }
    }

    return -1;
}

void zero_pages(void* pages) {
    int pageid;
    int num_pages;
    int i;

    pageid = GET_PAGEID(pages);
    num_pages = get_num_pages(pageid);

    for (i = 0; i < num_pages * PS_4K / 8; i++) {
        ((unsigned long*) pages)[i] = 0UL;
    }
}

void* page_alloc(int num_pages) {
    int i;
    int pageid;
    int num_found;
    int found_flag;

    num_found = 0;
    pageid = 0;
    found_flag = 0;

    for (i = 0; i < FREE_BLOCKS_SIZE; i++) {
        if (page_alloc_data.free_blocks[i].num_pages >= num_pages) {
            pageid = page_alloc_data.free_blocks[i].pageid;
            found_flag = 1;

            page_alloc_data.free_blocks[i] = (FreeBlock) {
                page_alloc_data.free_blocks[i].pageid + num_pages,
                page_alloc_data.free_blocks[i].num_pages - num_pages
            };

            break;
        }
    }

    if (!found_flag) {
        for (i = 0; i < page_alloc_data.num_pages; i++) {
            if (!IS_TAKEN(i)) {
                num_found++;
            } else {
                num_found = 0;
                pageid = i + 1;
            }

            if (num_found == num_pages) {
                found_flag = 1;
                break;
            }
        }
    }

    if (!found_flag) {
        return NULL;
    }

    for (i = pageid; i < pageid + num_pages - 1; i++) {
        UNSET_LAST(i);
        SET_TAKEN(i);
    }

    SET_TAKEN_AND_LAST(i);

    return page_alloc_data.pages + pageid;
}

void* page_zalloc(int num_pages) {
    void* pages;
    
    pages = page_alloc(num_pages);
    zero_pages(pages);

    return pages;
}

void page_dealloc(void* pages) {
    int i;
    int pageid;
    int num_pages;

    pageid = GET_PAGEID(pages);
    num_pages = get_num_pages(pageid);
    for (i = pageid; i < pageid + num_pages; i++) {
        UNSET_TAKEN(i);
    }

    for (i = 0; i < FREE_BLOCKS_SIZE; i++) {
        if (page_alloc_data.free_blocks[i].num_pages == 0) {
            page_alloc_data.free_blocks[i] = (FreeBlock) {pageid, num_pages};
            break;
        }
    }
}

void print_allocs(bool detailed) {
    int pageid;
    int num_pages;
    int total_allocated;

    total_allocated = 0;
    pageid = 0;
    while (pageid < page_alloc_data.num_pages) {
        num_pages = get_num_pages(pageid);
        if (num_pages > 0) {
            if (detailed)
                printf("pageid: %05d --- address: 0x%08x --- pages: %02d\n", pageid, page_alloc_data.pages + pageid, num_pages);
            
            total_allocated += num_pages;
            pageid += num_pages;
        } else {
            pageid++;
        }
    }

    printf("Total allocated pages: %d --- Total free pages: %d\n", total_allocated, page_alloc_data.num_pages - total_allocated);
}
