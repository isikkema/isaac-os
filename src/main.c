#include <printf.h>
#include <console.h>
#include <string.h>
#include <page_alloc.h>
#include <sbi.h>
#include <csr.h>
#include <start.h>
#include <mmu.h>


void test2() {
    printf("printing from test2\n");
}

ATTR_NAKED_NORET
void test_function(void) {
    printf("Running test_function()\n");
    test2();
    printf("After test2\n");

    sbi_hart_stop();

    printf("failed to stop hart\n");
    park();
}

int main(int hart) {
    if (page_alloc_init()) {
        printf("Failed to init page\n");
        return 1;
    }

    PageTable* tb = page_zalloc(1);

    mmu_map(tb, 0x30000000, 0x30000000, PB_READ | PB_WRITE | PB_EXECUTE);
    mmu_map(tb, 0x40000000, 0x40000000, PB_READ | PB_WRITE | PB_EXECUTE);
    mmu_map(tb, 0x30001000, 0x30001000, PB_READ | PB_WRITE | PB_EXECUTE);

    mmu_table_print(tb, 2);
    mmu_translations_print(tb);
    print_allocs();

    // run_console();
    sbi_poweroff();
    
    return 0;
}
