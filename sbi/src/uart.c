#include "uart.h"

void uart_init(void) {
    volatile char* uart = (char*) 0x10000000;

    uart[3] = 0b11; // Set word length to 8

    uart[2] = 0b01;  // Enable FIFO

    // IER
    // turn on data ready interrupts IER byte 1 bit 0
}

char uart_get(void) {
    volatile char* uart = (char*) 0x10000000;

    if (uart[5] & 0b1) {    // Check if data is ready
        return uart[0];     // Return available char
    }

    return 0xff;
}

void uart_put(char val) {
    volatile char* uart = (char*) 0x10000000;
    
    while (!(uart[5] & (1 << 6))) {}

    uart[0] = val;
}
