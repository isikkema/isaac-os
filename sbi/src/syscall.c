#include <syscall.h>
#include <csr.h>
#include <uart.h>

void syscall_handle(int hart) {
    long *mscratch;
    
    CSR_READ(mscratch, "mscratch");
    switch (mscratch[17]) {
    case 10:
        // putc
        break;
    
    case 11:
        // getc
        break;
        
    default:
        break;
    }
}