#include <csr.h>
#include <syscall.h>
#include <printf.h>
#include <plic.h>

void c_trap(void) {
    long mcause;
    long mhartid;
    long* mscratch;
    long mepc;
    int is_async;

    CSR_READ(mcause, "mcause");
    CSR_READ(mhartid, "mhartid");
    CSR_READ(mepc, "mepc");

    printf("mcause: 0x%08lx, mhartid: %d\n", mcause, mhartid);

    is_async = mcause >> 63;    // MSB is 1 if async
    mcause ^= (long)1 << 63;    // Set MSB to 0. C bugs are so stupid.

    printf("mcause: 0x%08lx, async: %d\n", mcause, is_async);

    if (is_async) {
        switch (mcause) {
            case 11:
                plic_handle_irq(mhartid);
                break;
            
            default:
                printf("error: c_trap: unhandled asynchonous interrupt: %ld\n", mcause);
        }
    } else {
        // switch (mcause) {
        //     case 9:
        //         // env call from s mode
        //         printf("mscratch[17]: %ld\n", mscratch[17]);
        //         CSR_WRITE("mepc", mepc + 4);
                
        //         break;
        // }
    }
}
