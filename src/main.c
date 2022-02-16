#include <printf.h>
#include <console.h>
#include <string.h>
#include <page_alloc.h>


int main(int hart) {
    page_alloc_init();
    page_alloc(1);
    print_allocs();

    while (1) {};
    run_console();

    while (1) {};
    
    return 0;
}
