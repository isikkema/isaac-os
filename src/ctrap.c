#include <csr.h>
#include <rs_int.h>
#include <printf.h>
#include <plic.h>
#include <sbi.h>


void c_trap(void) {
    u64 scause;
    u64 sepc;
    u32 hart;
    bool is_async;

    CSR_READ(scause, "scause");
    CSR_READ(sepc, "sepc");

    hart = sbi_whoami();

    is_async = MCAUSE_IS_ASYNC(scause);
    scause = MCAUSE_NUM(scause);

    if (is_async) {
        switch (scause) {
            case 9:
                plic_handle_irq(hart);
                break;
            
            default:
                printf("error: c_trap: unhandled asynchronous interrupt: %ld\n", scause);
        }
    } else {
        switch (scause) {
            default:
                printf("error: c_trap: unhandled synchronous interrupt: %ld\n", scause);
                printf("waiting for interrupt...\n");
                WFI();
        }
    }
}
