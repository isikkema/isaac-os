#include <hart.h>
#include <clint.h>
#include <csr.h>
#include <start.h>


HartData sbi_hart_data[NUM_HARTS];


HartStatus get_hart_status(unsigned int hart) {
    if (!IS_VALID_HART(hart)) {
        return HS_INVALID;
    }

    return sbi_hart_data[hart].status;
}

int hart_start(unsigned int hart, unsigned long target, int priv_mode) {
    priv_mode &= 0b1;

    printf("Starting hart %d at 0x%08x in mode %d\n", hart, target, priv_mode);

    if (!IS_VALID_HART(hart)) {
        return 0;
    }

    // try lock
    
    sbi_hart_data[hart].status = HS_STARTING;
    sbi_hart_data[hart].target_address = target;
    sbi_hart_data[hart].priv = priv_mode;
    clint_set_msip(hart);

    // unlock

    return 1;
}

int hart_stop(unsigned int hart) {
    sbi_hart_data[hart].status = HS_STOPPED;
    CSR_WRITE("mepc", park);
    CSR_WRITE("mstatus", MSTATUS_MPP_MACHINE | MSTATUS_MPIE);
    CSR_WRITE("mie", MIE_MSIE);
    CSR_WRITE("satp", 0);
    CSR_WRITE("sscratch", 0);
    CSR_WRITE("stvec", 0);
    CSR_WRITE("sepc", 0);
    CSR_WRITE("mip", 0);
    MRET();

    return 0;
}

void hart_handle_msip(unsigned int hart) {
    if (!IS_VALID_HART(hart)) {
        return;
    }
    
    // lock

    clint_unset_msip(hart);

    printf("handling msip -- %d -- %d\n", hart, sbi_hart_data[hart].status);

    if (sbi_hart_data[hart].status == HS_STARTING) {
        CSR_WRITE("mepc", sbi_hart_data[hart].target_address);
        CSR_WRITE("mstatus", (sbi_hart_data[hart].priv << MSTATUS_MPP_BIT) | MSTATUS_MPIE | MSTATUS_FS_INITIAL);
        CSR_WRITE("mie", MIE_SSIE | MIE_STIE | MIE_MTIE);
        CSR_WRITE("mideleg", SIP_SEIP | SIP_SSIP | SIP_STIP);
        CSR_WRITE("medeleg", MEDELEG_ALL);
        CSR_WRITE("sscratch", hart);

        sbi_hart_data[hart].status = HS_STARTED;
    }

    // unlock
    MRET();
}
