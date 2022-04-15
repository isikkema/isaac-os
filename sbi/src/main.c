#include <start.h>
#include <trap.h>
#include <uart.h>
#include <printf.h>
#include <plic.h>
#include <csr.h>
#include <lock.h>
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

ATTR_NAKED_NORET
int main(int hartid) {
    if (hartid == 0) {
        // Initialize machine devices, hart 0 status, interrupt settings, and then boot into the OS

        clear_bss();
        uart_init();

        barrier_release(&barrier);  // Allow other harts through barrier

        if (!plic_init(hartid)) {
            printf("SBI: failed to init plic\n");
            park();
        }

        pmp_init();

        sbi_hart_data[hartid].status = HS_STARTED;
        sbi_hart_data[hartid].scratch = HPM_MACHINE;
        sbi_hart_data[hartid].target_address = 0x0;

        CSR_WRITE("mscratch", SBI_GPREGS[hartid]);
    	CSR_WRITE("sscratch", hartid);

        CSR_WRITE("mepc", OS_LOAD_ADDR);
		CSR_WRITE("mtvec", sbi_trap_vector);

    	CSR_WRITE("mie", MIE_MEIE | MIE_MTIE | MIE_MSIE);

        CSR_WRITE("mideleg", SIP_SEIP | SIP_STIP | SIP_SSIP);
        CSR_WRITE("medeleg", MEDELEG_ALL);
        CSR_WRITE("mstatus", MSTATUS_FS_INITIAL | MSTATUS_MPP_SUPERVISOR | MSTATUS_MPIE);

        MRET();
    }

    barrier_sbi_wait(&barrier); // Wait for hart 0

    // Initialize other hart statuses, interrupt settings, and then wait until needed

    pmp_init();

    sbi_hart_data[hartid].status = HS_STOPPED;
    sbi_hart_data[hartid].scratch = HPM_MACHINE;
    sbi_hart_data[hartid].target_address = 0x0;

    CSR_WRITE("mscratch", SBI_GPREGS[hartid]);
    CSR_WRITE("sscratch", hartid);

    CSR_WRITE("mepc", park);
    CSR_WRITE("mtvec", sbi_trap_vector);

    CSR_WRITE("mie", MIE_MSIE);

    CSR_WRITE("mideleg", 0);
    CSR_WRITE("medeleg", 0);
    CSR_WRITE("mstatus", MSTATUS_FS_INITIAL | MSTATUS_MPP_MACHINE | MSTATUS_MPIE);

    unsigned long sp;
    asm volatile("mv %0, sp" : "=r"(sp));
    printf("SBI: hart: %d, sp: 0x%08lx\n", hartid, sp);

    // asm volatile("mv sp, %0" :: "r"(hartid ))
    // asm volatile("la sp, _stack_end");
    // asm volatile("sub sp, sp, %0" :: "r"(hartid * 4096));

    MRET();
}
