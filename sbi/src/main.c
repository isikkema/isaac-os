#include <start.h>
#include <uart.h>
#include <printf.h>
#include <plic.h>
#include <csr.h>
#include <lock.h>
#include <hart.h>


long int SBI_GPREGS[32][NUM_HARTS];


static void pmp_init() {
    CSR_WRITE("pmpaddr0", 0xffffffff >> 2);
    CSR_WRITE("pmpcfg0", (0b01 << 3) | (1 << 2) | (1 << 1) | (1 << 0));
}


int main(int hartid) {
    while (hartid != 0) {
        // sleep
    }
    
    if (hartid == 0) {
        uart_init();
        plic_init();
        pmp_init();
    }

    CSR_WRITE("mscratch", SBI_GPREGS[hartid]);

    CSR_WRITE("mepc", 0x80050000UL);
    CSR_WRITE("mideleg", (1 << 1) | (1 << 5) | (1 << 7));
    CSR_WRITE("medeleg", 0xB1FF);
    CSR_WRITE("mstatus", (1 << 13) | (1 << 11));

    asm volatile ("mret");

    return 0;
}
