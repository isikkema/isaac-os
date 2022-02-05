#include <csr.h>
#include <syscall.h>

void c_trap(void) {
    long mcause;
    long mhartid;
    long* mscratch;
    long mepc;

    CSR_READ(mcause, "mcause");
    CSR_READ(mhartid, "mhartid");
    CSR_READ(mhartid, "mepc");

    printf("mcause: %08lx, mhartid: %d\n", mcause, mhartid);

    // while (1) {};
    
    if (mcause < 0) {
        // async
    } else {
        // exception
    }

    // strip mcause MSB

    switch (mcause) {
        case 11:
            // machine external
            break;
    }

    // sync
    switch (mcause) {
        case 9:
            // env call from s mode
            printf("mscratch[17]: %ld\n", mscratch[17]);
            CSR_WRITE("mepc", mepc + 4);
            
            break;
    }
}
