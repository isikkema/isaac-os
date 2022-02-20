// ctrap.c
// Delegate handling of interrupts based on mcause


#include <csr.h>
#include <svccall.h>
#include <printf.h>
#include <plic.h>
#include <clint.h>


void c_trap(void) {
    long mcause;
    long mhartid;
    long* mscratch;
    unsigned long mepc;
    int is_async;

    CSR_READ(mcause, "mcause");
    CSR_READ(mhartid, "mhartid");
    CSR_READ(mscratch, "mscratch");
    CSR_READ(mepc, "mepc");

    is_async = MCAUSE_IS_ASYNC(mcause);
    mcause = MCAUSE_NUM(mcause);

    if (is_async) {
        switch (mcause) {
            case 3: // MSIP
                printf("GOT MSIP\n");
                hart_handle_msip(mhartid);
                break;

            case 11:
                plic_handle_irq(mhartid);
                break;
            
            default:
                printf("error: c_trap: unhandled asynchronous interrupt: %ld\n", mcause);
        }
    } else {
        switch (mcause) {
            case 9:
                svccall_handle(mhartid);
                CSR_WRITE("mepc", mepc + 4);    // Return to next instruction
                break;
            
            default:
                printf("error: c_trap: unhandled synchronous interrupt: %ld\n", mcause);
        }
    }
}
