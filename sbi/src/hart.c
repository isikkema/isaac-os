#include <hart.h>


HartData sbi_hart_data[NUM_HARTS];


HartStatus get_hart_status(unsigned int hart) {
    if (hart >= NUM_HARTS) {
        return HS_INVALID;
    }

    return sbi_hart_data[hart].status;
}

int hart_start(unsigned int hart, unsigned long target, int priv_mode) {
    printf("Starting hart %d at 0x%08x in mode %d\n", hart, target, priv_mode);
    
    return 0;
}
