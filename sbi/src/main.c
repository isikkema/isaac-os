#include <start.h>
#include <uart.h>
#include <printf.h>
#include <plic.h>


int main(int hartid) {
    while (hartid != 0) {
        // sleep
    }
    
    uart_init();
    plic_init();

    return 0;
}
