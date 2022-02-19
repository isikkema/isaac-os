#include <printf.h>
#include <console.h>
#include <string.h>
#include <page_alloc.h>
#include <sbi.h>

void test_function(void) {
    printf("Test function!\n");
}

int main(int hart) {
    sbi_hart_start(3, (unsigned long) test_function, 1);

    // run_console();
    sbi_poweroff();
    
    return 0;
}
