#include <start.h>
#include <uart.h>
#include <printf.h>
#include <plic.h>
#include <csr.h>
#include <lock.h>
#include <hart.h>
#include <hart.h>
#include <symbols.h>


long int SBI_GPREGS[32][NUM_HARTS];

Barrier barrier = {INT32_MAX};


static void pmp_init() {
    CSR_WRITE("pmpaddr0", 0xffffffff >> 2);
    CSR_WRITE("pmpcfg0", (0b01 << 3) | (1 << 2) | (1 << 1) | (1 << 0));
}

void clear_bss() {
    long* it;
    long* end;

    it = (long*) &_bss_start;
    end = (long*) &_bss_end;
    while (it < end) {
        *it = 0;
        it++;
    }
}

int main(int hartid) {
    if (hartid == 0) {
        clear_bss();
        uart_init();

        barrier_release(&barrier);

        plic_init();
        pmp_init();

        sbi_hart_data[hartid].status = HS_STARTED;
        sbi_hart_data[hartid].priv = HPM_MACHINE;
        sbi_hart_data[hartid].target_address = 0x0;

        CSR_WRITE("mscratch", SBI_GPREGS[hartid]);
    	CSR_WRITE("sscratch", hartid);

        CSR_WRITE("mepc", 0x80050000UL);

    	CSR_WRITE("mie", (1 << 11) | (1 << 7) | (1 << 3));

        CSR_WRITE("mideleg", (1 << 1) | (1 << 5) | (1 << 7));
        CSR_WRITE("medeleg", 0xB1FF);
        CSR_WRITE("mstatus", (1 << 13) | (1 << 11));

        asm volatile ("mret");
    }

    barrier_sbi_wait(&barrier);

    pmp_init();

    sbi_hart_data[hartid].status = HS_STOPPED;
    sbi_hart_data[hartid].priv = HPM_MACHINE;
    sbi_hart_data[hartid].target_address = 0x0;

    CSR_WRITE("mscratch", SBI_GPREGS[hartid]);
    CSR_WRITE("sscratch", hartid);

    CSR_WRITE("mepc", park);

    CSR_WRITE("mie", 1 << 3);

    CSR_WRITE("mideleg", 0);
    CSR_WRITE("medeleg", 0);
    CSR_WRITE("mstatus", (1 << 13) | (1 << 11));

    asm volatile ("mret");
}
