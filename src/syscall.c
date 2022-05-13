#include <syscall.h>
#include <csr.h>
#include <sbi.h>
#include <process.h>
#include <schedule.h>
#include <printf.h>


void syscall_handle(Process* process) {
    uint64_t a0;
    // uint64_t a1;
    // uint64_t a2;
    // uint64_t a3;
    // uint64_t a4;
    // uint64_t a5;
    // uint64_t a6;
    uint64_t a7;

    int hart;

    a0 = process->frame.gpregs[XREG_A0];
    // a1 = process->frame.gpregs[XREG_A1];
    // a2 = process->frame.gpregs[XREG_A2];
    // a3 = process->frame.gpregs[XREG_A3];
    // a4 = process->frame.gpregs[XREG_A4];
    // a5 = process->frame.gpregs[XREG_A5];
    // a6 = process->frame.gpregs[XREG_A6];
    a7 = process->frame.gpregs[XREG_A7];

    hart = sbi_whoami();

    switch (a7) {
        case SYS_EXIT: ;
            process->state = PS_DEAD;
            schedule_park(hart);
            schedule_remove(process);
            
            process_free(process);

            schedule_schedule(hart);
            break;
        
        case SYS_PUTCHAR:
            sbi_putchar((char) a0);
            break;
        
        case SYS_SLEEP: ;
            unsigned long current_time;
            
            current_time = sbi_get_time();

            process->state = PS_SLEEPING;
            process->sleep_until = current_time + a0;

            schedule_schedule(hart);
            break;

        default:
            printf("syscall_handle: unsupported syscall code: %d\n", a7);
    }
}
