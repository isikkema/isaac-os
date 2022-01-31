#include "start.h"
#include "trap.h"
#include "clint.h"
#include "symbols.h"
#include "uart.h"

#define JUMP_ADDR 0x80050000

int hart_0_is_good = 0;

void puts(char* s) {
    for (; *s; s++) {
        uart_put(*s);
    }
}

int main(int hartid) {
    while (hartid != 0) {}
    
    uart_init();
    puts("Hello, World!!!!\n\n");

    char c;
    while (1) {
        c = uart_get();
        if (c != 0xff) {
            puts("[");
            uart_put(c);
            puts("]");
        }
    }

    return 0;
    // long mstatus = (3 << 11) | (1 << 7);
    // unsigned long mtvec = (unsigned long)asm_trap_handler;
    // long medeleg = 0xb1ff;
    // long mideleg = (1 << 1) | (1 << 5) | (1 << 9);
    // long mie = (1 << 11) | (1 << 7) | (1 << 3) | (1 << 1);
    // if (hartid != 0) {
    //     pmp_init();
    //     while (!hart_0_is_good);
    //     syscall_hsd[hartid].status = Stopped;
    //     jump_to((long)hart_hang, mstatus, mtvec, medeleg, mideleg, mie);
    //     return;
    // }

    // pmp_init();
    // clear_memory_range(&_bss_start, &_bss_end);
    // hart_0_is_good = 1;
    // syscall_hsd[hartid].status = Started;
    // mstatus = (1 << 11) | (1 << 8) | (1 << 7) | (1 << 13);
    // mie = (1 << 11) | (1 << 7) | (1 << 3) | (1 << 1);
    // mideleg = (1 << 1) | (1 << 5) | (1 << 9);
    // jump_to(JUMP_ADDR, mstatus, mtvec, medeleg, mideleg, mie);
}
