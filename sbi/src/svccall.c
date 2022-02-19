// svccall.c
// Handles calls from the OS the the SBI.
// Delegates handling based on arg7


#include <syscall.h>
#include <csr.h>
#include <uart.h>
#include <svccodes.h>
#include <hart.h>


void svccall_handle(int hart) {
    long *mscratch;
    
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
            int             priv_mode = mscratch[XREG_A2];
            mscratch[XREG_A0] = hart_start(hart_to_start, target, priv_mode);
            break;
        
        case SBI_POWEROFF:
            *((unsigned short*) 0x100000) = 0x5555;
            break;

        default:
            break;
    }
}
