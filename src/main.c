#include <printf.h>
#include <console.h>
#include <string.h>
#include <page_alloc.h>
#include <sbi.h>


int main(int hart) {
    page_alloc_init();
    
    page_alloc(1);
    page_alloc(2);
    char* a = page_alloc(3);
    page_alloc(2);
    page_alloc(1);
    
    a[0] = 'h';
    a[1] = 'e';
    a[2] = 'l';
    a[3] = 'l';
    a[4] = 'o';
    a[5] = '\0';
    print_allocs();

    page_dealloc(a);

    print_allocs();

    char* b = page_zalloc(1);
    printf("[%s]\n", b);

    print_allocs();

    // run_console();
    sbi_poweroff();
    
    return 0;
}
