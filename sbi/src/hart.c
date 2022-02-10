#include <hart.h>


HartData sbi_hart_data[NUM_HARTS];


HartStatus get_hart_status(unsigned int hart) {
    return HS_INVALID;
}
