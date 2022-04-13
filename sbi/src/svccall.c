// svccall.c
// Handles calls from the OS the the SBI.
// Delegates handling based on arg7


#include <syscall.h>
#include <csr.h>
#include <uart.h>
#include <svccodes.h>
#include <hart.h>
#include <clint.h>
#include <printf.h>


void svccall_handle(int hart) {
    long *mscratch;
    unsigned long sip;


    CSR_READ(mscratch, "mscratch");
    switch (mscratch[XREG_A7]) {
        case SBI_PUTCHAR: ;
            char c = mscratch[XREG_A0] & 0xff;          // Get arg0
            uart_put(c);
            break;
    
        case SBI_GETCHAR:
            mscratch[XREG_A0] = uart_get_buffered();    // Set return value
            break;
        
        case SBI_GET_HART_STATUS: ;
            int hart_to_get = mscratch[XREG_A0];
            mscratch[XREG_A0] = get_hart_status(hart_to_get);
            break;
        
        case SBI_HART_START: ;
            int             hart_to_start = mscratch[XREG_A0];
            unsigned long   target = mscratch[XREG_A1];
            unsigned long   scratch = mscratch[XREG_A2];
            mscratch[XREG_A0] = hart_start(hart_to_start, target, scratch);
            break;
        
        case SBI_HART_STOP:
            mscratch[XREG_A0] = hart_stop(hart);
            break;
        
        case SBI_WHOAMI:
            mscratch[XREG_A0] = hart;
            break;
        
        case SBI_GET_TIME:
            mscratch[XREG_A0] = clint_get_time();
            break;
        
        case SBI_SET_TIMER: ;
            int             hart_to_set = mscratch[XREG_A0];
            unsigned long   timer_val = mscratch[XREG_A1];
            clint_set_timer(hart_to_set, timer_val);
            break;
        
        case SBI_ADD_TIMER: ;
            int             hart_to_add = mscratch[XREG_A0];
            unsigned long   timer_duration = mscratch[XREG_A1];
            unsigned long   time;
            time = clint_get_time();
            clint_set_timer(hart_to_add, time + timer_duration);
            break;

        case SBI_ACK_TIMER:
            CSR_READ(sip, "mip");
            clint_set_timer(hart, CLINT_TIME_INFINITE);
            CSR_WRITE("mip", sip & ~SIP_STIP);
            break;

        case SBI_POWEROFF:
            *((volatile unsigned short*) 0x100000) = 0x5555;
            break;

        default:
            break;
    }
}
