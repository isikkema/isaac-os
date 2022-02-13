#include <uart.h>
#include <lock.h>

#define UART_BASE ((volatile char*) 0x10000000)
#define UART_RW     (0)
#define UART_IER    (1)
#define UART_FCR    (2)
#define UART_LCR    (3)
#define UART_LSR    (5)

#define UART_BUFFER_SIZE (32)


Mutex mutex;

char uart_buffer[UART_BUFFER_SIZE];
int uart_buf_idx = 0;
int uart_buf_len = 0;


void uart_init(void) {
    UART_BASE[UART_LCR] = 0b11; // Set word length to 8

    UART_BASE[UART_FCR] = 0b01; // Enable FIFO

    UART_BASE[UART_IER] = 0b1;  // Turn on data ready interrupts

    // Divisor
    UART_BASE[UART_LCR] |= 1 << 7;      // Set DLAB
    UART_BASE[0] = 80;                  // Set Divisor
    UART_BASE[1] = 2;                   // ...
    UART_BASE[UART_LCR] &= ~(1 << 7);   // Unset DLAB
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

char uart_get_buffered(void) {
    char rv;

    mutex_sbi_lock(&mutex);

    if (uart_buf_len > 0) {                 // If buffer isn't empty,
        rv = uart_buffer[uart_buf_idx];     // return char at idx, increase idx and decrease len
        
        uart_buf_idx++;
        uart_buf_idx %= UART_BUFFER_SIZE;

        uart_buf_len--;
    } else {
        rv = 0xff;
    }

    mutex_unlock(&mutex);

    return rv;
}

void uart_buffer_write(char c) {
    int new_idx;

    mutex_sbi_lock(&mutex);

    if (uart_buf_len < UART_BUFFER_SIZE) {  // If buffer isn't full,
        uart_buf_len++;                     // increase len
    } else {
        uart_buf_idx++;                     // Otherwise, overwrite the first char in buffer
        uart_buf_idx %= UART_BUFFER_SIZE;
    }

    new_idx = (uart_buf_idx + uart_buf_len - 1) % UART_BUFFER_SIZE;
    uart_buffer[new_idx] = c;

    mutex_unlock(&mutex);
}

void uart_handle_irq(void) {
    char c;
    
    while (1) {                 // Write as many chars to the buffer as are available
        c = uart_get();
        if (c == 0xff) {
            break;
        }

        uart_buffer_write(c);
    }
}
