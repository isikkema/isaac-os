#include <clint.h>
#include <hart.h>
#include <csr.h>


void clint_set_msip(int hart) {
    if (!IS_VALID_HART(hart)) {
        return;
    }
    
    printf("setting msip...\n");
    CLINT_BASE_PTR[hart] = 1;
}

void clint_unset_msip(int hart) {
    if (!IS_VALID_HART(hart)) {
        return;
    }
    
    printf("unsetting msip...\n");
    CLINT_BASE_PTR[hart] = 0;
}


unsigned long clint_get_time() {
    unsigned long time;

    asm volatile("rdtime %0" : "=r"(time));

    return time;
}

void clint_set_timer(int hart, unsigned long val) {
    if (!IS_VALID_HART(hart)) {
        return;
    }

    CLINT_TIMECMP_BASE_PTR[hart] = val;
}

void clint_handle_mtip() {
    int hart;
    unsigned long mip;

    CSR_READ(hart, "mhartid");
    
    clint_set_timer(hart, CLINT_TIME_INFINITE);

    CSR_READ(mip, "mip");
    mip &= ~MIP_MTIP;
    CSR_WRITE("mip", mip | SIP_STIP);
}
