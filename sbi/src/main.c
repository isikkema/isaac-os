#include <start.h>
#include <uart.h>
#include <printf.h>
#include <plic.h>
#include <csr.h>
#include <lock.h>


Barrier barrier = {8};
Mutex mut;

long int SBI_GPREGS[32][8];


static void pmp_init() {
    CSR_WRITE("pmpaddr0", 0xffffffff >> 2);
    CSR_WRITE("pmpcfg0", (0b01 << 3) | (1 << 2) | (1 << 1) | (1 << 0));
}


int main(int hartid) {
    // while (hartid != 0) {
    //     // sleep
    // }
    
    if (hartid == 0) {
        uart_init();
        plic_init();
        pmp_init();
    }

    for (int i = 0; i < 100000000*hartid; i++) {
        1000.0 / 3.0;
    }

    mutex_spinlock(&mut);
    printf("Hart %d waiting...\n", hartid);
    mutex_unlock(&mut);

    barrier_spinwait(&barrier);

    mutex_spinlock(&mut);
    printf("Hart %d got past barrier!\n", hartid);
    mutex_unlock(&mut);

    while (1) {};

    CSR_WRITE("mscratch", &SBI_GPREGS[0][hartid]);

    CSR_WRITE("mepc", 0x80050000UL);
    CSR_WRITE("mideleg", (1 << 1) | (1 << 5) | (1 << 7));
    CSR_WRITE("medeleg", 0xB1FF);
    CSR_WRITE("mstatus", (1 << 13) | (1 << 11));

    asm volatile ("mret");

    return 0;
}
