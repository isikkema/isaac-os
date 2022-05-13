#include <csr.h>
#include <rs_int.h>
#include <printf.h>
#include <plic.h>
#include <sbi.h>
#include <schedule.h>
#include <start.h>
#include <syscall.h>


void c_trap(void) {
    u64 scause;
    u64 sepc;
    u32 hart;
    bool is_async;
    Process* process;

    CSR_READ(scause, "scause");
    CSR_READ(sepc, "sepc");

    hart = sbi_whoami();

    is_async = MCAUSE_IS_ASYNC(scause);
    scause = MCAUSE_NUM(scause);
   
    if (is_async) {
        switch (scause) {
            case 5:
                // STIP
                sbi_ack_timer();
                schedule_schedule(hart);
                break;
                
            case 9:
                plic_handle_irq(hart);
                break;
            
            default:
                printf("error: c_trap: unhandled asynchronous interrupt: %ld\n", scause);

                process = schedule_get_process_on_hart(hart);
                schedule_remove(process);
                schedule_stop(process);
                schedule_add(process);
                schedule_schedule(hart);
        }
    } else {
        switch (scause) {
            case 8:
                // Syscall
                CSR_WRITE("sepc", sepc + 4);    // Return to next instruction

                process = schedule_get_process_on_hart(hart);
                syscall_handle(process);
                break;
                
            default:
                printf("error: c_trap: unhandled synchronous interrupt: %ld\n", scause);

                process = schedule_get_process_on_hart(hart);
                schedule_remove(process);
                schedule_stop(process);
                schedule_add(process);
                schedule_schedule(hart);
        }
    }
}
