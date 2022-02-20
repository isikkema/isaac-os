#include <clint.h>
#include <hart.h>


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
