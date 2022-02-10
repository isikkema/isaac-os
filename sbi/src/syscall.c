#include <syscall.h>
#include <csr.h>
#include <uart.h>


void syscall_handle(int hart) {
    long *mscratch;
    
    CSR_READ(mscratch, "mscratch");
    switch (mscratch[17]) {
        case 10: ;
            char c = mscratch[10] & 0xff;   // mscratch[10] is a0
            uart_put(c);
            break;
    
        case 11:
            mscratch[10] = uart_get_buffered();
            break;
            
        default:
            break;
    }
}
