#include <csr.h>
#include <rs_int.h>
#include <printf.h>
#include <plic.h>


void c_trap(void) {
    u64 scause;
    u64 sepc;
    bool is_async;

    CSR_READ(scause, "scause");
    CSR_READ(sepc, "sepc");

    is_async = MCAUSE_IS_ASYNC(scause);
    scause = MCAUSE_NUM(scause);

    if (is_async) {
        switch (scause) {
            case 9:
                plic_handle_irq(0);
                break;
            
            default:
                printf("error: c_trap: unhandled asynchronous interrupt: %ld\n", scause);
        }
    } else {
        switch (scause) {
            default:
                printf("error: c_trap: unhandled synchronous interrupt: %ld\n", scause);
        }
    }
}
