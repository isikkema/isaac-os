#include <hart.h>


#define IS_VALID_HART(hart) (hart >= 0 && hart < NUM_HARTS)


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
    // set msip

    // unlock

    return 1;
}
