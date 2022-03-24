#include <printf.h>
#include <console.h>
#include <page_alloc.h>
#include <sbi.h>
#include <csr.h>
#include <start.h>
#include <mmu.h>
#include <kmalloc.h>
#include <pci.h>
#include <plic.h>


uint64_t OS_GPREGS[32];


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

    if (!pci_init()) {
        printf("Failed to init pci\n");
        return 1;
    }

    if (!plic_init(hart)) {
        printf("Failed to init plic\n");
        return 1;
    }

    CSR_WRITE("sscratch", OS_GPREGS);

    // todo: add help, malloc, and free commands
    //       also improve hart starting/stopping
    run_console();
    sbi_poweroff();
    
    return 0;
}
