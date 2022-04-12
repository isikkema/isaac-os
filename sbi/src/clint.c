#include <clint.h>
#include <hart.h>
#include <csr.h>


void clint_set_msip(int hart) {
    if (!IS_VALID_HART(hart)) {
        return;
    }
    
    CLINT_BASE_PTR[hart] = 1;
}

void clint_unset_msip(int hart) {
    if (!IS_VALID_HART(hart)) {
        return;
    }
    
    CLINT_BASE_PTR[hart] = 0;
}


unsigned long clint_get_time() {
    return *CLINT_TIME_PTR;
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
    CSR_WRITE("mip", mip | MIP_STIP);
}
