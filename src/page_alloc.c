#include <symbols.h>
#include <printf.h>


enum PageSize {
    PS_4K = 1 << 12,
    PS_2M = 1 << 21,
    PS_1G = 1 << 30
};

typedef struct {
    char data[PS_4K];
} Page;


void page_alloc_init(void) {
    unsigned long heap_size;
    unsigned long pages;
    unsigned long bk_bytes;

    heap_size = _HEAP_END - _HEAP_START + 1;
    heap_size = 20482;

    // bk_bytes + 4096*pages + extra = heap_size
    // bk_bytes = ceil(pages/4)
    // extra <= 4096 // see heap_size = 20481
    // 0.25*pages + 4096*pages + extra = heap_size
    // pages = floor(heap_size / 4096.25)

    pages = (heap_size / (PS_4K + 0.25));
    bk_bytes = pages / 4;
    if (pages % 4 != 0) {
        bk_bytes++;
    }

    printf("bk_bytes: %ld\n", bk_bytes);
    printf("pages: %ld\n", pages);
    printf("heap_size: %ld\n", heap_size);
    printf("used: %ld\n", bk_bytes + pages*4096);
    printf("extra: %ld\n", heap_size - (bk_bytes + pages*4096));
}
