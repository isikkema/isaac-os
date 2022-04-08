// sbi.c
// Defines wrappers for svccalls, which are handled in sbi/svccall.c


#include "../../sbi/src/include/svccodes.h"
#include <hart.h>


void sbi_putchar(char c) {
    // a7: SBI_PUTCHAR
    // a0: c
    asm volatile ("mv a7, %0\nmv a0, %1\necall" :: "r"(SBI_PUTCHAR), "r"(c) : "a7", "a0");
}

char sbi_getchar(void) {
    char c;

    // a7: SBI_GETCHAR
    asm volatile ("mv a7, %1\necall\nmv %0, a0" : "=r"(c) : "r"(SBI_GETCHAR) : "a7", "a0");
    // a0: c

    return c;
}

HartStatus sbi_get_hart_status(int hart) {
    HartStatus status;

    // a7: SBI_GET_HART_STATUS
    // a0: hart
    asm volatile ("mv a7, %1\nmv a0, %2\necall\nmv %0, a0" : "=r"(status) : "r"(SBI_GET_HART_STATUS), "r"(hart) : "a7", "a0");
    // a0: status

    return status;
}

bool sbi_hart_start(int hart, uint64_t target, uint64_t scratch) {
    bool started;

    // a7: SBI_HART_START
    // a0: hart
    // a1: target
    // a2: scratch
    asm volatile ("mv a7, %1\nmv a0, %2\nmv a1, %3\nmv a2, %4\necall\nmv %0, a0" : "=r"(started) : "r"(SBI_HART_START), "r"(hart), "r"(target), "r"(scratch) : "a7", "a0", "a1", "a2");
    // a0: started

    return started;
}

bool sbi_hart_stop(void) {
    bool stopped;

    asm volatile ("mv a7, %1\necall\nmv %0, a0" : "=r"(stopped) : "r"(SBI_HART_STOP) : "a7", "a0");

    return stopped;
}

int sbi_whoami(void) {
    int hart;

    asm volatile ("mv a7, %1\necall\nmv %0, a0" : "=r"(hart) : "r"(SBI_WHOAMI) : "a7", "a0");

    return hart;
}

void sbi_poweroff(void) {
    asm volatile ("mv a7, %0\necall" :: "r"(SBI_POWEROFF) : "a7");
}
