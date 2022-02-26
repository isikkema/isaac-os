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
    if (!page_alloc_init()) {
        printf("Failed to init page_alloc\n");
        return 1;
    }

    if (!mmu_init()) {
        printf("Failed to init mmu\n");
        return 1;
    }

    if (!kmalloc_init()) {
        printf("Failed to init kmalloc\n");
        return 1;
    }

    void* tmp1;
    void* tmp2;
    void* tmp3;
    void* tmp4;
    printf("kmalloced: 0x%08lx\n", tmp1 = kmalloc(10));
    printf("kmalloced: 0x%08lx\n", tmp2 = kmalloc(20));
    printf("kmalloced: 0x%08lx\n", tmp3 = kmalloc(3995));
    printf("kmalloced: 0x%08lx\n", tmp4 = kmalloc(4097));

    memcpy(tmp1, "Hello!", 7);

    kfree(tmp2);
    kfree(tmp4);
    kfree(tmp1);
    kfree(tmp3);

    printf("[%s]\n", tmp1);
    printf("kzalloced: 0x%08lx\n", tmp1 = kzalloc(10));
    printf("[%s]\n", tmp1);
    kfree(tmp1);

    kmalloc_print(true);

    print_allocs(false);

    // run_console();
    sbi_poweroff();
    
    return 0;
}
