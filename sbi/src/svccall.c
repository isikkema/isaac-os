#include <syscall.h>
#include <csr.h>
#include <uart.h>
#include <svccodes.h>
#include <hart.h>


void svccall_handle(int hart) {
    long *mscratch;
    
    CSR_READ(mscratch, "mscratch");
    switch (mscratch[17]) {
        case SBI_PUTCHAR: ;
            char c = mscratch[10] & 0xff;   // mscratch[10] is a0
            uart_put(c);
            break;
    
        case SBI_GETCHAR:
            mscratch[10] = uart_get_buffered();
            break;
        
        case SBI_GET_HART_STATUS: ;
            int hart = mscratch[10];
            mscratch[10] = get_hart_status(hart);
            break;
        
        case SBI_POWEROFF:
            *((unsigned short*) 0x100000) = 0x5555;
            break;

        default:
            break;
    }
}
