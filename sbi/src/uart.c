#include "uart.h"

#define UART_BASE ((volatile char*) 0x10000000)
#define UART_RW     (0)
#define UART_IER    (1)
#define UART_FCR    (2)
#define UART_LCR    (3)
#define UART_LSR    (5)

void uart_init(void) {
    UART_BASE[UART_LCR] = 0b11; // Set word length to 8

    UART_BASE[UART_FCR] = 0b01; // Enable FIFO

    UART_BASE[UART_IER] = 0b1;  // Turn on data ready interrupts

    // Divisor
}

char uart_get(void) {
    if (UART_BASE[UART_LSR] & 0b1) {    // Check if data is ready
        return UART_BASE[0];            // Return available char
    }

    return 0xff;                        // Return 255 if no data is ready
}

void uart_put(char val) {    
    while (!(UART_BASE[UART_LSR] & 0b1000000)) {    // While buffer is not empty
        // sleep
    }

    UART_BASE[0] = val;                             // Write data when buffer is ready
}

void uart_handle_irq(void) {
    char c = uart_get();
    uart_put(c);
}
