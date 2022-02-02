#include <start.h>
#include <uart.h>
#include <printf.h>
#include <plic.h>


void plic_init() {
    plic_enable(0, 10); // Enable UART on hart 0

    plic_set_priority(10, 7);   // Set UART to pri 7

    plic_set_threshold(0, 0);   // Set hart 0 to threshold 0
}

int main(int hartid) {
    while (hartid != 0) {
        // sleep
    }
    
    uart_init();
    plic_init();

    return 0;
}
