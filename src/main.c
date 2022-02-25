#include <printf.h>
#include <console.h>
#include <string.h>
#include <page_alloc.h>
#include <sbi.h>
#include <csr.h>
#include <start.h>
#include <mmu.h>
#include <kmalloc.h>


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
    kernel_mmu_table = tb;

    kmalloc_init();

    void* tmp1;
    void* tmp2;
    void* tmp3;
    void* tmp4;
    printf("kmalloced: 0x%08x\n", tmp1 = kmalloc(10));
    printf("kmalloced: 0x%08x\n", tmp2 = kmalloc(20));
    printf("kmalloced: 0x%08x\n", tmp3 = kmalloc(3995));
    printf("kmalloced: 0x%08x\n", tmp4 = kmalloc(4097));

    kfree(tmp2);
    kfree(tmp4);
    kfree(tmp1);
    kfree(tmp3);
    kmalloc_print();


    // mmu_map(tb, 0x30000000, 0x30000000, PB_READ | PB_WRITE | PB_EXECUTE);
    // mmu_map(tb, 0x40000000, 0x40000000, PB_READ | PB_WRITE | PB_EXECUTE);
    // mmu_map(tb, 0x30001000, 0x30001000, PB_READ | PB_WRITE | PB_EXECUTE);

    mmu_table_print(tb, 2);
    // mmu_translations_print(tb);
    print_allocs();

    // run_console();
    sbi_poweroff();
    
    return 0;
}
