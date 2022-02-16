#include <printf.h>
#include <console.h>
#include <string.h>
#include <page_alloc.h>
#include <sbi.h>


int main(int hart) {
    page_alloc_init();
    
    page_alloc(1);
    page_alloc(2);
    void* p = page_alloc(3);
    page_alloc(2);
    page_alloc(1);
    
    print_allocs();

    page_dealloc(p);

    print_allocs();

    page_alloc(1);

    print_allocs();

    // run_console();
    sbi_poweroff();
    
    return 0;
}
