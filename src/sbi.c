#include "../../sbi/src/include/svccodes.h"
#include <hart.h>


void sbi_putchar(char c) {
    asm volatile ("mv a7, %0\nmv a0, %1\necall" :: "r"(SBI_PUTCHAR), "r"(c) : "a7", "a0");
}

char sbi_getchar(void) {
    char c;

    asm volatile ("mv a7, %1\necall\nmv %0, a0" : "=r"(c) : "r"(SBI_GETCHAR) : "a7", "a0");

    return c;
}

HartStatus sbi_get_hart_status(int hart) {
    HartStatus status;

    asm volatile ("mv a7, %1\nmv a0, %2\necall\nmv %0, a0" : "=r"(status) : "r"(SBI_GET_HART_STATUS), "r"(hart) : "a7", "a0");
    
    return status;
}

void sbi_poweroff(void) {
    asm volatile ("mv a7, %0\necall" :: "r"(SBI_POWEROFF) : "a7");
}
