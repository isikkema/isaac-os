// #include "main.h"

// #define JUMP_ADDR 0x80050000

// void main(int hartid) {
//     long mstatus = (3 << 11) | (1 << 7);
//     unsigned long mtvec = (unsigned long)asm_trap_handler;
//     long medeleg = 0xb1ff;
//     long mideleg = (1 << 1) | (1 << 5) | (1 << 9);
//     long mie = (1 << 11) | (1 << 7) | (1 << 3) | (1 << 1);
//     if (hartid != 0) {
//         pmp_init();
//         while (!hart_0_is_good);
//         syscall_hsd[hartid].status = Stopped;
//         jump_to((long)hart_hang, mstatus, mtvec, medeleg, mideleg, mie);
//         return;
//     }

//     clear_memory_range(&_bss_start, &_bss_end);
//     hart_0_is_good = 1;
//     syscall_hsd[hartid].status = Started;
//     mstatus = (1 << 11) | (1 << 8) | (1 << 7) | (1 << 13);
//     mie = (1 << 11) | (1 << 7) | (1 << 3) | (1 << 1);
//     mideleg = (1 << 1) | (1 << 5) | (1 << 9);
//     jump_to(JUMP_ADDR, mstatus, mtvec, medeleg, mideleg, mie);
// }
