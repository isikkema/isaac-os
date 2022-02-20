#include <printf.h>
#include <console.h>
#include <string.h>
#include <page_alloc.h>
#include <sbi.h>
#include <csr.h>
#include <start.h>


void test2() {
    printf("printing\n");
}

ATTR_NAKED_NORET
void test_function(void) {
    printf("a\n");
    test2();
    printf("b\n");

    sbi_poweroff();
}

int main(int hart) {
    sbi_hart_start(3, (unsigned long) hart_start_start, 1);

    while (1) {};
    // run_console();
    // sbi_poweroff();
    
    return 0;
}
