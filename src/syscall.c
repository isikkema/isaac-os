#include <syscall.h>
#include <csr.h>
#include <sbi.h>
#include <process.h>
#include <schedule.h>
#include <printf.h>


void syscall_handle(uint64_t* regs) {
    int hart;
    Process* process;

    printf("syscall_handle: regs: 0x%08lx\n", (uint64_t) regs);
    printf("syscall_handle: code: %d\n", regs[XREG_A7]);

    hart = sbi_whoami();
    printf("syscall_handle: hart: %d\n", hart);

    switch (regs[XREG_A7]) {
        case SYS_EXIT:
            printf("syscall_handle: exiting...\n");

            process = schedule_get_process_on_hart(hart);
            
            schedule_park(hart);
            printf("syscall_handle: remove: %d\n", schedule_remove(process));
            // printf("syscall_handle: stop: %d\n", schedule_stop(process));
            
            process_free(process);

            schedule_schedule(hart);
            break;
        
        default:
            printf("syscall_handle: unsupported syscall code: %d\n", regs[XREG_A7]);
    }
}
