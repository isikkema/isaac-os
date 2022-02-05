#include <sbi.h>


void sbi_putchar(char c) {
    asm volatile ("mv a7, %0\nmv a0, %1\necall" :: "r"(SBI_PUTCHAR), "r"(c) : "a7", "a0");
}

char sbi_getchar(void) {
    char c;

    asm volatile ("mv a7, %1\necall\nmv %0, a0" : "=r"(c) : "r"(SBI_GETCHAR) : "a7", "a0");

    return c;
}
