#include <csr.h>


typedef struct TrapFrame {
    long gp_regs[32];
    long fp_regs[32];
    unsigned long pc;
} TrapFrame;


TrapFrame c_sbi_trap_frame;

void c_trap(void) {
    long mcause;

    CSR_READ(mcause, "mcause");
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
}
