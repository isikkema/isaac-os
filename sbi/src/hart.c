#include <hart.h>


HartData sbi_hart_data[NUM_HARTS];


HartStatus get_hart_status(unsigned int hart) {
    if (hart >= NUM_HARTS) {
        return HS_INVALID;
    }

    return sbi_hart_data[hart].status;
}
