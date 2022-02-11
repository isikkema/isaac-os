#include <hart.h>


HartData sbi_hart_data[NUM_HARTS];


HartStatus get_hart_status(unsigned int hart) {
    if (hart == 0) {
        return HS_STARTED;
    } else {
        return HS_STOPPED;
    }
}
