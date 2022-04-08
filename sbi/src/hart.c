#include <hart.h>
#include <clint.h>
#include <csr.h>
#include <start.h>
#include <printf.h>
#include <lock.h>


HartData sbi_hart_data[NUM_HARTS];


HartStatus get_hart_status(int hart) {
    if (!IS_VALID_HART(hart)) {
        return HS_INVALID;
    }

    return sbi_hart_data[hart].status;
}

bool hart_start(int hart, uint64_t target, uint64_t scratch) {
    printf("Starting hart %d at 0x%08x with sscratch at 0x%08x\n", hart, target, scratch);

    if (!IS_VALID_HART(hart)) {
        return false;
    }

    if (!mutex_trylock(&sbi_hart_data[hart].lock)) {
        return false;
    }
    
    if (sbi_hart_data[hart].status != HS_STOPPED) {
        mutex_unlock(&sbi_hart_data[hart].lock);
        return false;
    }

    sbi_hart_data[hart].status = HS_STARTING;
    sbi_hart_data[hart].target_address = target;
    sbi_hart_data[hart].scratch = scratch;
    clint_set_msip(hart);

    mutex_unlock(&sbi_hart_data[hart].lock);
    return true;
}

bool hart_stop(int hart) {
    if (!IS_VALID_HART(hart)) {
        return false;
    }

    if (!mutex_trylock(&sbi_hart_data[hart].lock)) {
        return false;
    }

    if (sbi_hart_data[hart].status != HS_STARTED) {
        mutex_unlock(&sbi_hart_data[hart].lock);
        return false;
    }

    sbi_hart_data[hart].status = HS_STOPPED;
    CSR_WRITE("mepc", park);
    CSR_WRITE("mstatus", MSTATUS_MPP_MACHINE | MSTATUS_MPIE);
    CSR_WRITE("mie", MIE_MSIE);
    CSR_WRITE("satp", 0);
    CSR_WRITE("sscratch", 0);
    CSR_WRITE("stvec", 0);
    CSR_WRITE("sepc", 0);
    CSR_WRITE("mip", 0);

    mutex_unlock(&sbi_hart_data[hart].lock);
    MRET();

    // In case we didn't mret
    return false;
}

void hart_handle_msip(int hart) {
    if (!IS_VALID_HART(hart)) {
        return;
    }
    
    mutex_sbi_lock(&sbi_hart_data[hart].lock);

    clint_unset_msip(hart);

    printf("handling msip -- %d -- %d\n", hart, sbi_hart_data[hart].status);

    if (sbi_hart_data[hart].status != HS_STARTING) {
        mutex_unlock(&sbi_hart_data[hart].lock);
        return;
    }

    int current_hart;
    CSR_READ(current_hart, "mhartid");
    printf("hanging on hart %d...\n", current_hart);
    while (true) {
        WFI();
    }

    CSR_WRITE("mepc", sbi_hart_data[hart].target_address);
    CSR_WRITE("mstatus", MSTATUS_MPP_SUPERVISOR | MSTATUS_MPIE | MSTATUS_FS_INITIAL);
    CSR_WRITE("mie", MIE_SSIE | MIE_STIE | MIE_MTIE);
    CSR_WRITE("mideleg", SIP_SEIP | SIP_SSIP | SIP_STIP);
    CSR_WRITE("medeleg", MEDELEG_ALL);
    CSR_WRITE("sscratch", sbi_hart_data[hart].scratch);

    sbi_hart_data[hart].status = HS_STARTED;

    mutex_unlock(&sbi_hart_data[hart].lock);
    MRET();
}
